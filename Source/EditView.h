#pragma once

// =============================================================================
//  Fenêtre Édition : barre de transport, règle, en-têtes de pistes,
//  clips avec formes d'onde, curseur de lecture.
// =============================================================================

namespace chen
{

//==============================================================================
/** Barre d'outils : outils, modes d'édition, transport, afficheur LCD, CPU. */
class TransportBar : public Component, private Timer
{
public:
    enum class Mode { slip, grid, shuffle, spot };

    TransportBar (te::Edit& e) : edit (e) { startTimerHz (20); }

    std::function<void()> onStateChange;
    Mode mode = Mode::grid;

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff26272c), 0, 0, Colour (0xff1f2024), 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (Col::line);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        auto& tr = edit.getTransport();

        // marque : la tête de Pistache + ChènRecPro
        drawPistache (g, Rectangle<float> (10, (getHeight() - 36) / 2.0f, 72, 36), 6.0f);
        auto bx = 90;
        {
            auto f = uiFont (17.0f, true);
            g.setFont (f);
            auto x = (float) bx;
            auto part = [&] (const String& s, Colour c)
            {
                g.setColour (c);
                g.drawText (s, Rectangle<float> (x, 8, 80, 24), Justification::centredLeft);
                x += GlyphArrangement::getStringWidth (f, s);
            };
            part (String::fromUTF8 ("Chèn"), Colour (0xffe6e8ec));
            part ("Rec", Col::accent);
            part ("Pro", Colour (0xffe6e8ec));
        }
        g.setColour (Col::text3);
        g.setFont (uiFont (9.5f, true));
        g.drawText (String::fromUTF8 ("STUDIO DES CHÈNEVIÈRES"), bx, 30, 160, 14, Justification::centredLeft);

        // modes d'édition
        auto mr = modesRect();
        group (g, mr);
        const char* names[] = { "SLIP", "GRILLE", "SHUFFLE", "SPOT" };
        for (int i = 0; i < 4; ++i)
        {
            auto r = modeRect (i);
            bool on = (int) mode == i;
            if (on) { g.setColour (Colour (0xff33302a)); g.fillRoundedRectangle (r.toFloat().reduced (1), 3); }
            g.setColour (on ? Col::accent : Col::text2);
            g.setFont (uiFont (11.5f, on));
            g.drawText (names[i], r, Justification::centred);
        }

        // transport
        group (g, transportRect());
        for (int i = 0; i < numTransportButtons; ++i)
            drawTransportButton (g, i, transportButtonRect (i), tr);

        // afficheur LCD
        auto lcd = lcdRect().toFloat();
        g.setGradientFill (ColourGradient (Colour (0xff0d1310), lcd.getCentreX(), lcd.getY(), Colour (0xff070908), lcd.getCentreX(), lcd.getBottom(), false));
        g.fillRoundedRectangle (lcd, 5.0f);
        g.setColour (Col::accent.withAlpha (0.12f));
        g.drawRoundedRectangle (lcd.reduced (1), 5.0f, 1.0f);

        auto pos = tr.getPosition();
        auto bb = edit.tempoSequence.toBarsAndBeats (pos);
        auto beat = bb.beats.inBeats();
        auto ticks = (int) ((beat - std::floor (beat)) * 960.0);
        auto sixteenth = ticks / 240 + 1;
        String bars = String (bb.bars + 1).paddedLeft (' ', 3) + ". " + String ((int) std::floor (beat) + 1) + ". " + String (sixteenth) + ". " + String (ticks % 240).paddedLeft ('0', 3);

        auto secs = pos.inSeconds();
        auto fps = 25;
        String tc = String::formatted ("%02d:%02d:%02d:%02d", (int) secs / 3600, ((int) secs / 60) % 60, (int) secs % 60, (int) ((secs - std::floor (secs)) * fps));

        auto& ts = edit.tempoSequence.getTimeSigAt (pos);
        auto bpm = edit.tempoSequence.getBpmAt (pos);

        int x = (int) lcd.getX() + 8;
        auto cell = [&] (const String& k, const String& v, int w, float size, Colour c)
        {
            g.setColour (Colour (0xff6d7a72));
            g.setFont (uiFont (9.5f, true));
            g.drawText (k, x + 6, (int) lcd.getY() + 4, w, 12, Justification::centredLeft);
            g.setColour (c);
            g.setFont (monoFont (size));
            g.drawText (v, x + 6, (int) lcd.getY() + 15, w, (int) lcd.getHeight() - 18, Justification::centredLeft);
            x += w + 8;
            g.setColour (Colour (0xff16201b));
            g.fillRect (x, (int) lcd.getY() + 6, 1, (int) lcd.getHeight() - 12);
        };

        cell ("MESURES", bars, 200, 22.0f, Col::lcd);
        cell (String::fromUTF8 ("TIMECODE · 25 i/s"), tc, 128, 15.0f, Col::lcd);
        cell ("TEMPO", String (bpm, 3), 76, 15.0f, Col::lcd);
        cell ("SIGN.", String (ts.numerator.get()) + "/" + String (ts.denominator.get()), 48, 14.0f, Col::text);

        // charge CPU
        auto cr = cpuRect();
        if (cr.getWidth() < 70) return;   // pas la place : on masque le CPU
        auto cpu = (float) edit.engine.getDeviceManager().getCpuUsage();
        g.setColour (Col::text2);
        g.setFont (uiFont (10.5f));
        g.drawText ("CPU", cr.removeFromLeft (34), Justification::centredLeft);
        auto bar = cr.withSizeKeepingCentre (cr.getWidth(), 6).toFloat();
        g.setColour (Colour (0xff0d0e10));
        g.fillRoundedRectangle (bar, 2);
        g.setColour (cpu > 0.8f ? Col::rec : Col::play);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * jlimit (0.0f, 1.0f, cpu)), 2);
    }

    void mouseDown (const MouseEvent& e) override
    {
        auto p = e.getPosition();
        for (int i = 0; i < 4; ++i)
            if (modeRect (i).contains (p)) { mode = (Mode) i; repaint(); if (onStateChange) onStateChange(); return; }

        auto& tr = edit.getTransport();
        for (int i = 0; i < numTransportButtons; ++i)
        {
            if (! transportButtonRect (i).contains (p)) continue;
            switch (i)
            {
                case 0: tr.setPosition (0s); break;
                case 1: tr.setPosition (te::TimePosition::fromSeconds (jmax (0.0, tr.getPosition().inSeconds() - 2.0))); break;
                case 2: tr.setPosition (tr.getPosition() + te::TimeDuration::fromSeconds (2.0)); break;
                case 3: tr.stop (false, false); break;
                case 4: EngineHelpers::togglePlay (edit); break;
                case 5: Comping::toggleRecord (edit); break;
                case 6: tr.looping = ! tr.looping.get(); break;
                case 7: edit.clickTrackEnabled = ! edit.clickTrackEnabled.get(); break;
                case 8: { auto res = tr.applyRetrospectiveRecord (false); ignoreUnused (res); break; }
                case 9:
                {
                    auto& arm = edit.getAutomationRecordManager();
                    arm.setWritingAutomation (! arm.isWritingAutomation());
                    break;
                }
                default: break;
            }
            repaint();
            return;
        }
    }

private:
    te::Edit& edit;
    static constexpr int numTransportButtons = 10;

    void timerCallback() override { repaint(); }

    static void group (Graphics& g, Rectangle<int> r)
    {
        g.setColour (Colour (0xff17181b));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (Colour (0xff0b0c0e));
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
    }

    Rectangle<int> modesRect() const          { return { 256, (getHeight() - 34) / 2, 260, 34 }; }
    Rectangle<int> modeRect (int i) const     { auto r = modesRect().reduced (3); return r.withWidth (r.getWidth() / 4).translated (i * r.getWidth() / 4, 0); }
    Rectangle<int> transportRect() const      { return { jmax (modesRect().getRight() + 12, getWidth() / 2 - 420), (getHeight() - 34) / 2, numTransportButtons * 34 + 6, 34 }; }
    Rectangle<int> transportButtonRect (int i) const { auto r = transportRect(); return { r.getX() + 3 + i * 34, r.getY() + 3, 32, 28 }; }
    Rectangle<int> lcdRect() const            { return { transportRect().getRight() + 12, (getHeight() - 46) / 2, 520, 46 }; }
    Rectangle<int> cpuRect() const
    {
        auto x = jmax (lcdRect().getRight() + 16, getWidth() - 170);
        return { x, (getHeight() - 20) / 2, jmax (0, jmin (156, getWidth() - x - 10)), 20 };
    }

    void drawTransportButton (Graphics& g, int i, Rectangle<int> r, te::TransportControl& tr)
    {
        bool on = (i == 4 && tr.isPlaying()) || (i == 5 && tr.isRecording())
               || (i == 6 && tr.looping.get()) || (i == 7 && edit.clickTrackEnabled.get())
               || (i == 9 && edit.getAutomationRecordManager().isWritingAutomation());
        if (on)
        {
            g.setColour (i == 4 ? Colour (0xff1d3324) : i == 5 ? Colour (0xff3a1d1f) : Colour (0xff33302a));
            g.fillRoundedRectangle (r.toFloat(), 3.0f);
        }

        auto c = i == 4 ? Col::play : i == 5 ? Col::rec : (on ? Col::accent : Colour (0xffaeb2ba));
        g.setColour (c);
        auto cx = (float) r.getCentreX(), cy = (float) r.getCentreY();
        Path p;
        switch (i)
        {
            case 0: p.addRectangle (cx - 6, cy - 6, 2, 12); p.addTriangle (cx + 6, cy - 6, cx + 6, cy + 6, cx - 3, cy); break;
            case 1: p.addTriangle (cx, cy - 6, cx, cy + 6, cx - 7, cy); p.addTriangle (cx + 7, cy - 6, cx + 7, cy + 6, cx, cy); break;
            case 2: p.addTriangle (cx - 7, cy - 6, cx - 7, cy + 6, cx, cy); p.addTriangle (cx, cy - 6, cx, cy + 6, cx + 7, cy); break;
            case 3: p.addRectangle (cx - 6, cy - 6, 12, 12); break;
            case 4: p.addTriangle (cx - 5, cy - 7, cx - 5, cy + 7, cx + 7, cy); break;
            case 5: p.addEllipse (cx - 6.5f, cy - 6.5f, 13, 13); break;
            case 6:
            {
                Path loop;
                loop.startNewSubPath (cx - 6, cy + 3); loop.lineTo (cx - 6, cy - 3); loop.lineTo (cx + 5, cy - 3);
                loop.startNewSubPath (cx + 6, cy - 3); loop.lineTo (cx + 6, cy + 3); loop.lineTo (cx - 5, cy + 3);
                g.strokePath (loop, PathStrokeType (1.6f));
                p.addTriangle (cx + 3, cy - 6, cx + 3, cy, cx + 7, cy - 3);
                p.addTriangle (cx - 3, cy, cx - 3, cy + 6, cx - 7, cy + 3);
                break;
            }
            case 9:
            {
                // écriture d'automation
                g.setColour (on ? Col::rec : Colour (0xffaeb2ba));
                g.setFont (uiFont (13.0f, true));
                g.drawText ("W", r, Justification::centred);
                break;
            }
            case 8:
            {
                // MIDI rétrospectif : flèche circulaire + point
                g.setColour (Colour (0xffb993ff));
                Path arc; arc.addCentredArc (cx, cy, 6.5f, 6.5f, 0, -2.4f, 2.0f, true);
                g.strokePath (arc, PathStrokeType (1.6f));
                p.addTriangle (cx - 8, cy - 4, cx - 3, cy - 6, cx - 6, cy - 0.5f);
                p.addEllipse (cx - 1.8f, cy - 1.8f, 3.6f, 3.6f);
                break;
            }
            case 7:
            {
                Path m; m.startNewSubPath (cx - 3, cy - 7); m.lineTo (cx + 3, cy - 7); m.lineTo (cx + 6, cy + 7); m.lineTo (cx - 6, cy + 7); m.closeSubPath();
                g.strokePath (m, PathStrokeType (1.4f));
                g.drawLine (cx, cy + 3, cx + 4, cy - 4, 1.4f);
                break;
            }
            default: break;
        }
        g.fillPath (p);
    }
};

