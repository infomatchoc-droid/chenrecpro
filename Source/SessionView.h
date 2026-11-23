#pragma once

// =============================================================================
//  Vue Session, façon Ableton Live :
//  une colonne par piste, une ligne par scène, des clips à lancer,
//  lancement calé sur la mesure, warp des clips audio.
// =============================================================================

namespace chen
{

namespace Launcher
{
    inline String quantName (te::LaunchQType t)
    {
        using Q = te::LaunchQType;
        switch (t)
        {
            case Q::none:      return String::fromUTF8 ("immédiat");
            case Q::eightBars: return "8 mesures";
            case Q::fourBars:  return "4 mesures";
            case Q::twoBars:   return "2 mesures";
            case Q::bar:       return "1 mesure";
            case Q::half:      return "1/2";
            case Q::quarter:   return "1/4";
            case Q::eighth:    return "1/8";
            case Q::sixteenth: return "1/16";
            default:           return te::getName (t);
        }
    }

    inline te::MonotonicBeat launchPosition (te::Edit& e)
    {
        auto epc = e.getTransport().getCurrentPlaybackContext();
        if (epc == nullptr) return {};
        auto sp = epc->getSyncPoint();
        if (! sp) return {};

        auto& lq = e.getLaunchQuantisation();
        auto pos = sp->beat;
        auto q = pos;
        for (int guard = 0; guard < 64; ++guard)
        {
            q = lq.getNext (q);
            if (q >= te::BeatPosition()) break;
            q = q + te::BeatDuration::fromBeats (0.001);
        }
        return { sp->monotonicBeat.v + (q - pos) };
    }

    inline void ensureGrid (te::Edit& e, int scenes)
    {
        e.getSceneList().ensureNumberOfScenes (scenes);
        for (auto* t : te::getAudioTracks (e))
            t->getClipSlotList().ensureNumberOfSlots (e.getSceneList().getNumScenes());
    }

    inline bool isActive (te::Clip& c)
    {
        if (auto lh = c.getLaunchHandle())
            return lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing
                || lh->getQueuedStatus() == te::LaunchHandle::QueueState::playQueued;
        return false;
    }

    inline void startTransportIfNeeded (te::Edit& e)
    {
        auto& tc = e.getTransport();
        if (tc.isPlaying()) return;
        auto bb = e.tempoSequence.toBarsAndBeats (tc.getPosition());
        tc.setPosition (e.tempoSequence.toTime (te::tempo::BarsAndBeats { bb.bars, {} }));
        if (auto epc = tc.getCurrentPlaybackContext()) epc->blockUntilSyncPointChange();
        tc.play (false);
    }

    /** Lance un clip ; les autres clips de la piste s'arrêtent au même moment. */
    inline void launch (te::Clip& c)
    {
        auto& e = c.edit;
        bool wasPlaying = e.getTransport().isPlaying();
        startTransportIfNeeded (e);
        auto when = wasPlaying ? std::optional (launchPosition (e)) : std::nullopt;

        if (auto* at = dynamic_cast<te::AudioTrack*> (c.getTrack()))
            for (auto* slot : at->getClipSlotList().getClipSlots())
                if (auto* other = slot->getClip(); other != nullptr && other != &c)
                    if (auto lh = other->getLaunchHandle())
                        lh->stop (when);

        if (auto lh = c.getLaunchHandle())
            lh->play (when);
    }

    inline void stopTrack (te::AudioTrack& t)
    {
        auto& e = t.edit;
        auto when = e.getTransport().isPlaying() ? std::optional (launchPosition (e)) : std::nullopt;
        for (auto* slot : t.getClipSlotList().getClipSlots())
            if (auto* c = slot->getClip())
                if (auto lh = c->getLaunchHandle())
                    lh->stop (when);
    }

    inline void launchScene (te::Edit& e, int scene)
    {
        startTransportIfNeeded (e);
        for (auto* t : te::getAudioTracks (e))
        {
            auto slots = t->getClipSlotList().getClipSlots();
            if (scene < slots.size())
            {
                if (auto* c = slots[scene]->getClip()) launch (*c);
                else                                   stopTrack (*t);
            }
        }
    }

    inline void stopAll (te::Edit& e)
    {
        for (auto* t : te::getAudioTracks (e)) stopTrack (*t);
    }

    /** Retour à l'arrangement (comme le bouton d'Ableton). */
    inline void backToArrangement (te::Edit& e)
    {
        stopAll (e);
        for (auto* t : te::getAudioTracks (e)) t->playSlotClips = false;
    }
}

//==============================================================================
/** Enregistrement dans une case de la Session (façon Ableton) :
    l'entrée de la piste est envoyée dans la case, le reste des pistes n'enregistre pas,
    et à l'arrêt la prise est calée sur des mesures entières puis lancée en boucle. */
class SlotRecorder : private te::TransportControl::Listener,
                     private Timer
{
public:
    explicit SlotRecorder (te::Edit& e) : edit (e) { edit.getTransport().addListener (this); }
    ~SlotRecorder() override
    {
        edit.getTransport().removeListener (this);
        if (state != State::idle) restore();
    }

    bool isRecordingInto (const te::ClipSlot& s) const { return state != State::idle && slotID == s.itemID; }
    bool isBusy() const { return state != State::idle; }
    bool isStopping() const { return state == State::stopping; }

    /** Temps d'enregistrement écoulé (pour l'affichage). */
    double elapsedSeconds() const
    {
        if (state != State::recording || ! gotStart) return 0.0;
        return jmax (0.0, edit.getTransport().getPosition().inSeconds() - recStart.inSeconds());
    }

    std::function<void (const String&)> onProblem;

    void start (te::AudioTrack& t, te::ClipSlot& slot)
    {
        auto& tc = edit.getTransport();
        if (state != State::idle) return;
        if (tc.isRecording())
        {
            if (onProblem) onProblem (String::fromUTF8 ("Un enregistrement est déjà en cours dans l'arrangement."));
            return;
        }

        Array<te::InputDeviceInstance*> ours;
        for (auto* in : edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, 0)) ours.add (in);
        if (ours.isEmpty())
        {
            if (onProblem) onProblem (String::fromUTF8 ("La piste « ") + t.getName()
                                      + String::fromUTF8 (" » n'a pas d'entrée : choisis-en une dans l'inspecteur ou l'en-tête de piste."));
            return;
        }

        // les autres destinations armées n'enregistrent pas pendant la prise en Session
        disabled.clear();
        monitors.clear();
        for (auto* in : edit.getAllInputDevices())
            for (auto id : in->getTargets())
                if (in->isRecordingEnabled (id))
                {
                    bool mine = ours.contains (in) && id == t.itemID;
                    if (mine && in->getInputDevice().getMonitorMode() == te::InputDevice::MonitorMode::automatic)
                    {
                        // on continue d'entendre l'entrée alors que la piste elle-même n'enregistre plus
                        monitors.push_back ({ &in->getInputDevice(), te::InputDevice::MonitorMode::automatic });
                        in->getInputDevice().setMonitorMode (te::InputDevice::MonitorMode::on);
                    }
                    in->setRecordingEnabled (id, false);
                    disabled.push_back ({ in, id });
                }

        slotID = slot.itemID;
        track = &t;
        instances.clear();
        for (auto* in : ours)
        {
            auto dest = in->setTarget (slotID, false, nullptr);
            if (dest) { (*dest)->recordEnabled = true; instances.push_back (in); }
        }
        if (instances.empty())
        {
            restore();
            if (onProblem) onProblem (String::fromUTF8 ("Impossible d'enregistrer dans cette case."));
            return;
        }
        edit.dispatchPendingUpdatesSynchronously();

        if (! tc.isPlaying())
        {
            // départ calé sur le début de la mesure en cours
            auto bb = edit.tempoSequence.toBarsAndBeats (tc.getPosition());
            tc.setPosition (edit.tempoSequence.toTime (te::tempo::BarsAndBeats { bb.bars, {} }));
        }
        gotStart = false;
        recStart = tc.getPosition();
        state = State::recording;
        tc.record (false);
    }

    /** Arrête la prise (la lecture continue) ; la prise sera lancée en boucle. */
    void stop()
    {
        if (state != State::recording) return;
        state = State::stopping;
        stopAskedMs = Time::getMillisecondCounter();
        auto& tc = edit.getTransport();
        if (tc.isRecording()) tc.stopRecording (false);
        startTimer (250);
    }

private:
    enum class State { idle, recording, stopping };

    te::Edit& edit;
    State state = State::idle;
    te::EditItemID slotID;
    te::AudioTrack::Ptr track;
    std::vector<WeakReference<te::InputDeviceInstance>> instances;
    std::vector<std::pair<WeakReference<te::InputDeviceInstance>, te::EditItemID>> disabled;
    std::vector<std::pair<te::InputDevice*, te::InputDevice::MonitorMode>> monitors;
    te::TimePosition recStart;
    bool gotStart = false;
    uint32 stopAskedMs = 0;
    int takeCount = 0;