//==============================================================================
/** Zone de montage : règle, en-têtes, clips, curseur. Tout est dessiné ici. */
class ArrangeView : public Component,
                    public FileDragAndDropTarget,
                    private ScrollBar::Listener,
                    private Timer,
                    private ValueTree::Listener,
                    private AsyncUpdater
{
public:
    ArrangeView (te::Edit& e, TransportBar& tb) : edit (e), transportBar (tb)
    {
        edit.state.addListener (this);
        startTimerHz (30);
        setWantsKeyboardFocus (true);
        addAndMakeVisible (hScroll);
        addAndMakeVisible (vScroll);
        hScroll.setAutoHide (false);
        vScroll.setAutoHide (false);
        addChildComponent (playlistAnchor);
        hScroll.addListener (this);
        vScroll.addListener (this);
    }

    ~ArrangeView() override
    {
        edit.state.removeListener (this);
        for (auto* t : te::getAudioTracks (edit))
            if (attached.contains (t->itemID.getRawID()))
                if (auto* lm = t->getLevelMeterPlugin())
                    lm->measurer.removeClient (meterClients[t->itemID]);
    }

    static constexpr int headerW = 250, rulerH = 46, markerH = 18, chordH = 30;

    //==========================================================================
    // Piste d'accords
    bool chordsShown() const { return (bool) edit.state.getProperty ("chenShowChords", false); }
    int topH() const { return rulerH + (chordsShown() ? chordH : 0); }

public:
    void toggleChordTrack()
    {
        edit.state.setProperty ("chenShowChords", ! chordsShown(), nullptr);
        resized();
        repaint();
    }
    bool isChordTrackShown() const { return chordsShown(); }

    /** Piste instrument où écouter / écrire les accords : la sélection, sinon la première. */
    te::AudioTrack* chordTargetTrack() const
    {
        if (selectedTrack != nullptr && EngineHelpers::isInstrumentTrack (*selectedTrack)) return selectedTrack;
        for (auto* t : te::getAudioTracks (edit))
            if (EngineHelpers::isInstrumentTrack (*t)) return t;
        return nullptr;
    }

    void auditionChord (const Chords::Chord& c)
    {
        auto* t = chordTargetTrack();
        if (t == nullptr) return;
        auto notes = Chords::voicing (c);
        for (int n : notes) t->injectLiveMidiMessage (MidiMessage::noteOn (1, n, (uint8) 85), chordAuditionSource);
        auto tp = te::Track::Ptr (t);
        auto src = chordAuditionSource;
        Timer::callAfterDelay (700, [tp, notes, src]
        {
            if (auto* t2 = dynamic_cast<te::AudioTrack*> (tp.get()))
                for (int n : notes) t2->injectLiveMidiMessage (MidiMessage::noteOff (1, n), src);
        });
    }

private:
    te::MPESourceID chordAuditionSource = te::createUniqueMPESourceID();
    int chordDragIndex = -1;
    double chordDragOffset = 0.0;
    int selectedChord = -1;

    double beatAtX (int x) const { return edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (jmax (0.0, xToTime (x)))).inBeats(); }
    int xAtBeat (double b) const { return (int) timeToX (edit.tempoSequence.toTime (te::BeatPosition::fromBeats (b)).inSeconds()); }

    int chordIndexAt (int x) const
    {
        auto chords = Chords::get (edit);
        for (size_t i = 0; i < chords.size(); ++i)
        {
            int x1 = xAtBeat (chords[i].beat), x2 = xAtBeat (Chords::endOf (edit, chords, i));
            if (x >= x1 && x < jmax (x1 + 24, x2)) return (int) i;
        }
        return -1;
    }

    void paintChords (Graphics& g)
    {
        if (! chordsShown()) return;
        Rectangle<int> row (0, rulerH, getWidth(), chordH);
        g.setColour (Colour (0xff15161a));
        g.fillRect (row);
        auto hdr = row.withWidth (headerW);
        g.setColour (Col::panel1);
        g.fillRect (hdr);
        g.setColour (Col::accent);
        g.setFont (uiFont (11.0f, true));
        g.drawText (String::fromUTF8 ("♪  ACCORDS"), hdr.reduced (12, 0).removeFromLeft (90), Justification::centredLeft);
        g.setColour (Col::text3);
        g.setFont (uiFont (10.0f));
        auto* tt = chordTargetTrack();
        g.drawText (tt ? String::fromUTF8 ("écoute : ") + tt->getName() : String::fromUTF8 ("(pas de piste instrument)"),
                    hdr.reduced (12, 0).withTrimmedLeft (90), Justification::centredRight, true);

        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (row.withTrimmedLeft (headerW));
        auto chords = Chords::get (edit);
        for (size_t i = 0; i < chords.size(); ++i)
        {
            int x1 = xAtBeat (chords[i].beat), x2 = xAtBeat (Chords::endOf (edit, chords, i));
            auto r = Rectangle<int> (x1, rulerH + 3, jmax (24, x2 - x1) - 2, chordH - 6).toFloat();
            bool sel = (int) i == selectedChord;
            g.setColour (sel ? Col::accent : Colour (0xff2b2418));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (Col::accent.withAlpha (sel ? 1.0f : 0.7f));
            g.drawRoundedRectangle (r, 3.0f, 1.0f);
            g.setColour (sel ? Colour (0xff15100a) : Col::text);
            g.setFont (uiFont (13.0f, true));
            g.drawText (Chords::name (chords[i]), r.reduced (7, 0).toNearestInt(), Justification::centredLeft, true);
        }
        g.setColour (Colour (0xff24262c));
        g.fillRect (headerW, rulerH + chordH - 1, getWidth() - headerW, 1);
    }

    bool chordMouseDown (const MouseEvent& e)
    {
        if (! chordsShown() || e.y < rulerH || e.y >= rulerH + chordH) return false;
        if (e.x < headerW)
        {
            // en-tête : menu de la piste d'accords
            PopupMenu m;
            m.addItem (1, String::fromUTF8 ("Accords → MIDI (sur la piste instrument)"), chordTargetTrack() != nullptr && ! Chords::get (edit).empty());
            m.addItem (2, String::fromUTF8 ("Les notes du clip MIDI sélectionné suivent les accords"), dynamic_cast<te::MidiClip*> (selectedClip.get()) != nullptr);
            m.addItem (3, String::fromUTF8 ("Détecter les accords du clip MIDI sélectionné"), dynamic_cast<te::MidiClip*> (selectedClip.get()) != nullptr);
            m.addSeparator();
            m.addItem (4, String::fromUTF8 ("Effacer tous les accords"), ! Chords::get (edit).empty());
            m.addItem (5, String::fromUTF8 ("Masquer la piste d'accords"));
            m.showMenuAsync (PopupMenu::Options(), [sp = SafePointer<ArrangeView> (this)] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                sp->chordTrackCommand (r);
            });
            return true;
        }

        int idx = chordIndexAt (e.x);
        auto chords = Chords::get (edit);
        if (idx >= 0)
        {
            selectedChord = idx;
            if (e.mods.isPopupMenu())
            {
                editChord (idx);
                return true;
            }
            auditionChord (chords[(size_t) idx]);
            chordDragIndex = idx;
            chordDragOffset = beatAtX (e.x) - chords[(size_t) idx].beat;
            repaint();
            return true;
        }

        // case vide : poser un accord (suite logique : même accord que le précédent, à changer)
        const double beat = std::floor (beatAtX (e.x) + 1.0e-6);   // le temps sur lequel on a cliqué
        Chords::chooseChord (Chords::at (chords, beat), [sp = SafePointer<ArrangeView> (this), beat] (Chords::Chord c)
        {
            if (sp == nullptr) return;
            auto all = Chords::get (sp->edit);
            c.beat = beat;
            all.erase (std::remove_if (all.begin(), all.end(), [beat] (auto& x) { return std::abs (x.beat - beat) < 1.0e-3; }), all.end());
            all.push_back (c);
            std::sort (all.begin(), all.end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
            Chords::set (sp->edit, all);
            sp->auditionChord (c);
            sp->selectedChord = -1;
            sp->repaint();
        });
        return true;
    }

    void editChord (int idx)
    {
        auto chords = Chords::get (edit);
        if (idx < 0 || idx >= (int) chords.size()) return;
        auto cur = chords[(size_t) idx];
        SafePointer<ArrangeView> sp (this);
        Chords::chooseChord (cur,
            [sp, idx] (Chords::Chord c)
            {
                if (sp == nullptr) return;
                auto all = Chords::get (sp->edit);
                if (idx >= (int) all.size()) return;
                c.beat = all[(size_t) idx].beat;
                all[(size_t) idx] = c;
                Chords::set (sp->edit, all);
                sp->auditionChord (c);
                sp->repaint();
            },
            [sp, idx] { if (sp) sp->deleteChord (idx); },
            [sp, cur] { if (sp) sp->auditionChord (cur); });
    }

    void deleteChord (int idx)
    {
        auto all = Chords::get (edit);
        if (idx < 0 || idx >= (int) all.size()) return;
        all.erase (all.begin() + idx);
        Chords::set (edit, all);
        selectedChord = -1;
        repaint();
    }

    bool chordMouseDrag (const MouseEvent& e)
    {
        if (chordDragIndex < 0) return false;
        if (e.getDistanceFromDragStart() < 4) return true;
        auto all = Chords::get (edit);
        if (chordDragIndex >= (int) all.size()) { chordDragIndex = -1; return true; }
        double nb = jmax (0.0, (double) std::round (beatAtX (e.x) - chordDragOffset));   // accords calés sur les temps
        double lo = chordDragIndex > 0 ? all[(size_t) chordDragIndex - 1].beat + 0.25 : 0.0;
        double hi = chordDragIndex + 1 < (int) all.size() ? all[(size_t) chordDragIndex + 1].beat - 0.25 : 1.0e9;
        nb = jlimit (lo, jmax (lo, hi), nb);
        if (std::abs (nb - all[(size_t) chordDragIndex].beat) > 1.0e-6)
        {
            all[(size_t) chordDragIndex].beat = nb;
            Chords::set (edit, all);
            repaint();
        }
        return true;
    }

public:
    void chordTrackCommand (int r)
    {
        if (r == 1)
        {
            if (auto* t = chordTargetTrack())
                if (auto* mc = Chords::toMidi (edit, *t)) { selectedClip = mc; notifySelection(); }
        }
        else if (r == 2)
        {
            if (auto* mc = dynamic_cast<te::MidiClip*> (selectedClip.get())) Chords::conformClip (edit, *mc);
        }
        else if (r == 3)
        {
            if (auto* mc = dynamic_cast<te::MidiClip*> (selectedClip.get())) Chords::detectFromClip (edit, *mc);
        }
        else if (r == 4) Chords::set (edit, {});
        else if (r == 5) toggleChordTrack();
        repaint();
    }

public:

    //==========================================================================
    void paint (Graphics& g) override
    {
        auto tracks = te::getAudioTracks (edit);
        auto lanes = laneArea();

        g.fillAll (Col::bg);

        // ---- pistes
        {
            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (0, topH(), getWidth(), lanes.getHeight());
            int y = topH() - scrollY();
            int n = 1;
            for (auto* t : tracks)
            {
                if (int ex = extraAbove (*t); ex > 0)
                {
                    paintFolder (g, Folders::folderOf (*t), { 0, y, getWidth(), ex });
                    y += ex;
                }
                auto h = trackHeight (*t);
                Rectangle<int> row (0, y, getWidth(), h);
                if (h > 0 && row.getBottom() >= topH() && row.getY() <= getHeight())
                {
                    paintLane (g, *t, row.withTrimmedLeft (headerW).withHeight (mainH));
                    paintHeader (g, *t, n, row.withWidth (headerW).withHeight (mainH));
                    if (h > mainH) paintCompLanes (g, *t, row.withTrimmedTop (mainH));
                    if (auto fid = Folders::folderOf (*t); fid.isNotEmpty())
                    {
                        g.setColour (Folders::colour (edit, fid));       // filet du dossier
                        g.fillRect (0, row.getY(), 3, h);
                    }
                }
                y += h; ++n;
            }
            g.setColour (Colour (0xff121315));
            g.fillRect (headerW, y, getWidth() - headerW, getHeight() - y);
            g.setColour (Col::panel1);
            g.fillRect (0, y, headerW, getHeight() - y);
        }

        paintRuler (g);
        paintChords (g);

        // ---- curseur de lecture
        auto px = timeToX (edit.getTransport().getPosition().inSeconds());
        if (px >= headerW)
        {
            g.setColour (Col::accent.withAlpha (0.15f));
            g.fillRect ((float) px - 3, (float) topH(), 6.0f, (float) getHeight());
            g.setColour (Col::accent);
            g.fillRect ((float) px - 0.5f, (float) markerH, 1.5f, (float) getHeight());
            Path tri; tri.addTriangle ((float) px - 6, rulerH - 13.0f, (float) px + 6, rulerH - 13.0f, (float) px, rulerH - 3.0f);
            g.fillPath (tri);
        }

        if (dropHighlight)
        {
            g.setColour (Col::select.withAlpha (0.12f));
            g.fillRect (lanes);
            g.setColour (Col::select);
            g.drawRect (lanes, 2);
        }

        if (tracks.isEmpty())
        {
            g.setColour (Col::text3);
            g.setFont (uiFont (15.0f));
            g.drawText (String::fromUTF8 ("Ctrl+T pour créer une piste · glisse un fichier audio ici pour l'importer"),
                        lanes, Justification::centred);
        }
    }

    void resized() override
    {
        hScroll.setBounds (headerW, getHeight() - 12, getWidth() - headerW - 12, 12);
        vScroll.setBounds (getWidth() - 12, topH(), 12, getHeight() - topH() - 12);
        updateScrollbars();
    }

    //==========================================================================
    enum class DragMode { none, move, trimStart, trimEnd, fadeIn, fadeOut };

    void mouseMove (const MouseEvent& e) override
    {
        auto mode = DragMode::none;
        if (auto* t = trackAt (e.y); t != nullptr && e.x > headerW)
            if (auto* c = clipAt (*t, xToTime (e.x)))
                mode = hitZone (*t, *c, e.getPosition());

        switch (mode)
        {
            case DragMode::trimStart:
            case DragMode::trimEnd:  setMouseCursor (MouseCursor::LeftRightResizeCursor); break;
            case DragMode::fadeIn:
            case DragMode::fadeOut:  setMouseCursor (MouseCursor::CrosshairCursor); break;
            case DragMode::move:     setMouseCursor (MouseCursor::DraggingHandCursor); break;
            default:                 setMouseCursor (MouseCursor::NormalCursor); break;
        }
    }

    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus();
        auto p = e.getPosition();
        dragMode = DragMode::none;

        if (p.y < rulerH && p.x > headerW)
        {
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (snap (xToTime (p.x))));
            draggingPlayhead = true;
            return;
        }

        chordDragIndex = -1;
        if (chordMouseDown (e)) return;
        if (selectedChord >= 0) { selectedChord = -1; repaint(); }

        if (auto fid = folderAt (p.y); fid.isNotEmpty())
        {
            folderMouseDown (fid, e);
            return;
        }

        auto* t = trackAt (p.y);
        if (t == nullptr) { selectedClip = nullptr; notifySelection(); repaint(); return; }

        if (p.y >= trackTop (*t) + mainH)
        {
            laneMouseDown (*t, e);
            return;
        }

        if (p.x < headerW)
        {
            if (e.mods.isCommandDown() || e.mods.isCtrlDown() || e.mods.isShiftDown())
            {
                if (selectedTrack != nullptr) multiSel.addIfNotAlreadyThere (selectedTrack);
                if (multiSel.contains (t)) multiSel.removeFirstMatchingValue (t); else multiSel.add (t);
                selectedTrack = t;
                notifySelection();
                repaint();
                return;
            }
            multiSel.clear();
            headerClick (*t, p);
            notifySelection();
            return;
        }

        selectedTrack = t;

        if (automationView)
        {
            automationMouseDown (*t, e);
            notifySelection();
            repaint();
            return;
        }

        selectedClip = clipAt (*t, xToTime (p.x));
        if (selectedClip != nullptr)
        {
            auto pos = selectedClip->getPosition();
            orig = { pos.getStart().inSeconds(), pos.getEnd().inSeconds(), pos.getOffset().inSeconds() };
            dragMouseTime = xToTime (p.x);
            dragMode = hitZone (*t, *selectedClip, p);

            if (e.mods.isPopupMenu())
                showClipMenu();
        }
        else
        {
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (snap (xToTime (p.x))));
        }
        notifySelection();
        repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (chordMouseDrag (e)) return;
        if (draggingPlayhead)
        {
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (snap (jmax (0.0, xToTime (e.x)))));
            return;
        }

        if (autoDragParam != nullptr)
        {
            automationDrag (e);
            return;
        }

        if (headerVolTrack != nullptr)
        {
            auto pos = dbToFaderPos (headerVolStartDb) + (e.x - headerVolStartX) / (headerW - 100.0f) * (e.mods.isShiftDown() ? 0.2f : 1.0f);
            Groups::setVolume (*headerVolTrack, faderPosToDb (jlimit (0.0f, 1.0f, pos)), e.mods.isCommandDown() || e.mods.isCtrlDown());
            repaint();
            return;
        }

        if (compTrack != nullptr)
        {
            compT2 = jmax (0.0, xToTime (e.x));
            if (transportBar.mode == TransportBar::Mode::grid) compT2 = snap (compT2);
            repaint();
            return;
        }

        if (selectedClip == nullptr || e.getDistanceFromDragStart() < 3 || dragMode == DragMode::none)
            return;

        auto now = xToTime (e.x);
        auto snapped = [this] (double v) { return transportBar.mode == TransportBar::Mode::grid ? snap (v) : v; };
        auto* audio = dynamic_cast<te::AudioClipBase*> (selectedClip.get());

        switch (dragMode)
        {
            case DragMode::move:
            {
                if (transportBar.mode == TransportBar::Mode::spot) break;
                auto newStart = jmax (0.0, snapped (orig.start + now - dragMouseTime));
                selectedClip->setStart (te::TimePosition::fromSeconds (newStart), false, true);

                // déplacement vers une autre piste
                if (auto* target = trackAt (e.y); target != nullptr && target != selectedClip->getTrack())
                {
                    selectedClip->moveTo (*target);
                    selectedTrack = target;
                }
                break;
            }
            case DragMode::trimStart:
            {
                auto minStart = orig.start - orig.offset;           // début du fichier source
                auto newStart = jlimit (jmax (0.0, minStart), orig.end - 0.01, snapped (now));
                selectedClip->setStart (te::TimePosition::fromSeconds (newStart), true, false);
                break;
            }
            case DragMode::trimEnd:
            {
                auto pos = selectedClip->getPosition();
                auto maxEnd = pos.getStart().inSeconds() + selectedClip->getMaximumLength().inSeconds() - pos.getOffset().inSeconds();
                auto newEnd = jlimit (pos.getStart().inSeconds() + 0.01, jmax (pos.getStart().inSeconds() + 0.01, maxEnd), snapped (now));
                selectedClip->setEnd (te::TimePosition::fromSeconds (newEnd), true);
                break;
            }
            case DragMode::fadeIn:
                if (audio != nullptr)
                {
                    auto pos = audio->getPosition();
                    auto len = jlimit (0.0, pos.getLength().inSeconds() - audio->getFadeOut().inSeconds(), now - pos.getStart().inSeconds());
                    audio->setFadeIn (te::TimeDuration::fromSeconds (len));
                }
                break;
            case DragMode::fadeOut:
                if (audio != nullptr)
                {
                    auto pos = audio->getPosition();
                    auto len = jlimit (0.0, pos.getLength().inSeconds() - audio->getFadeIn().inSeconds(), pos.getEnd().inSeconds() - now);
                    audio->setFadeOut (te::TimeDuration::fromSeconds (len));
                }
                break;
            default: break;
        }
        notifySelection();
        repaint();
    }

    void mouseUp (const MouseEvent&) override
    {
        if (compTrack != nullptr)
        {
            auto lanes = Comping::getLanes (*compTrack);
            if (compLane >= 0 && compLane < (int) lanes.size())
                Comping::promote (*compTrack, lanes[(size_t) compLane], jmin (compT1, compT2), jmax (compT1, compT2));
            compTrack = nullptr;
            repaint();
        }
        draggingPlayhead = false; dragMode = DragMode::none; autoDragParam = nullptr; headerVolTrack = nullptr;
        chordDragIndex = -1;
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (chordsShown() && e.y >= rulerH && e.y < rulerH + chordH)
        {
            if (int idx = chordIndexAt (e.x); idx >= 0 && e.x >= headerW) editChord (idx);
            return;
        }
        auto* t = trackAt (e.y);
        if (t == nullptr) return;
        if (e.x < headerW)
        {
            if (e.y < trackTop (*t) + 26) renameTrack (*t);
            else if (e.y >= trackTop (*t) + 44 && e.y <= trackTop (*t) + 62) { Groups::setVolume (*t, 0.0f); repaint(); }   // double-clic : 0 dB
            return;
        }

        if (auto* mc = dynamic_cast<te::MidiClip*> (clipAt (*t, xToTime (e.x))))
        {
            if (onOpenMidiEditor) onOpenMidiEditor (mc);
            return;
        }

        // double-clic sur un clip audio : VariVox (comme l'éditeur d'échantillons / VariAudio de Cubase)
        if (auto* wc = dynamic_cast<te::WaveAudioClip*> (clipAt (*t, xToTime (e.x))))
        {
            if (onOpenVariVox) onOpenVariVox (wc);
            return;
        }

        // double-clic dans le vide d'une piste instrument : nouveau clip MIDI de 4 mesures
        if (EngineHelpers::isInstrumentTrack (*t))
        {
            auto& ts = edit.tempoSequence;
            auto bb = ts.toBarsAndBeats (te::TimePosition::fromSeconds (jmax (0.0, xToTime (e.x))));
            auto start = ts.toTime (te::tempo::BarsAndBeats { bb.bars, {} });
            auto end   = ts.toTime (te::tempo::BarsAndBeats { bb.bars + 4, {} });
            if (auto mc = t->insertMIDIClip ("MIDI", { start, end }, nullptr))
            {
                selectedClip = mc.get();
                notifySelection();
                if (onOpenMidiEditor) onOpenMidiEditor (mc.get());
            }
        }
    }

    std::function<void (te::MidiClip*)> onOpenMidiEditor;
    std::function<void (te::WaveAudioClip*)> onOpenVariVox;

    bool automationView = false;
    void toggleAutomationView() { automationView = ! automationView; repaint(); }
    void showAutomationView() { automationView = true; repaint(); }

    void selectTrack (te::AudioTrack* t)
    {
        selectedTrack = t;
        selectedClip = nullptr;
        notifySelection();
        repaint();
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            auto anchor = xToTime (e.x);
            pixelsPerSecond = jlimit (4.0, 2000.0, pixelsPerSecond * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15));
            scrollSeconds = jmax (0.0, anchor - (e.x - headerW) / pixelsPerSecond);
        }
        else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        {
            auto d = std::abs (w.deltaX) > 0 ? w.deltaX : w.deltaY;
            scrollSeconds = jmax (0.0, scrollSeconds - d * 300.0 / pixelsPerSecond);
        }
        else
        {
            vScroll.setCurrentRangeStart (vScroll.getCurrentRangeStart() - w.deltaY * 200.0);
        }
        updateScrollbars();
        repaint();
    }

    bool keyPressed (const KeyPress& k) override
    {
        auto code = k.getKeyCode();
        auto cmd = k.getModifiers().isCommandDown();

        if ((code == KeyPress::deleteKey || code == KeyPress::backspaceKey) && selectedChord >= 0)
        {
            deleteChord (selectedChord);
            return true;
        }

        if ((code == KeyPress::deleteKey || code == KeyPress::backspaceKey) && selectedClip != nullptr)
        {
            selectedClip->removeFromParent();
            selectedClip = nullptr;
            notifySelection();
            repaint();
            return true;
        }
        if (! cmd && (code == 'B' || code == 'b'))   { splitAtPlayhead(); return true; }
        if (cmd && (code == 'E' || code == 'e'))     { splitAtPlayhead(); return true; }   // Pro Tools : séparer
        if (cmd && k.getModifiers().isShiftDown() && (code == 'D' || code == 'd')) { folderFromSelection(); return true; }
        if (cmd && (code == 'D' || code == 'd'))     { duplicateSelected(); return true; }
        if (cmd && k.getModifiers().isShiftDown() && (code == KeyPress::upKey || code == KeyPress::downKey))
        {
            nudgeClipGain (code == KeyPress::upKey ? 1.0f : -1.0f);
            return true;
        }
        return false;
    }

    //==========================================================================
    void splitAtPlayhead()
    {
        auto pos = edit.getTransport().getPosition();
        te::Clip* c = selectedClip.get();
        if (c == nullptr || ! c->getPosition().time.contains (pos))
            c = selectedTrack ? clipAt (*selectedTrack, pos.inSeconds()) : nullptr;
        if (c == nullptr) return;
        if (auto* ct = dynamic_cast<te::ClipTrack*> (c->getTrack()))
            if (auto* right = ct->splitClip (*c, pos))
                selectedClip = right;
        notifySelection();
        repaint();
    }

    void duplicateSelected()
    {
        if (selectedClip == nullptr) return;
        auto* ct = dynamic_cast<te::ClipTrack*> (selectedClip->getTrack());
        if (ct == nullptr) return;
        auto copy = selectedClip->state.createCopy();
        edit.createNewItemID().writeID (copy, nullptr);
        if (auto* nc = ct->insertClipWithState (copy))
        {
            nc->setStart (selectedClip->getPosition().getEnd(), false, true);
            selectedClip = nc;
        }
        notifySelection();
        repaint();
    }

    void nudgeClipGain (float db)
    {
        if (auto* a = dynamic_cast<te::AudioClipBase*> (selectedClip.get()))
            a->setGainDB (jlimit (-48.0f, 24.0f, a->getGainDB() + db));
        notifySelection();
        repaint();
    }

    void showClipMenu()
    {
        PopupMenu m;
        m.addItem (1, String::fromUTF8 ("Séparer au curseur\tB"));
        m.addItem (2, "Dupliquer\tCtrl+D");
        m.addItem (3, "Supprimer\tSuppr");
        m.addSeparator();
        m.addItem (4, String::fromUTF8 ("Gain de clip +1 dB\tCtrl+Maj+↑"));
        m.addItem (5, String::fromUTF8 ("Gain de clip −1 dB\tCtrl+Maj+↓"));
        m.addItem (6, String::fromUTF8 ("Gain de clip à 0 dB"));
        m.addItem (7, "Retirer les fondus");
        if (dynamic_cast<te::WaveAudioClip*> (selectedClip.get()) != nullptr)
        {
            m.addSeparator();
            m.addItem (8, String::fromUTF8 ("Corriger la justesse (VariVox)…\tdouble-clic"));
        }
        m.showMenuAsync (PopupMenu::Options(), [this, sp = SafePointer<ArrangeView> (this)] (int r)
        {
            if (sp == nullptr || selectedClip == nullptr) return;
            if (r == 1) splitAtPlayhead();
            if (r == 2) duplicateSelected();
            if (r == 3) { selectedClip->removeFromParent(); selectedClip = nullptr; }
            if (r == 4) nudgeClipGain (1.0f);
            if (r == 5) nudgeClipGain (-1.0f);
            if (r == 8) if (auto* wc = dynamic_cast<te::WaveAudioClip*> (selectedClip.get())) if (onOpenVariVox) onOpenVariVox (wc);
            if (auto* a = dynamic_cast<te::AudioClipBase*> (selectedClip.get()))
            {
                if (r == 6) a->setGainDB (0.0f);
                if (r == 7) { a->setFadeIn ({}); a->setFadeOut ({}); }
            }
            notifySelection();
            repaint();
        });
    }

    std::function<void()> onSelectionChanged;
    void openInputMenu (te::AudioTrack& t) { showInputMenu (t); }
    te::Clip* getSelectedClip() const { return selectedClip.get(); }

    //==========================================================================
    bool isInterestedInFileDrag (const StringArray& files) override
    {
        for (auto& f : files)
            if (File (f).hasFileExtension ("mid;midi")) return true;
        for (auto& f : files)
            if (edit.engine.getAudioFileFormatManager().readFormatManager.findFormatForFileExtension (File (f).getFileExtension()) != nullptr)
                return true;
        return false;
    }

    void fileDragEnter (const StringArray&, int, int) override { dropHighlight = true; repaint(); }
    void fileDragExit (const StringArray&) override            { dropHighlight = false; repaint(); }

    void filesDropped (const StringArray& files, int x, int y) override
    {
        dropHighlight = false;
        auto* t = trackAt (y);
        auto start = snap (jmax (0.0, xToTime (jmax (x, headerW))));
        for (auto& f : files)
        {
            if (t == nullptr)
                t = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr).get();
            if (t == nullptr) break;
            if (File (f).hasFileExtension ("mid;midi")) importMidiFile (*t, File (f), start);
            else                                        importFile (*t, File (f), start);
            t = nextTrack (*t);
        }
        repaint();
    }

    void importFile (te::AudioTrack& t, const File& f, double start)
    {
        te::AudioFile af (edit.engine, f);
        if (! af.isValid()) return;
        if (t.getClips().isEmpty() && t.getName().startsWith ("Audio"))
            t.setName (f.getFileNameWithoutExtension());
        t.insertWaveClip (f.getFileNameWithoutExtension(), f,
                          { { te::TimePosition::fromSeconds (start), te::TimeDuration::fromSeconds (af.getLength()) }, {} }, false);
    }

    /** Importe un fichier MIDI (toutes les pistes fusionnées) dans un clip. */
    void importMidiFile (te::AudioTrack& t, const File& f, double start)
    {
        FileInputStream in (f);
        MidiFile mf;
        if (! in.openedOk() || ! mf.readFrom (in)) return;
        auto ppq = mf.getTimeFormat() > 0 ? (double) mf.getTimeFormat() : 960.0;

        MidiMessageSequence all;
        for (int i = 0; i < mf.getNumTracks(); ++i) all.addSequence (*mf.getTrack (i), 0);
        all.updateMatchedPairs();

        double lastBeat = 4.0;
        for (auto* ev : all)
            if (ev->message.isNoteOn() && ev->noteOffObject != nullptr)
                lastBeat = jmax (lastBeat, ev->noteOffObject->message.getTimeStamp() / ppq);

        auto& ts = edit.tempoSequence;
        auto startT = te::TimePosition::fromSeconds (start);
        auto startBeat = ts.toBeats (startT);
        auto lenBeats = std::ceil (lastBeat / 4.0) * 4.0;
        auto endT = ts.toTime (startBeat + te::BeatDuration::fromBeats (lenBeats));

        if (auto mc = t.insertMIDIClip (f.getFileNameWithoutExtension(), { startT, endT }, nullptr))
        {
            auto& seq = mc->getSequence();
            for (auto* ev : all)
                if (ev->message.isNoteOn() && ev->noteOffObject != nullptr)
                {
                    auto b = ev->message.getTimeStamp() / ppq;
                    auto l = jmax (0.01, ev->noteOffObject->message.getTimeStamp() / ppq - b);
                    seq.addNote (ev->message.getNoteNumber(), te::BeatPosition::fromBeats (b), te::BeatDuration::fromBeats (l),
                                 ev->message.getVelocity(), 0, nullptr);
                }
        }
        if (t.getClips().size() == 1 && t.getName().startsWith ("Audio"))
            t.setName (f.getFileNameWithoutExtension());
    }

    te::AudioTrack* getSelectedTrack() const { return selectedTrack; }

    /** Pistes sélectionnées (Ctrl/Maj+clic dans les en-têtes). */
    Array<te::AudioTrack*> getSelectedTracks() const
    {
        auto res = multiSel;
        if (selectedTrack != nullptr) res.addIfNotAlreadyThere (selectedTrack);
        auto all = te::getAudioTracks (edit);
        res.removeIf ([&all] (te::AudioTrack* t) { return ! all.contains (t); });
        return res;
    }

    Array<te::AudioTrack*> multiSel;
    te::AudioTrack* headerVolTrack = nullptr;
    float headerVolStartDb = 0.0f;
    int headerVolStartX = 0;

    void zoomToFit()
    {
        auto len = jmax (30.0, edit.getLength().inSeconds() + 4.0);
        pixelsPerSecond = (getWidth() - headerW - 20) / len;
        scrollSeconds = 0;
        updateScrollbars();
        repaint();
    }