    void recordingStarted (te::SyncPoint start, std::optional<te::TimeRange>) override
    {
        if (state == State::recording) { recStart = start.time; gotStart = true; }
    }

    void recordingStopped (te::SyncPoint, bool) override
    {
        if (state == State::recording)   // arrêt par le transport (Stop / Espace)
        {
            state = State::stopping;
            stopAskedMs = Time::getMillisecondCounter();
            startTimer (250);
        }
    }

    void recordingFinished (te::InputDeviceInstance&, te::EditItemID targetID,
                            const ReferenceCountedArray<te::Clip>& clips) override
    {
        if (state == State::idle || targetID != slotID) return;
        ReferenceCountedArray<te::Clip> keep (clips);
        MessageManager::callAsync ([this, keep, alive = WeakReference<SlotRecorder> (this)]
        {
            if (alive == nullptr) return;
            for (auto* c : keep) setUpLoop (*c);
            if (! keep.isEmpty() && edit.getTransport().isPlaying())
                Launcher::launch (*keep.getFirst());
            restore();
        });
    }

    void timerCallback() override
    {
        // aucune prise reçue (silence, entrée débranchée…) : on remet tout en place
        if (state == State::stopping && Time::getMillisecondCounter() - stopAskedMs > 4000) restore();
        if (state == State::idle) stopTimer();
    }

    /** Boucle sur des mesures entières, à partir de la première barre de mesure de la prise. */
    void setUpLoop (te::Clip& c)
    {
        auto& ts = edit.tempoSequence;
        const double bpm = ts.getTempoAt (recStart).getBpm();
        auto& sig = ts.getTimeSigAt (recStart);
        const double beatsPerBar = sig.numerator.get() * 4.0 / jmax (1, sig.denominator.get());
        const double barSecs = beatsPerBar * 60.0 / bpm;

        auto bb = ts.toBarsAndBeats (recStart);
        auto barTime = ts.toTime (te::tempo::BarsAndBeats { bb.bars, {} });
        if (recStart.inSeconds() - barTime.inSeconds() > 0.05)
            barTime = ts.toTime (te::tempo::BarsAndBeats { bb.bars + 1, {} });
        const double offset = jmax (0.0, barTime.inSeconds() - recStart.inSeconds());
        const double len = c.getPosition().getLength().inSeconds();
        const int bars = jmax (1, (int) std::lround ((len - offset) / barSecs));
        const auto offsetBeats = te::BeatPosition::fromBeats (offset * bpm / 60.0);
        const te::BeatRange loop { offsetBeats, te::BeatDuration::fromBeats (bars * beatsPerBar) };

        c.setName ("Prise " + String (++takeCount));
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (&c))
        {
            wc->setUsesProxy (false);
            wc->getLoopInfo().setBpm (bpm, wc->getAudioFile().getInfo());
            wc->setAutoTempo (true);
            wc->setLoopRangeBeats (loop);
        }
        else if (auto* mc = dynamic_cast<te::MidiClip*> (&c))
        {
            mc->setLoopRangeBeats (loop);
        }
    }

    void restore()
    {
        stopTimer();
        for (auto& in : instances)
            if (in != nullptr) (void) in->removeTarget (slotID, nullptr);
        instances.clear();
        for (auto& [in, id] : disabled)
            if (in != nullptr) in->setRecordingEnabled (id, true);
        disabled.clear();
        for (auto& [dev, mode] : monitors) dev->setMonitorMode (mode);
        monitors.clear();
        state = State::idle;
        track = nullptr;
    }

    JUCE_DECLARE_WEAK_REFERENCEABLE (SlotRecorder)
};

//==============================================================================
/** « Arrangement REC » (façon Ableton) : tout clip lancé depuis la Session est
    recopié dans l'arrangement, à l'endroit et pour la durée où il a réellement joué. */
class ArrangementRecorder : private Timer
{
public:
    explicit ArrangementRecorder (te::Edit& e) : edit (e) {}
    ~ArrangementRecorder() override { stopTimer(); }

    bool isArmed() const { return armed; }
    int getNumWritten() const { return written; }