private:
    te::Edit& edit;
    TransportBar& transportBar;
    double pixelsPerSecond = 40.0, scrollSeconds = 0.0;
    ScrollBar hScroll { false }, vScroll { true };
    te::AudioTrack* selectedTrack = nullptr;
    te::Clip::Ptr selectedClip;
    double dragMouseTime = 0;
    struct { double start = 0, end = 0, offset = 0; } orig;
    DragMode dragMode = DragMode::none;
    Component playlistAnchor;

    void notifySelection()
    {
        if (selectedTrack != nullptr && EngineHelpers::isInstrumentTrack (*selectedTrack))
            EngineHelpers::routeMidiTo (*selectedTrack);
        if (onSelectionChanged) onSelectionChanged();
    }

    Rectangle<float> clipRect (te::Track& t, te::Clip& c) const
    {
        auto pos = c.getPosition();
        auto x1 = timeToX (pos.getStart().inSeconds()), x2 = timeToX (pos.getEnd().inSeconds());
        return { x1, (float) trackTop (t) + 2, x2 - x1, (float) mainH - 5 };
    }

    DragMode hitZone (te::Track& t, te::Clip& c, Point<int> p) const
    {
        auto r = clipRect (t, c);
        auto x = (float) p.x, y = (float) p.y;
        bool inHeader = y < r.getY() + 16;
        auto* a = dynamic_cast<te::AudioClipBase*> (&c);
        if (a != nullptr && inHeader)
        {
            auto fin  = r.getX() + (float) (a->getFadeIn().inSeconds() * pixelsPerSecond);
            auto fout = r.getRight() - (float) (a->getFadeOut().inSeconds() * pixelsPerSecond);
            if (x < jmax (r.getX() + 12, fin + 6))        return DragMode::fadeIn;
            if (x > jmin (r.getRight() - 12, fout - 6))   return DragMode::fadeOut;
        }
        if (! inHeader && x < r.getX() + 7)      return DragMode::trimStart;
        if (! inHeader && x > r.getRight() - 7)  return DragMode::trimEnd;
        return DragMode::move;
    }
    bool draggingPlayhead = false, dropHighlight = false;
    struct ThumbEntry { File file; std::unique_ptr<te::SmartThumbnail> thumb; };
    std::map<te::EditItemID, ThumbEntry> thumbs;
    std::map<te::EditItemID, te::LevelMeasurer::Client> meterClients;
    std::map<te::EditItemID, float> meterLevels;
    bool wasRecording = false;

    Rectangle<int> laneArea() const { return { headerW, topH(), getWidth() - headerW - 12, getHeight() - topH() - 12 }; }
    int scrollY() const             { return (int) vScroll.getCurrentRangeStart(); }
    static constexpr int mainH = 86, laneH = 46;
    static constexpr int folderH = 30;
    int trackHeight (te::Track& t) const
    {
        if (auto* at = dynamic_cast<te::AudioTrack*> (&t); at != nullptr && Folders::isHidden (edit, *at)) return 0;
        return mainH + Comping::countLanes (t) * laneH;
    }
    /** En-tête de dossier au-dessus de la première piste d'un dossier. */
    int extraAbove (te::AudioTrack& t) const { return Folders::isFirstMember (edit, t) ? folderH : 0; }

    /** Dossier dont l'en-tête est à cette hauteur (vide sinon). */
    String folderAt (int y) const
    {
        if (y < topH()) return {};
        int ty = topH() - scrollY();
        for (auto* t : te::getAudioTracks (edit))
        {
            if (int ex = extraAbove (*t); ex > 0)
            {
                if (y >= ty && y < ty + ex) return Folders::folderOf (*t);
                ty += ex;
            }
            ty += trackHeight (*t);
        }
        return {};
    }

    int folderTop (const String& id) const
    {
        int ty = topH() - scrollY();
        for (auto* t : te::getAudioTracks (edit))
        {
            if (int ex = extraAbove (*t); ex > 0)
            {
                if (Folders::folderOf (*t) == id) return ty;
                ty += ex;
            }
            ty += trackHeight (*t);
        }
        return ty;
    }

    double xToTime (int x) const    { return scrollSeconds + (x - headerW) / pixelsPerSecond; }
    float timeToX (double t) const  { return (float) (headerW + (t - scrollSeconds) * pixelsPerSecond); }

    double snap (double t) const
    {
        if (transportBar.mode != TransportBar::Mode::grid) return t;
        auto beats = edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t)).inBeats();
        auto snapped = std::round (beats * 4.0) / 4.0;   // double-croche
        return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (snapped)).inSeconds();
    }

    int trackTop (te::Track& t) const
    {
        int y = topH() - scrollY();
        for (auto* tr : te::getAudioTracks (edit)) { y += extraAbove (*tr); if (tr == &t) return y; y += trackHeight (*tr); }
        return y;
    }

    te::AudioTrack* trackAt (int y) const
    {
        if (y < topH()) return nullptr;
        int ty = topH() - scrollY();
        for (auto* t : te::getAudioTracks (edit))
        {
            ty += extraAbove (*t);
            auto h = trackHeight (*t);
            if (y >= ty && y < ty + h) return t;
            ty += h;
        }
        return nullptr;
    }

    te::AudioTrack* nextTrack (te::AudioTrack& t) const
    {
        auto tracks = te::getAudioTracks (edit);
        auto i = tracks.indexOf (&t);
        return i >= 0 && i + 1 < tracks.size() ? tracks[i + 1] : nullptr;
    }

    te::Clip* clipAt (te::AudioTrack& t, double time) const
    {
        for (auto* c : t.getClips())
            if (c->getPosition().time.contains (te::TimePosition::fromSeconds (time)))
                return c;
        return nullptr;
    }

    void scrollBarMoved (ScrollBar* sb, double start) override
    {
        if (sb == &hScroll) scrollSeconds = start;
        repaint();
    }

    void updateScrollbars()
    {
        int total = 0;
        for (auto* t : te::getAudioTracks (edit)) total += trackHeight (*t) + extraAbove (*t);
        vScroll.setRangeLimits (0, jmax (total + 40, laneArea().getHeight()));
        vScroll.setCurrentRange (vScroll.getCurrentRangeStart(), laneArea().getHeight());

        auto len = jmax (edit.getLength().inSeconds() + 60.0, 300.0);
        hScroll.setRangeLimits (0, len);
        hScroll.setCurrentRange (scrollSeconds, laneArea().getWidth() / pixelsPerSecond, dontSendNotification);
    }

    //==========================================================================
    void paintRuler (Graphics& g)
    {
        g.setColour (Col::panel1);
        g.fillRect (0, 0, headerW, rulerH);
        g.setColour (Col::text3);
        g.setFont (uiFont (11.0f));
        g.drawText (String (te::getAudioTracks (edit).size()) + " pistes", 10, 0, headerW - 20, rulerH, Justification::centredLeft);
        g.setColour (Col::line);
        g.fillRect (headerW - 1, 0, 1, getHeight());

        Rectangle<int> r (headerW, 0, getWidth() - headerW, rulerH);
        g.setColour (Colour (0xff1a1b1f));
        g.fillRect (r);
        g.setColour (Col::bg);
        g.fillRect (r.withHeight (markerH));

        // boucle
        auto& tr = edit.getTransport();
        auto loop = tr.getLoopRange();
        if (loop.getLength() > 0s)
        {
            auto x1 = timeToX (loop.getStart().inSeconds()), x2 = timeToX (loop.getEnd().inSeconds());
            g.setColour (Col::accent.withAlpha (tr.looping.get() ? 0.25f : 0.08f));
            g.fillRect (x1, (float) markerH + 1, x2 - x1, 6.0f);
        }

        // graduations
        auto t0 = xToTime (headerW), t1 = xToTime (getWidth());
        auto b0 = (int) std::floor (edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (jmax (0.0, t0))).inBeats());
        auto b1 = (int) std::ceil (edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t1)).inBeats());
        auto beatPx = pixelsPerSecond * 60.0 / jmax (1.0, edit.tempoSequence.getBpmAt (te::TimePosition::fromSeconds (t0)));
        int barEvery = beatPx * 4 < 30 ? 4 : 1;

        for (int b = b0; b <= b1; ++b)
        {
            auto x = timeToX (edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) b)).inSeconds());
            auto bb = edit.tempoSequence.toBarsAndBeats (edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) b)));
            bool isBar = bb.getWholeBeats() == 0;
            if (isBar)
            {
                g.setColour (Col::text2);
                g.fillRect (x, (float) rulerH - 14, 1.0f, 14.0f);
                if (bb.bars % barEvery == 0)
                {
                    g.setColour (Col::text);
                    g.setFont (uiFont (11.5f, true));
                    g.drawText (String (bb.bars + 1), (int) x + 4, rulerH - 18, 40, 14, Justification::centredLeft);
                }
            }
            else if (beatPx > 8)
            {
                g.setColour (Colour (0xff4a4d55));
                g.fillRect (x, (float) rulerH - 6, 1.0f, 6.0f);
            }
        }

        // marqueurs
        if (auto* mt = edit.getMarkerTrack())
            for (auto* c : mt->getClips())
            {
                auto x = timeToX (c->getPosition().getStart().inSeconds());
                g.setColour (Col::accent.withAlpha (0.22f));
                g.fillRect (x, 1.0f, (float) (c->getPosition().getLength().inSeconds() * pixelsPerSecond), (float) markerH - 2);
                g.setColour (Col::accent);
                g.fillRect (x, 1.0f, 2.0f, (float) markerH - 2);
                g.setFont (uiFont (11.0f, true));
                g.drawText (c->getName().toUpperCase(), (int) x + 6, 1, 200, markerH - 2, Justification::centredLeft);
            }

        g.setColour (Col::line);
        g.fillRect (headerW, rulerH - 1, getWidth() - headerW, 1);
    }

    //==========================================================================
    // Dossiers
    Rectangle<int> folderToggleRect (int y) const { return { 8, y + 7, 16, 16 }; }
    Rectangle<int> folderMuteRect (int y) const   { return { headerW - 110, y + 6, 20, 17 }; }
    Rectangle<int> folderSoloRect (int y) const   { return { headerW - 88, y + 6, 20, 17 }; }

    void paintFolder (Graphics& g, const String& id, Rectangle<int> r)
    {
        if (r.getBottom() < topH() || r.getY() > getHeight()) return;
        auto col = Folders::colour (edit, id);
        auto members = Folders::members (edit, id);
        bool collapsed = Folders::isCollapsed (edit, id);

        // en-tête
        auto hr = r.withWidth (headerW);
        g.setGradientFill (ColourGradient (col.withAlpha (0.28f).overlaidWith (Col::panel2.withAlpha (0.6f)), 0, (float) hr.getY(),
                                           Col::panel1, 0, (float) hr.getBottom(), false));
        g.fillRect (hr);
        g.setColour (col);
        g.fillRect (0, r.getY(), 3, r.getHeight());
        auto tg = folderToggleRect (r.getY()).toFloat();
        Path tri;
        if (collapsed) tri.addTriangle (tg.getX() + 4, tg.getY() + 3, tg.getX() + 4, tg.getBottom() - 3, tg.getRight() - 3, tg.getCentreY());
        else           tri.addTriangle (tg.getX() + 3, tg.getY() + 5, tg.getRight() - 3, tg.getY() + 5, tg.getCentreX(), tg.getBottom() - 4);
        g.setColour (col);
        g.fillPath (tri);
        g.setColour (Col::text);
        g.setFont (uiFont (13.0f, true));
        g.drawText (Folders::name (edit, id), 30, r.getY(), headerW - 150, r.getHeight(), Justification::centredLeft, true);
        g.setColour (Col::text3);
        g.setFont (uiFont (10.5f));
        g.drawText (String (members.size()) + (members.size() > 1 ? " pistes" : " piste"), headerW - 64, r.getY(), 56, r.getHeight(), Justification::centredRight);
        drawPill (g, folderMuteRect (r.getY()).toFloat(), "M", Folders::allMuted (edit, id), Colour (0xffc98a1a));
        drawPill (g, folderSoloRect (r.getY()).toFloat(), "S", Folders::allSoloed (edit, id), Colour (0xffd6c33a));

        // zone : aperçu des clips des pistes du dossier
        auto lr = r.withTrimmedLeft (headerW);
        g.setColour (col.withAlpha (0.07f));
        g.fillRect (lr);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (lr);
            for (auto* t : members)
                for (auto* c : t->getClips())
                {
                    auto pos = c->getPosition();
                    float x1 = timeToX (pos.getStart().inSeconds()), x2 = timeToX (pos.getEnd().inSeconds());
                    if (x2 < (float) headerW || x1 > (float) getWidth()) continue;
                    g.setColour (t->getColour().withAlpha (0.45f));
                    g.fillRect (x1, (float) r.getY() + 5.0f, jmax (2.0f, x2 - x1), (float) r.getHeight() - 10.0f);
                }
        }
        g.setColour (Col::line);
        g.fillRect (0, r.getBottom() - 1, getWidth(), 1);
        g.fillRect (headerW - 1, r.getY(), 1, r.getHeight());
    }

    void folderMouseDown (const String& id, const MouseEvent& e)
    {
        int y = folderTop (id);
        auto p = e.getPosition();
        if (p.x < headerW && folderMuteRect (y).contains (p)) { Folders::setMute (edit, id, ! Folders::allMuted (edit, id)); repaint(); return; }
        if (p.x < headerW && folderSoloRect (y).contains (p)) { Folders::setSolo (edit, id, ! Folders::allSoloed (edit, id)); repaint(); return; }

        if (e.mods.isPopupMenu())
        {
            showFolderMenu (id);
            return;
        }
        if (e.getNumberOfClicks() > 1 && p.x < headerW && p.x > 30) { renameFolder (id); return; }

        if (folderToggleRect (y).expanded (6).contains (p) || p.x >= headerW || e.getNumberOfClicks() > 1)
        {
            Folders::setCollapsed (edit, id, ! Folders::isCollapsed (edit, id));
            updateScrollbars();
            repaint();
            return;
        }

        // clic sur le nom : sélectionne les pistes du dossier
        auto m = Folders::members (edit, id);
        multiSel = m;
        selectedTrack = m.isEmpty() ? nullptr : m.getFirst();
        notifySelection();
        repaint();
    }

    void renameFolder (const String& id)
    {
        auto* aw = new AlertWindow (String::fromUTF8 ("Renommer le dossier"), {}, MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", Folders::name (edit, id));
        aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([sp = SafePointer<ArrangeView> (this), aw, id] (int r)
        {
            if (r == 1 && sp != nullptr)
            {
                auto n = aw->getTextEditorContents ("name").trim();
                if (n.isNotEmpty()) Folders::rename (sp->edit, id, n);
                sp->repaint();
            }
        }), true);
    }

    void showFolderMenu (const String& id)
    {
        PopupMenu m;
        bool collapsed = Folders::isCollapsed (edit, id);
        m.addItem (1, collapsed ? String::fromUTF8 ("Déplier") : String::fromUTF8 ("Replier"));
        m.addItem (2, String::fromUTF8 ("Renommer…"));
        m.addItem (3, String::fromUTF8 ("Sélectionner les pistes du dossier"));
        PopupMenu colours;
        for (int i = 0; i < 6; ++i) colours.addItem (100 + i, String::fromUTF8 ("Couleur ") + String (i + 1));
        m.addSubMenu (String::fromUTF8 ("Couleur"), colours);
        m.addSeparator();
        m.addItem (4, String::fromUTF8 ("Tout replier"));
        m.addItem (5, String::fromUTF8 ("Tout déplier"));
        m.addSeparator();
        m.addItem (6, String::fromUTF8 ("Défaire le dossier (garder les pistes)"));
        m.showMenuAsync (PopupMenu::Options(), [sp = SafePointer<ArrangeView> (this), id] (int r)
        {
            if (sp == nullptr || r <= 0) return;
            auto& ed = sp->edit;
            if (r == 1) Folders::setCollapsed (ed, id, ! Folders::isCollapsed (ed, id));
            else if (r == 2) sp->renameFolder (id);
            else if (r == 3) { auto mm = Folders::members (ed, id); sp->multiSel = mm; sp->selectedTrack = mm.isEmpty() ? nullptr : mm.getFirst(); sp->notifySelection(); }
            else if (r == 4 || r == 5)
            {
                for (auto f : ed.state.getChildWithName (Folders::listType))
                    Folders::setCollapsed (ed, f.getProperty ("id").toString(), r == 4);
            }
            else if (r == 6) Folders::dissolve (ed, id);
            else if (r >= 100) Folders::folderTree (ed, id).setProperty ("colour", r - 100, nullptr);
            sp->updateScrollbars();
            sp->repaint();
        });
    }