    void setArmed (bool shouldArm)
    {
        if (shouldArm == armed) return;
        if (! shouldArm)
        {
            flush (true);
            armed = false;
            stopTimer();
            return;
        }
        armed = true;
        written = 0;
        active.clear();
        auto& tc = edit.getTransport();
        if (! tc.isPlaying()) tc.play (false);
        seenPlaying = false;
        startTimerHz (30);
    }

private:
    struct Active { te::AudioTrack::Ptr track; te::Clip::Ptr clip; };

    te::Edit& edit;
    bool armed = false, seenPlaying = false;
    int written = 0;
    std::map<te::EditItemID, Active> active;

    static te::Clip* playingClip (te::AudioTrack& t)
    {
        for (auto* slot : t.getClipSlotList().getClipSlots())
            if (auto* c = slot->getClip())
                if (auto lh = c->getLaunchHandle())
                    if (lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing)
                        return c;
        return nullptr;
    }

    void timerCallback() override
    {
        auto& tc = edit.getTransport();
        if (tc.isPlaying()) seenPlaying = true;

        for (auto* t : te::getAudioTracks (edit))
        {
            auto* now = playingClip (*t);
            auto it = active.find (t->itemID);
            if (it != active.end() && it->second.clip.get() != now)
            {
                commit (it->second, false);
                active.erase (it);
            }
            if (now != nullptr && active.find (t->itemID) == active.end())
                active[t->itemID] = { t, now };
        }

        // le transport s'arrête : on écrit ce qui jouait et on désarme (comme Ableton)
        if (seenPlaying && ! tc.isPlaying())
        {
            flush (false);
            armed = false;
            stopTimer();
        }
    }

    void flush (bool stillPlaying)
    {
        for (auto& [id, a] : active) commit (a, stillPlaying);
        active.clear();
    }

    void commit (const Active& a, bool stillPlaying)
    {
        if (a.track == nullptr || a.clip == nullptr) return;
        auto lh = a.clip->getLaunchHandle();
        if (lh == nullptr) return;

        auto range = (stillPlaying || lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing)
                        ? lh->getPlayedRange() : lh->getLastPlayedRange();
        if (! range || range->getLength() <= te::BeatDuration::fromBeats (0.01)) return;

        auto& ts = edit.tempoSequence;
        auto start = ts.toTime (range->getStart());
        auto end = ts.toTime (range->getEnd());

        // un clip qui ne boucle pas s'arrête à sa fin
        if (! a.clip->isLooping())
            end = jmin (end, start + a.clip->getPosition().getLength());
        if (end <= start) return;

        a.track->deleteRegion ({ start, end }, nullptr);
        auto copy = a.clip->state.createCopy();
        edit.createNewItemID().writeID (copy, nullptr);
        if (auto* nc = a.track->insertClipWithState (copy))
        {
            nc->setPosition (te::ClipPosition { { start, end }, a.clip->getPosition().getOffset() });
            ++written;
        }
    }
};

//==============================================================================
class SessionView : public Component,
                    public FileDragAndDropTarget,
                    private Timer
{
public:
    explicit SessionView (te::Edit& e) : edit (e), recorder (e), arrRec (e)
    {
        Launcher::ensureGrid (edit, 8);
        recorder.onProblem = [] (const String& msg)
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, String::fromUTF8 ("Enregistrement en Session"), msg);
        };
        startTimerHz (20);
    }

    std::function<void (te::MidiClip*)> onOpenMidiEditor;
    std::function<void (te::AudioTrack*)> onSelectTrack;

    void paint (Graphics& g) override
    {
        g.fillAll (Col::bg);
        auto tracks = te::getAudioTracks (edit);
        auto scenes = edit.getSceneList().getScenes();
        Launcher::ensureGrid (edit, jmax (8, scenes.size()));

        // en-têtes de colonnes
        for (int i = 0; i < tracks.size(); ++i)
        {
            auto* t = tracks[i];
            auto r = headerRect (i).toFloat();
            g.setColour (t->getColour());
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Colour (0xff141414));
            g.setFont (uiFont (12.5f, true));
            g.drawText (t->getName(), r.reduced (6, 0).withTrimmedRight (24), Justification::centredLeft, true);

            // armement de la piste (●)
            auto ar = armRect (i).toFloat();
            bool armed = EngineHelpers::isTrackArmed (*t);
            g.setColour (Colour (0xff141414).withAlpha (armed ? 0.9f : 0.35f));
            g.fillEllipse (ar);
            g.setColour (armed ? Col::rec : Colour (0xff141414).withAlpha (0.6f));
            g.fillEllipse (ar.reduced (4.0f));
        }

        // cases
        for (int ti = 0; ti < tracks.size(); ++ti)
        {
            auto slots = tracks[ti]->getClipSlotList().getClipSlots();
            for (int si = 0; si < scenes.size(); ++si)
            {
                auto r = cellRect (ti, si).toFloat();
                auto* c = si < slots.size() ? slots[si]->getClip() : nullptr;
                bool blink = (Time::getMillisecondCounter() / 250) % 2 == 0;

                if (si < slots.size() && recorder.isRecordingInto (*slots[si]))
                {
                    g.setColour (Col::rec.withAlpha (recorder.isStopping() ? 0.45f : (blink ? 0.95f : 0.7f)));
                    g.fillRoundedRectangle (r, 3.0f);
                    g.setColour (Colours::white);
                    g.fillEllipse (r.getX() + 8, r.getCentreY() - 5, 10.0f, 10.0f);
                    g.setFont (uiFont (12.0f, true));
                    auto secs = recorder.elapsedSeconds();
                    auto barSecs = 60.0 / edit.tempoSequence.getTempoAt (edit.getTransport().getPosition()).getBpm()
                                   * edit.tempoSequence.getTimeSigAt (edit.getTransport().getPosition()).numerator.get();
                    g.drawText (recorder.isStopping() ? String::fromUTF8 ("Finalisation…")
                                                      : String::fromUTF8 ("Enregistrement  ") + String (secs / barSecs, 1) + " mes.",
                                r.withTrimmedLeft (24).reduced (4, 0), Justification::centredLeft, true);
                    continue;
                }

                if (c == nullptr)
                {
                    g.setColour (Colour (0xff1d1e22));
                    g.fillRoundedRectangle (r, 3.0f);
                    if (EngineHelpers::isTrackArmed (*tracks[ti]) && ! recorder.isBusy())
                    {
                        g.setColour (Col::rec.withAlpha (0.85f));                     // ● enregistrer ici
                        g.fillEllipse (r.getX() + 8, r.getCentreY() - 5, 10.0f, 10.0f);
                    }
                    else
                    {
                        g.setColour (Colour (0xff35373d));
                        g.fillRect (r.getX() + 8, r.getCentreY() - 3, 6.0f, 6.0f);   // ■ arrêt
                    }
                    continue;
                }

                auto col = tracks[ti]->getColour();
                auto lh = c->getLaunchHandle();
                bool playing = lh && lh->getPlayingStatus() == te::LaunchHandle::PlayState::playing;
                bool queued  = lh && lh->getQueuedStatus() == te::LaunchHandle::QueueState::playQueued;

                g.setColour (queued && blink ? col.brighter (0.4f) : col);
                g.fillRoundedRectangle (r, 3.0f);
                if (playing)
                {
                    g.setColour (Colours::white);
                    g.drawRoundedRectangle (r, 3.0f, 2.0f);
                }

                Path tri;
                tri.addTriangle (r.getX() + 8, r.getCentreY() - 6, r.getX() + 8, r.getCentreY() + 6, r.getX() + 17, r.getCentreY());
                g.setColour (Colour (0xff141414));
                g.fillPath (tri);
                g.setFont (uiFont (12.0f, true));
                g.drawText (c->getName(), r.withTrimmedLeft (22).reduced (4, 0), Justification::centredLeft, true);

                // progression de lecture
                if (playing)
                    if (auto played = lh->getPlayedRange())
                    {
                        auto len = jmax (0.001, clipBeats (*c).inBeats());
                        auto prog = std::fmod (played->getLength().inBeats(), len) / len;
                        g.setColour (Colours::black.withAlpha (0.35f));
                        g.fillRect (r.getX() + 4, r.getBottom() - 6, (r.getWidth() - 8), 3.0f);
                        g.setColour (Colour (0xff141414));
                        g.fillRect (r.getX() + 4, r.getBottom() - 6, (float) ((r.getWidth() - 8) * prog), 3.0f);
                    }
            }

            // bouton stop de la piste
            auto sr = stopRect (ti).toFloat();
            g.setColour (Colour (0xff1b1e23));
            g.fillRoundedRectangle (sr, 3.0f);
            g.setColour (Col::text2);
            g.fillRect (sr.getCentreX() - 4, sr.getCentreY() - 4, 8.0f, 8.0f);
        }

        // colonne des scènes
        auto sc = scenesColumn().toFloat();
        g.setColour (Col::panel1);
        g.fillRect (sc);
        g.setColour (Col::text2);
        g.setFont (uiFont (11.0f, true));
        g.drawText (String::fromUTF8 ("SCÈNES"), headerRect (tracks.size()).withX ((int) sc.getX() + 10), Justification::centredLeft);
        for (int si = 0; si < scenes.size(); ++si)
        {
            auto r = sceneRect (si).toFloat();
            g.setColour (Col::panel3);
            g.fillRoundedRectangle (r, 3.0f);
            Path tri;
            tri.addTriangle (r.getX() + 8, r.getCentreY() - 6, r.getX() + 8, r.getCentreY() + 6, r.getX() + 17, r.getCentreY());
            g.setColour (Col::accent);
            g.fillPath (tri);
            g.setColour (Col::text);
            g.setFont (uiFont (12.5f, true));
            auto name = scenes[si]->name.get();
            g.drawText (name.isNotEmpty() ? name : String::fromUTF8 ("Scène ") + String (si + 1), r.withTrimmedLeft (24), Justification::centredLeft, true);
        }

        auto drawButton = [&] (Rectangle<int> r, const String& txt, Colour c)
        {
            g.setColour (Col::panel3);
            g.fillRoundedRectangle (r.toFloat(), 3.0f);
            g.setColour (c);
            g.setFont (uiFont (11.5f, true));
            g.drawText (txt, r, Justification::centred);
        };
        drawButton (stopAllRect(), String::fromUTF8 ("■  Tout arrêter"), Col::text);
        drawButton (addSceneRect(), String::fromUTF8 ("+  Scène"), Col::text2);
        drawButton (backRect(), String::fromUTF8 ("↺  Retour à l'arrangement"), Col::accent);
        drawButton (quantRect(), "Lancement : " + Launcher::quantName (edit.getLaunchQuantisation().type.get()), Col::text);

        // ● Arrangement : enregistre le jeu de la Session dans la timeline
        {
            auto r = arrRecRect();
            bool on = arrRec.isArmed();
            bool blink = (Time::getMillisecondCounter() / 400) % 2 == 0;
            g.setColour (on ? Col::rec.withAlpha (blink ? 0.9f : 0.65f) : Col::panel3);
            g.fillRoundedRectangle (r.toFloat(), 3.0f);
            g.setColour (on ? Colours::white : Col::rec);
            g.fillEllipse ((float) r.getX() + 12.0f, (float) r.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour (on ? Colours::white : Col::text);
            g.setFont (uiFont (11.5f, true));
            g.drawText (on ? String::fromUTF8 ("Enregistre dans l'arrangement…") : String::fromUTF8 ("Arrangement"),
                        r.withTrimmedLeft (30), Justification::centredLeft);
        }

        if (tracks.isEmpty())
        {
            g.setColour (Col::text3);
            g.setFont (uiFont (15.0f));
            g.drawText (String::fromUTF8 ("Crée des pistes pour remplir la Session"), getLocalBounds(), Justification::centred);
        }

        g.setColour (Col::text3);
        g.setFont (uiFont (11.0f));
        g.drawText (String::fromUTF8 ("Clic : lancer  ·  ● sur une piste armée : enregistrer dans la case (re-clic : boucler)  ·  case vide : arrêter la piste  ·  double-clic sur une piste instrument : nouveau clip MIDI  ·  glisser un fichier audio : clip calé au tempo  ·  clic droit : options  ·  Tab : Édition"),
                    10, getHeight() - 24, getWidth() - 20, 20, Justification::centredLeft);
    }

    void mouseDown (const MouseEvent& e) override
    {
        auto p = e.getPosition();
        auto tracks = te::getAudioTracks (edit);
        auto scenes = edit.getSceneList().getScenes();

        if (stopAllRect().contains (p))  { recorder.stop(); Launcher::stopAll (edit); return; }
        if (backRect().contains (p))     { Launcher::backToArrangement (edit); return; }
        if (addSceneRect().contains (p)) { Launcher::ensureGrid (edit, scenes.size() + 1); repaint(); return; }
        if (quantRect().contains (p))    { showQuantMenu(); return; }
        if (arrRecRect().contains (p))   { arrRec.setArmed (! arrRec.isArmed()); repaint(); return; }

        for (int si = 0; si < scenes.size(); ++si)
            if (sceneRect (si).contains (p))
            {
                if (e.mods.isPopupMenu()) renameScene (*scenes[si]);
                else                      Launcher::launchScene (edit, si);
                return;
            }

        for (int ti = 0; ti < tracks.size(); ++ti)
        {
            if (stopRect (ti).contains (p)) { Launcher::stopTrack (*tracks[ti]); return; }
            if (armRect (ti).contains (p))
            {
                bool hasInput = false;
                for (auto* in : edit.getAllInputDevices()) hasInput = hasInput || te::isOnTargetTrack (*in, *tracks[ti], 0);
                if (! hasInput && recorder.onProblem)
                    recorder.onProblem (String::fromUTF8 ("La piste « ") + tracks[ti]->getName()
                                        + String::fromUTF8 (" » n'a pas d'entrée : choisis-en une dans l'inspecteur ou l'en-tête de piste (Tab pour revenir à l'Édition)."));
                EngineHelpers::armTrack (*tracks[ti], ! EngineHelpers::isTrackArmed (*tracks[ti]));
                repaint();
                return;
            }
            if (headerRect (ti).contains (p)) { if (onSelectTrack) onSelectTrack (tracks[ti]); return; }

            auto slots = tracks[ti]->getClipSlotList().getClipSlots();
            for (int si = 0; si < scenes.size() && si < slots.size(); ++si)
            {
                if (! cellRect (ti, si).contains (p)) continue;
                if (onSelectTrack) onSelectTrack (tracks[ti]);
                auto* c = slots[si]->getClip();

                if (recorder.isRecordingInto (*slots[si])) { recorder.stop(); return; }   // re-clic : fin de la prise
                if (c == nullptr && ! e.mods.isPopupMenu() && e.getNumberOfClicks() < 2
                    && EngineHelpers::isTrackArmed (*tracks[ti]) && ! recorder.isBusy())
                {
                    recorder.start (*tracks[ti], *slots[si]);
                    return;
                }

                if (c != nullptr && e.mods.isPopupMenu()) { showClipMenu (*c); return; }
                if (c != nullptr)                         { Launcher::launch (*c); return; }
                if (e.getNumberOfClicks() < 2)            { Launcher::stopTrack (*tracks[ti]); return; }

                // double-clic dans une case vide d'une piste instrument : nouveau clip MIDI de 4 mesures
                if (EngineHelpers::isInstrumentTrack (*tracks[ti]))
                {
                    auto len = edit.tempoSequence.toTime (te::BeatPosition::fromBeats (16.0));
                    if (auto mc = te::insertMIDIClip (*slots[si], "MIDI", { te::TimePosition(), len }))
                        if (onOpenMidiEditor) onOpenMidiEditor (mc.get());
                }
                return;
            }
        }
    }


    bool isInterestedInFileDrag (const StringArray& files) override
    {
        for (auto& f : files)
            if (edit.engine.getAudioFileFormatManager().readFormatManager.findFormatForFileExtension (File (f).getFileExtension()) != nullptr)
                return true;
        return false;
    }

    void filesDropped (const StringArray& files, int x, int y) override
    {
        auto tracks = te::getAudioTracks (edit);
        auto scenes = edit.getSceneList().getScenes();
        for (int ti = 0; ti < tracks.size(); ++ti)
            for (int si = 0; si < scenes.size(); ++si)
                if (cellRect (ti, si).contains (x, y))
                {
                    auto slots = tracks[ti]->getClipSlotList().getClipSlots();
                    int s = si;
                    for (auto& f : files)
                    {
                        if (s >= slots.size()) break;
                        addAudioToSlot (*slots[s++], File (f));
                    }
                    repaint();
                    return;
                }
    }

    /** Clip audio calé au tempo (warp) et bouclé, comme dans Ableton. */
    void addAudioToSlot (te::ClipSlot& slot, const File& f)
    {
        te::AudioFile af (edit.engine, f);
        if (! af.isValid()) return;
        if (auto* old = slot.getClip()) old->removeFromParent();

        auto len = te::TimeDuration::fromSeconds (af.getLength());
        if (auto wc = te::insertWaveClip (slot, f.getFileNameWithoutExtension(), f, { { te::TimePosition(), len }, {} }, te::DeleteExistingClips::yes))
        {
            wc->setAutoTempo (true);
            wc->setUsesProxy (false);
            auto beats = clipBeats (*wc);
            if (beats > te::BeatDuration())
                wc->setLoopRangeBeats ({ te::BeatPosition(), te::BeatPosition() + beats });
        }
    }