public:
    /** Range les pistes sélectionnées (ou la piste courante) dans un nouveau dossier. */
    void folderFromSelection()
    {
        auto sel = getSelectedTracks();
        if (sel.isEmpty()) return;
        for (auto* t : sel) Folders::removeFromFolder (edit, *t);
        Folders::create (edit, sel);
        updateScrollbars();
        repaint();
    }

    void removeSelectionFromFolder()
    {
        for (auto* t : getSelectedTracks()) Folders::removeFromFolder (edit, *t);
        updateScrollbars();
        repaint();
    }

private:
    void paintHeader (Graphics& g, te::AudioTrack& t, int number, Rectangle<int> r)
    {
        bool sel = &t == selectedTrack || multiSel.contains (&t);
        g.setGradientFill (ColourGradient (sel ? Colour (0xff2d3039) : Col::panel2, (float) r.getX(), 0,
                                           sel ? Colour (0xff23252b) : Col::panel1, (float) r.getRight(), 0, false));
        g.fillRect (r);
        g.setColour (Col::line);
        g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);
        g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());

        g.setColour (t.getColour());
        g.fillRect (r.getX(), r.getY(), 5, r.getHeight() - 1);

        if (auto gr = Groups::groupOf (t); gr > 0)
        {
            auto b = Rectangle<float> ((float) r.getX() + 8, r.getY() + 7.0f, 16, 15);
            g.setColour (Groups::colour (gr));
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (Colour (0xff141414));
            g.setFont (uiFont (11.0f, true));
            g.drawText (Groups::letter (gr), b, Justification::centred);
        }
        else
        {
            g.setColour (Col::text3);
            g.setFont (uiFont (11.0f));
            g.drawText (String (number), r.getX() + 10, r.getY() + 6, 18, 18, Justification::centredLeft);
        }
        g.setColour (Colour (0xffe3e5e9));
        g.setFont (uiFont (13.5f, true));
        g.drawText (t.getName(), r.getX() + 30, r.getY() + 6, r.getWidth() - 140, 18, Justification::centredLeft, true);

        auto bx = r.getRight() - 110;
        drawPill (g, { (float) bx,      r.getY() + 6.0f, 20, 17 }, "M", t.isMuted (false), Colour (0xffc98a1a));
        drawPill (g, { (float) bx + 22, r.getY() + 6.0f, 20, 17 }, "S", t.isSolo (false), Colour (0xffd6c33a));
        drawPill (g, { (float) bx + 44, r.getY() + 6.0f, 20, 17 }, String::fromUTF8 ("●"), EngineHelpers::isTrackArmed (t), Colour (0xffc43b40), Colours::white);
        drawPill (g, { (float) bx + 66, r.getY() + 6.0f, 20, 17 }, "I", EngineHelpers::isInputMonitoringEnabled (t), Colour (0xff2f6fb3), Colours::white);

        // entrée
        String input = String::fromUTF8 ("Aucune entrée");
        if (EngineHelpers::isInstrumentTrack (t))
            input = String::fromUTF8 ("MIDI · ") + EngineHelpers::instrumentName (t);
        else
            for (auto* in : edit.getAllInputDevices())
                if (te::isOnTargetTrack (*in, t, 0)) input = in->getInputDevice().getName();
        g.setColour (Col::text3);
        g.setFont (uiFont (11.0f));
        g.drawText (input, r.getX() + 30, r.getY() + 27, r.getWidth() - 170, 14, Justification::centredLeft, true);

        // bouton couloirs de prises
        {
            auto lr = Rectangle<float> ((float) r.getRight() - 134, r.getY() + 26.0f, 18, 16);
            drawPill (g, lr, String::fromUTF8 ("≡"), Comping::lanesShown (t), Col::accent);
        }

        // playlist
        {
            auto pr = Rectangle<float> ((float) r.getRight() - 112, r.getY() + 26.0f, 88, 16);
            g.setColour (Colour (0xff131416));
            g.fillRoundedRectangle (pr, 2);
            g.setColour (Col::accent);
            g.setFont (uiFont (10.5f));
            g.drawText (String::fromUTF8 ("▾ ") + Playlists::getCurrent (t), pr.reduced (4, 0), Justification::centredLeft, true);
        }

        // volume
        if (auto* v = t.getVolumePlugin())
        {
            auto vr = Rectangle<float> ((float) r.getX() + 30, r.getY() + 52.0f, (float) r.getWidth() - 100, 4.0f);
            auto pos = dbToFaderPos (v->getVolumeDb());
            g.setColour (Colour (0xff121315));
            g.fillRoundedRectangle (vr, 2);
            g.setColour (Colour (0xff6c717b));
            g.fillRoundedRectangle (vr.withWidth (vr.getWidth() * pos), 2);
            g.setColour (Col::text);
            g.fillRoundedRectangle (vr.getX() + vr.getWidth() * pos - 3, vr.getY() - 3, 6, 10, 1);
            g.setColour (Col::text2);
            g.setFont (uiFont (10.5f));
            g.drawText (dbString (v->getVolumeDb()), (int) vr.getRight() + 6, (int) vr.getY() - 6, 44, 16, Justification::centredRight);
        }

        // mètre
        auto lvl = meterLevels.count (t.itemID) ? meterLevels[t.itemID] : -100.0f;
        drawMeter (g, { (float) r.getRight() - 10, r.getY() + 4.0f, 6, r.getHeight() - 9.0f }, dbToMeter (lvl), 0);
    }

    void headerClick (te::AudioTrack& t, Point<int> p)
    {
        selectedTrack = &t;
        auto top = trackTop (t);
        auto bx = headerW - 110;
        if (p.y >= top + 6 && p.y <= top + 23)
        {
            auto i = (p.x - bx) / 22;
            if (p.x >= bx && i < 4)
            {
                if (i == 0) Groups::setMute (t, ! t.isMuted (false));
                if (i == 1) Groups::setSolo (t, ! t.isSolo (false));
                if (i == 2) EngineHelpers::armTrack (t, ! EngineHelpers::isTrackArmed (t));
                if (i == 3) EngineHelpers::enableInputMonitoring (t, ! EngineHelpers::isInputMonitoringEnabled (t));
            }
        }
        else if (p.y >= top + 44 && p.y <= top + 62 && p.x >= 22 && p.x <= headerW - 62)
        {
            // il faut attraper la poignée (évite les changements de niveau accidentels)
            if (auto* v = t.getVolumePlugin())
            {
                auto handleX = 30.0f + (headerW - 100.0f) * dbToFaderPos (v->getVolumeDb());
                if (std::abs (p.x - handleX) <= 8.0f)
                {
                    headerVolTrack = &t;
                    headerVolStartDb = v->getVolumeDb();
                    headerVolStartX = p.x;
                }
            }
        }
        else if (p.y >= top + 26 && p.y <= top + 42 && p.x >= headerW - 134 && p.x < headerW - 114)
        {
            Comping::setLanesShown (t, ! Comping::lanesShown (t));
            updateScrollbars();
        }
        else if (p.y >= top + 26 && p.y <= top + 42 && p.x >= headerW - 112)
        {
            playlistAnchor.setBounds (headerW - 112, top + 26, 88, 16);
            Playlists::showMenu (t, &playlistAnchor);
        }
        else if (p.y >= top + 26 && p.y <= top + 42)
        {
            showInputMenu (t);
        }
        repaint();
    }

    void showInputMenu (te::AudioTrack& t)
    {
        PopupMenu m;
        m.addSectionHeader (String::fromUTF8 ("ENTRÉE DE LA PISTE"));
        int id = 1;
        Array<te::InputDeviceInstance*> ins;
        for (auto* in : edit.getAllInputDevices())
        {
            if (in->getInputDevice().getDeviceType() != te::InputDevice::waveDevice) continue;
            m.addItem (id++, in->getInputDevice().getName(), true, te::isOnTargetTrack (*in, t, 0));
            ins.add (in);
        }
        if (ins.isEmpty()) m.addItem (-1, String::fromUTF8 ("Aucune entrée audio — vérifie les Réglages audio"), false);

        m.showMenuAsync (PopupMenu::Options(), [this, ins, &t] (int r)
        {
            if (r <= 0) return;
            auto* in = ins[r - 1];
            for (auto* other : edit.getAllInputDevices())
                if (te::isOnTargetTrack (*other, t, 0))
                    [[maybe_unused]] auto res = other->removeTarget (t.itemID, &edit.getUndoManager());
            [[maybe_unused]] auto res = in->setTarget (t.itemID, true, &edit.getUndoManager(), 0);
            repaint();
        });
    }

    void renameTrack (te::AudioTrack& t)
    {
        auto* aw = new AlertWindow ("Renommer la piste", {}, MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", t.getName());
        aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([aw, tp = te::Track::Ptr (&t)] (int r)
        {
            if (r == 1) tp->setName (aw->getTextEditorContents ("name"));
        }), true);
    }

    void paintLane (Graphics& g, te::AudioTrack& t, Rectangle<int> r)
    {
        g.setColour (&t == selectedTrack ? Colour (0xff1a1c21) : Colour (0xff151619));
        g.fillRect (r);

        // grille
        auto t0 = xToTime (r.getX()), t1 = xToTime (r.getRight());
        auto b0 = (int) std::floor (edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (jmax (0.0, t0))).inBeats());
        auto b1 = (int) std::ceil (edit.tempoSequence.toBeats (te::TimePosition::fromSeconds (t1)).inBeats());
        auto beatPx = pixelsPerSecond * 60.0 / jmax (1.0, edit.tempoSequence.getBpmAt (te::TimePosition::fromSeconds (t0)));
        for (int b = b0; b <= b1; ++b)
        {
            auto x = timeToX (edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) b)).inSeconds());
            bool bar = edit.tempoSequence.toBarsAndBeats (edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) b))).getWholeBeats() == 0;
            if (! bar && beatPx < 10) continue;
            g.setColour (Colours::white.withAlpha (bar ? 0.075f : 0.028f));
            g.fillRect (x, (float) r.getY(), 1.0f, (float) r.getHeight());
        }

        g.setColour (Col::line);
        g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);

        auto col = t.getColour();
        for (auto* c : t.getClips())
        {
            auto pos = c->getPosition();
            auto x1 = timeToX (pos.getStart().inSeconds()), x2 = timeToX (pos.getEnd().inSeconds());
            if (x2 < r.getX() || x1 > r.getRight()) continue;

            auto cr = Rectangle<float> (x1, (float) r.getY() + 2, x2 - x1, (float) r.getHeight() - 5);
            bool sel = c == selectedClip.get();

            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r);
            Path clipPath; clipPath.addRoundedRectangle (cr, 3.0f);
            g.reduceClipRegion (clipPath);

            g.setColour (col.withAlpha (0.20f));
            g.fillRect (cr);
            auto hdr = cr.removeFromTop (14);
            g.setColour (col);
            g.fillRect (hdr);
            g.setColour (Colour (0xd90a0a0c));
            g.setFont (uiFont (11.0f, true));
            g.drawText (c->getName(), hdr.reduced (5, 0), Justification::centredLeft, true);
            if (c->state.hasProperty ("chenOriginalSource") && hdr.getWidth() > 90)   // voix corrigée par VariVox
            {
                auto badge = hdr.removeFromRight (24).reduced (2, 2);
                g.setColour (Colour (0xd90a0a0c));
                g.fillRoundedRectangle (badge.toFloat(), 2.0f);
                g.setColour (Col::accent);
                g.setFont (uiFont (8.5f, true));
                g.drawText ("VV", badge, Justification::centred);
            }

            if (auto* mc = dynamic_cast<te::MidiClip*> (c))
                drawMidiNotes (g, *mc, cr.reduced (0, 3), col);

            if (auto* wac = dynamic_cast<te::WaveAudioClip*> (c))
            {
                drawWaveform (g, *wac, cr.reduced (0, 2), col);
                drawFadesAndGain (g, *wac, Rectangle<float> (x1, (float) r.getY() + 2, x2 - x1, (float) r.getHeight() - 5), hdr, sel);
            }

            g.setColour (sel ? Colours::white : col.darker (0.35f));
            g.drawRoundedRectangle (Rectangle<float> (x1, (float) r.getY() + 2, x2 - x1, (float) r.getHeight() - 5).reduced (0.5f), 3.0f, sel ? 1.5f : 1.0f);
        }

        if (automationView)
            drawAutomation (g, t, r);

        // prise en cours d'enregistrement
        if (edit.getTransport().isRecording() && EngineHelpers::isTrackArmed (t))
        {
            auto start = edit.getTransport().getTimeWhenStarted().inSeconds();
            auto x1 = timeToX (start), x2 = timeToX (edit.getTransport().getPosition().inSeconds());
            g.setColour (Col::rec.withAlpha (0.35f));
            g.fillRoundedRectangle (x1, (float) r.getY() + 2, jmax (2.0f, x2 - x1), (float) r.getHeight() - 5, 3);
            g.setColour (Col::rec);
            g.drawRoundedRectangle (x1, (float) r.getY() + 2, jmax (2.0f, x2 - x1), (float) r.getHeight() - 5, 3, 1);
        }
    }

    //==========================================================================
    // Couloirs de comping
    te::AudioTrack* compTrack = nullptr;
    int compLane = -1;
    double compT1 = 0, compT2 = 0;
    std::map<String, std::unique_ptr<te::SmartThumbnail>> laneThumbs;

    void laneMouseDown (te::AudioTrack& t, const MouseEvent& e)
    {
        auto idx = (e.y - trackTop (t) - mainH) / laneH;
        auto lanes = Comping::getLanes (t);
        if (idx < 0 || idx >= (int) lanes.size()) return;
        auto& lane = lanes[(size_t) idx];
        selectedTrack = &t;

        if (e.x < headerW)
        {
            // ▲ : prise entière ; clic sur le nom : bascule sur cette playlist
            if (e.x > headerW - 40)
            {
                double s0 = 1e9, s1 = 0;
                for (auto& c : lane.clips) { s0 = jmin (s0, c.start); s1 = jmax (s1, c.start + c.length); }
                if (s1 > s0) Comping::promote (t, lane, s0, s1);
            }
            else if (e.mods.isPopupMenu() || e.getNumberOfClicks() > 1)
            {
                Playlists::switchTo (t, lane.name);
            }
            repaint();
            return;
        }

        compTrack = &t;
        compLane = idx;
        compT1 = compT2 = transportBar.mode == TransportBar::Mode::grid ? snap (xToTime (e.x)) : xToTime (e.x);
        repaint();
    }

    void paintCompLanes (Graphics& g, te::AudioTrack& t, Rectangle<int> area)
    {
        auto lanes = Comping::getLanes (t);
        auto col = t.getColour();
        for (int i = 0; i < (int) lanes.size(); ++i)
        {
            auto row = Rectangle<int> (0, area.getY() + i * laneH, getWidth(), laneH);
            auto& lane = lanes[(size_t) i];

            // en-tête du couloir
            auto hr = row.withWidth (headerW);
            g.setColour (Colour (0xff191a1d));
            g.fillRect (hr);
            g.setColour (col.withAlpha (0.5f));
            g.fillRect (hr.getX(), hr.getY(), 5, hr.getHeight() - 1);
            g.setColour (Col::text2);
            g.setFont (uiFont (11.5f));
            g.drawText (lane.name, hr.getX() + 30, hr.getY(), headerW - 80, laneH, Justification::centredLeft, true);
            drawPill (g, { (float) headerW - 34, hr.getY() + 13.0f, 24, 18 }, String::fromUTF8 ("▲"), false, Col::accent);

            // zone de la prise
            auto lr = row.withTrimmedLeft (headerW);
            g.setColour (Colour (0xff131417));
            g.fillRect (lr);
            for (auto& c : lane.clips)
            {
                auto x1 = timeToX (c.start), x2 = timeToX (c.start + c.length);
                if (x2 < lr.getX() || x1 > lr.getRight()) continue;
                auto cr = Rectangle<float> (jmax ((float) lr.getX(), x1), lr.getY() + 3.0f, jmin ((float) lr.getRight(), x2) - jmax ((float) lr.getX(), x1), laneH - 6.0f);
                g.setColour (col.withAlpha (0.12f));
                g.fillRoundedRectangle (cr, 2.0f);

                auto& th = laneThumbs[c.file.getFullPathName()];
                if (th == nullptr && c.file.existsAsFile())
                    th = std::make_unique<te::SmartThumbnail> (edit.engine, te::AudioFile (edit.engine, c.file), *this, &edit);
                if (th != nullptr && ! th->isGeneratingProxy() && ! th->isOutOfDate())
                {
                    auto visStart = xToTime ((int) cr.getX()), visEnd = xToTime ((int) cr.getRight());
                    auto src0 = c.offset + (visStart - c.start), src1 = c.offset + (visEnd - c.start);
                    g.setColour (col.withAlpha (0.55f));
                    th->drawChannel (g, cr.reduced (0, 2).toNearestInt(), { te::TimePosition::fromSeconds (src0), te::TimePosition::fromSeconds (src1) }, 0, 1.0f);
                }
                g.setColour (col.withAlpha (0.4f));
                g.drawRoundedRectangle (Rectangle<float> (x1, lr.getY() + 3.0f, x2 - x1, laneH - 6.0f), 2.0f, 1.0f);
            }

            // sélection en cours
            if (compTrack == &t && compLane == i)
            {
                auto a = timeToX (jmin (compT1, compT2)), b = timeToX (jmax (compT1, compT2));
                g.setColour (Col::select.withAlpha (0.3f));
                g.fillRect (a, (float) lr.getY() + 2, jmax (1.0f, b - a), (float) laneH - 4);
                g.setColour (Col::select);
                g.drawRect (a, (float) lr.getY() + 2, jmax (1.0f, b - a), (float) laneH - 4, 1.0f);
            }

            g.setColour (Col::line);
            g.fillRect (0, row.getBottom() - 1, getWidth(), 1);
        }
    }

    //==========================================================================
    // Automation du volume (courbe sur la piste)
    te::AutomatableParameter::Ptr autoDragParam;
    int autoDragIndex = -1;
    te::AudioTrack* autoDragTrack = nullptr;

    Rectangle<float> automationArea (te::Track& t) const
    {
        return { (float) headerW, trackTop (t) + 6.0f, (float) getWidth() - headerW, mainH - 12.0f };
    }

    float valueToY (Rectangle<float> a, float v) const  { return a.getBottom() - jlimit (0.0f, 1.0f, v) * a.getHeight(); }
    float yToValue (Rectangle<float> a, float y) const  { return jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight()); }

    te::AutomatableParameter* autoParam (te::AudioTrack& t) const { return Automation::shown (t); }

    static float norm (te::AutomatableParameter& p, float v)
    {
        auto r = p.getValueRange();
        return (v - r.getStart()) / jmax (0.0001f, r.getLength());
    }
    static float denorm (te::AutomatableParameter& p, float n)
    {
        auto r = p.getValueRange();
        return r.getStart() + jlimit (0.0f, 1.0f, n) * r.getLength();
    }

    void drawAutomation (Graphics& g, te::AudioTrack& t, Rectangle<int> lane)
    {
        auto* param = autoParam (t);
        if (param == nullptr) return;
        auto& curve = param->getCurve();
        auto a = automationArea (t);

        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRect (lane);

        Path p;
        for (int x = lane.getX(); x <= lane.getRight(); x += 2)
        {
            auto time = te::TimePosition::fromSeconds (jmax (0.0, xToTime (x)));
            auto val = curve.getNumPoints() > 0 ? curve.getValueAt (time, param->getCurrentValue()) : param->getCurrentValue();
            auto y = valueToY (a, norm (*param, val));
            if (x == lane.getX()) p.startNewSubPath ((float) x, y); else p.lineTo ((float) x, y);
        }
        g.setColour (Col::accent);
        g.strokePath (p, PathStrokeType (1.6f));

        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            auto x = timeToX (curve.getPointTime (i).inSeconds());
            auto y = valueToY (a, norm (*param, curve.getPointValue (i)));
            g.setColour (Col::bg);
            g.fillRect (x - 3, y - 3, 6.0f, 6.0f);
            g.setColour (Col::accent);
            g.drawRect (x - 3, y - 3, 6.0f, 6.0f, 1.2f);
        }

        // étiquette : paramètre affiché (clic : choisir), valeur, autres paramètres automatisés
        auto name = Automation::displayName (t, *param);
        auto label = String::fromUTF8 ("▾ ") + name + "   " + Automation::valueText (t, *param);
        g.setFont (uiFont (10.5f, true));
        auto w = jmin (lane.getWidth() - 12, GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 14);
        auto tag = Rectangle<int> (lane.getX() + 4, lane.getY() + 3, w, 16);
        g.setColour (Colour (0xcc0c0d10));
        g.fillRoundedRectangle (tag.toFloat(), 3.0f);
        g.setColour (Col::accent);
        g.drawText (label, tag.reduced (6, 0), Justification::centredLeft, true);
        int others = Automation::countAutomated (t) - (curve.getNumPoints() > 0 ? 1 : 0);
        if (others > 0)
        {
            auto txt = String::fromUTF8 ("+ ") + String (others) + String::fromUTF8 (others > 1 ? " autres paramètres automatisés" : " autre paramètre automatisé");
            g.setFont (uiFont (10.0f));
            auto r2 = Rectangle<int> (tag.getRight() + 4, tag.getY(), GlyphArrangement::getStringWidthInt (g.getCurrentFont(), txt) + 12, 16);
            g.setColour (Colour (0xcc0c0d10));
            g.fillRoundedRectangle (r2.toFloat(), 3.0f);
            g.setColour (Col::text2);
            g.drawText (txt, r2.reduced (6, 0), Justification::centredLeft);
        }
    }

    void automationMouseDown (te::AudioTrack& t, const MouseEvent& e)
    {
        auto* param = autoParam (t);
        if (param == nullptr) return;

        // clic sur l'étiquette : choisir le paramètre (volume, pan, envois, plugins)
        if (e.x < headerW + 240 && e.y < trackTop (t) + 22)
        {
            Automation::showMenu (t, [sp = SafePointer<ArrangeView> (this)] { if (sp) sp->repaint(); });
            return;
        }

        auto& curve = param->getCurve();
        auto a = automationArea (t);
        auto* um = &edit.getUndoManager();

        // point existant ?
        for (int i = 0; i < curve.getNumPoints(); ++i)
        {
            auto x = timeToX (curve.getPointTime (i).inSeconds());
            auto y = valueToY (a, norm (*param, curve.getPointValue (i)));
            if (std::abs (x - e.position.x) < 6 && std::abs (y - e.position.y) < 6)
            {
                if (e.mods.isAltDown() || e.mods.isPopupMenu()) { curve.removePoint (i, um); return; }
                autoDragParam = param;
                autoDragIndex = i;
                autoDragTrack = &t;
                return;
            }
        }

        if (e.mods.isAltDown() || e.mods.isPopupMenu()) return;

        // nouveau point
        auto time = te::TimePosition::fromSeconds (jmax (0.0, xToTime (e.x)));
        if (transportBar.mode == TransportBar::Mode::grid) time = te::TimePosition::fromSeconds (snap (time.inSeconds()));
        if (curve.getNumPoints() == 0)   // ancre au début, pour garder le niveau actuel avant le premier point
            curve.addPoint (te::TimePosition(), param->getCurrentValue(), 0.0f, um);
        autoDragIndex = curve.addPoint (time, denorm (*param, yToValue (a, e.position.y)), 0.0f, um);
        autoDragParam = param;
        autoDragTrack = &t;
    }

    void automationDrag (const MouseEvent& e)
    {
        if (autoDragTrack == nullptr || autoDragIndex < 0) return;
        auto a = automationArea (*autoDragTrack);
        auto time = te::TimePosition::fromSeconds (jmax (0.0, xToTime (e.x)));
        if (transportBar.mode == TransportBar::Mode::grid) time = te::TimePosition::fromSeconds (snap (time.inSeconds()));
        autoDragIndex = autoDragParam->getCurve().movePoint (*autoDragParam, autoDragIndex, time, denorm (*autoDragParam, yToValue (a, e.position.y)), false, &edit.getUndoManager());
        repaint();
    }

    void drawMidiNotes (Graphics& g, te::MidiClip& c, Rectangle<float> area, Colour col)
    {
        auto& seq = c.getSequence();
        if (seq.getNumNotes() == 0) return;
        auto range = seq.getNoteNumberRange();
        auto lo = range.getStart() - 1, hi = jmax (range.getEnd() + 1, lo + 12);
        auto noteH = jmax (1.5f, area.getHeight() / (float) (hi - lo + 1));
        g.setColour (col.brighter (0.4f));
        for (auto* n : seq.getNotes())
        {
            auto t1 = c.getTimeOfContentBeat (n->getBeatPosition()).inSeconds();
            auto t2 = c.getTimeOfContentBeat (n->getBeatPosition() + n->getLengthBeats()).inSeconds();
            auto x1 = timeToX (t1), x2 = timeToX (t2);
            auto y = area.getBottom() - (n->getNoteNumber() - lo + 1) * noteH;
            g.fillRect (x1, y, jmax (1.5f, x2 - x1 - 1), jmax (1.5f, noteH - 0.5f));
        }
    }

    void drawFadesAndGain (Graphics& g, te::AudioClipBase& c, Rectangle<float> r, Rectangle<float> hdr, bool sel)
    {
        auto body = r.withTrimmedTop (hdr.getHeight());
        auto fin  = (float) (c.getFadeIn().inSeconds() * pixelsPerSecond);
        auto fout = (float) (c.getFadeOut().inSeconds() * pixelsPerSecond);

        auto drawFade = [&] (bool in, float w)
        {
            if (w < 1.0f) return;
            Path shade, curve;
            auto x0 = in ? body.getX() : body.getRight() - w;
            auto xA = in ? x0 : x0 + w, xB = in ? x0 + w : x0;   // xA = silence, xB = plein niveau
            shade.startNewSubPath (xA, body.getBottom());
            for (int i = 0; i <= 24; ++i)
            {
                auto t = i / 24.0f;
                auto gain = std::sin (t * MathConstants<float>::halfPi);   // courbe à puissance constante
                auto x = xA + (xB - xA) * t;
                auto y = body.getBottom() - body.getHeight() * gain;
                if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
                shade.lineTo (x, y);
            }
            shade.lineTo (xB, body.getY());
            shade.lineTo (xA, body.getY());
            shade.closeSubPath();
            g.setColour (Colours::black.withAlpha (0.35f));
            g.fillPath (shade);
            g.setColour (Colours::white.withAlpha (0.75f));
            g.strokePath (curve, PathStrokeType (1.2f));
        };
        drawFade (true, fin);
        drawFade (false, fout);

        // poignées de fondu (coins hauts), visibles sur le clip sélectionné
        if (sel)
        {
            g.setColour (Colours::white);
            g.fillRect (r.getX() + fin - 3, r.getY() + 1, 6.0f, 6.0f);
            g.fillRect (r.getRight() - fout - 3, r.getY() + 1, 6.0f, 6.0f);
        }

        // gain de clip
        auto db = c.getGainDB();
        if (std::abs (db) > 0.05f)
        {
            g.setColour (Colour (0xd90a0a0c));
            g.setFont (uiFont (10.5f, true));
            g.drawText (dbString (db) + " dB", hdr.reduced (5, 0), Justification::centredRight, false);
            auto y = body.getBottom() - body.getHeight() * jlimit (0.05f, 0.95f, dbToFaderPos (db));
            g.setColour (Colours::white.withAlpha (0.55f));
            g.drawHorizontalLine ((int) y, body.getX(), body.getRight());
        }
    }

    void drawWaveform (Graphics& g, te::WaveAudioClip& c, Rectangle<float> area, Colour col)
    {
        auto& entry = thumbs[c.itemID];
        te::AudioFile af (c.getAudioFile());   // fichier source : stable même avec fondus et gain
        if (entry.thumb == nullptr)
            entry.thumb = std::make_unique<te::SmartThumbnail> (edit.engine, af, *this, &edit);
        else if (entry.file != af.getFile())
            entry.thumb->setNewFile (af);
        entry.file = af.getFile();
        auto& thumb = entry.thumb;

        if (thumb->isGeneratingProxy() || thumb->isOutOfDate()) return;

        auto pos = c.getPosition();
        auto visStart = jmax (pos.getStart().inSeconds(), xToTime ((int) area.getX()));
        auto visEnd   = jmin (pos.getEnd().inSeconds(),   xToTime ((int) area.getRight()));
        if (visEnd <= visStart) return;

        auto x1 = timeToX (visStart), x2 = timeToX (visEnd);
        auto speed = c.getSpeedRatio();
        auto srcStart = (visStart - pos.getStart().inSeconds() + pos.getOffset().inSeconds()) * speed;
        auto srcEnd   = (visEnd   - pos.getStart().inSeconds() + pos.getOffset().inSeconds()) * speed;
        auto draw = area.withX (x1).withRight (x2).toNearestInt();

        g.setColour (col.withAlpha (0.9f));
        auto gain = c.getGain();
        if (thumb->getNumChannels() > 1)
        {
            auto top = draw.removeFromTop (draw.getHeight() / 2);
            thumb->drawChannel (g, top,  { te::TimePosition::fromSeconds (srcStart), te::TimePosition::fromSeconds (srcEnd) }, 0, gain);
            thumb->drawChannel (g, draw, { te::TimePosition::fromSeconds (srcStart), te::TimePosition::fromSeconds (srcEnd) }, 1, gain);
        }
        else
        {
            thumb->drawChannel (g, draw, { te::TimePosition::fromSeconds (srcStart), te::TimePosition::fromSeconds (srcEnd) }, 0, gain);
        }
    }

    //==========================================================================
    void timerCallback() override
    {
        auto& tr = edit.getTransport();

        // défilement automatique pendant la lecture
        if (tr.isPlaying())
        {
            auto px = timeToX (tr.getPosition().inSeconds());
            if (px > getWidth() - 60 || px < headerW)
            {
                scrollSeconds = jmax (0.0, tr.getPosition().inSeconds() - 1.0);
                updateScrollbars();
            }
        }

        // mètres des pistes
        for (auto* t : te::getAudioTracks (edit))
        {
            auto& client = meterClients[t->itemID];
            if (auto* lm = t->getLevelMeterPlugin())
                if (! attached.contains (t->itemID.getRawID()))
                {
                    lm->measurer.addClient (client);
                    attached.add (t->itemID.getRawID());
                }
            auto l = jmax (client.getAndClearAudioLevel (0).dB, client.getAndClearAudioLevel (1).dB);
            auto& cur = meterLevels[t->itemID];
            cur = jmax (l, cur - 1.5f);
        }

        if (wasRecording && ! tr.isRecording())
            triggerAsyncUpdate();
        wasRecording = tr.isRecording();

        repaint();
    }

    Array<uint64> attached;

    void handleAsyncUpdate() override
    {
        updateScrollbars();
        repaint();
    }

    void valueTreeChildAdded (ValueTree&, ValueTree&) override        { triggerAsyncUpdate(); }
    void valueTreePropertyChanged (ValueTree&, const Identifier& id) override
    {
        if (id == Comping::showLanesID) triggerAsyncUpdate();
    }
    void valueTreeChildRemoved (ValueTree&, ValueTree& c, int) override
    {
        if (selectedClip != nullptr && ! selectedClip->state.getParent().isValid())
        {
            selectedClip = nullptr;
            notifySelection();
        }
        if (te::TrackList::isTrack (c))
        {
            // les mesures sont rattachées à des plugins qui disparaissent avec la piste
            Array<uint64> stillThere;
            for (auto* t : te::getAudioTracks (edit))
                if (attached.contains (t->itemID.getRawID())) stillThere.add (t->itemID.getRawID());
            attached = stillThere;
            if (selectedTrack != nullptr && ! te::getAudioTracks (edit).contains (selectedTrack))
                selectedTrack = nullptr;
        }
        triggerAsyncUpdate();
    }
};

} // namespace chen