private:
    te::Edit& edit;
    SlotRecorder recorder;
    ArrangementRecorder arrRec;

    te::BeatDuration clipBeats (te::Clip& c) const
    {
        auto pos = c.getPosition();
        return edit.tempoSequence.toBeats (pos.getEnd()) - edit.tempoSequence.toBeats (pos.getStart());
    }

    static constexpr int colW = 150, cellH = 38, headerH = 30, gap = 4, scenesW = 210, top = 52;

    Rectangle<int> headerRect (int ti) const        { return { 10 + ti * (colW + gap), top, colW, headerH }; }
    Rectangle<int> armRect (int ti) const           { auto h = headerRect (ti); return { h.getRight() - 24, h.getCentreY() - 9, 18, 18 }; }
    Rectangle<int> cellRect (int ti, int si) const  { return { 10 + ti * (colW + gap), top + headerH + gap + si * (cellH + gap), colW, cellH }; }
    Rectangle<int> stopRect (int ti) const          { return cellRect (ti, edit.getSceneList().getNumScenes()).withHeight (26); }
    Rectangle<int> scenesColumn() const             { return { getWidth() - scenesW, 0, scenesW, getHeight() - 28 }; }
    Rectangle<int> sceneRect (int si) const         { auto c = cellRect (0, si); return { getWidth() - scenesW + 10, c.getY(), scenesW - 20, cellH }; }
    Rectangle<int> stopAllRect() const              { auto r = sceneRect (edit.getSceneList().getNumScenes()); return r.withHeight (26); }
    Rectangle<int> addSceneRect() const             { return stopAllRect().translated (0, 32); }
    Rectangle<int> backRect() const                 { return { 10, 12, 230, 28 }; }
    Rectangle<int> quantRect() const                { return { 250, 12, 200, 28 }; }
    Rectangle<int> arrRecRect() const               { return { 460, 12, 240, 28 }; }

    void timerCallback() override { repaint(); }

    void showQuantMenu()
    {
        PopupMenu m;
        m.addSectionHeader ("QUANTIFICATION DU LANCEMENT");
        using Q = te::LaunchQType;
        const Q types[] = { Q::none, Q::eightBars, Q::fourBars, Q::twoBars, Q::bar, Q::half, Q::quarter, Q::eighth, Q::sixteenth };
        for (int i = 0; i < (int) std::size (types); ++i)
            m.addItem (i + 1, Launcher::quantName (types[i]), true, edit.getLaunchQuantisation().type.get() == types[i]);
        m.showMenuAsync (PopupMenu::Options(), [this, types] (int r)
        {
            if (r > 0) edit.getLaunchQuantisation().type = types[r - 1];
        });
    }

    void showClipMenu (te::Clip& c)
    {
        PopupMenu m;
        m.addItem (1, "Lancer");
        m.addItem (2, "Renommer…");
        if (dynamic_cast<te::MidiClip*> (&c) != nullptr) m.addItem (3, String::fromUTF8 ("Ouvrir dans l'éditeur clavier"));
        m.addSeparator();
        m.addItem (4, "Supprimer");
        te::Clip::Ptr keep (&c);
        m.showMenuAsync (PopupMenu::Options(), [this, keep] (int r)
        {
            if (keep == nullptr) return;
            if (r == 1) Launcher::launch (*keep);
            if (r == 3) if (auto* mc = dynamic_cast<te::MidiClip*> (keep.get())) if (onOpenMidiEditor) onOpenMidiEditor (mc);
            if (r == 4) keep->removeFromParent();
            if (r == 2)
            {
                auto* aw = new AlertWindow ("Renommer le clip", {}, MessageBoxIconType::NoIcon);
                aw->addTextEditor ("name", keep->getName());
                aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
                aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
                aw->enterModalState (true, ModalCallbackFunction::create ([keep, aw] (int res)
                {
                    if (res == 1) keep->setName (aw->getTextEditorContents ("name"));
                }), true);
            }
        });
    }

    void renameScene (te::Scene& s)
    {
        auto* aw = new AlertWindow (String::fromUTF8 ("Renommer la scène"), {}, MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", s.name.get());
        aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
        auto* sp = &s;
        aw->enterModalState (true, ModalCallbackFunction::create ([sp, aw] (int res)
        {
            if (res == 1) sp->name = aw->getTextEditorContents ("name");
        }), true);
    }
};

} // namespace chen
