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

        // marque
        if (pistache == nullptr) pistache = createPistache();
        if (pistache != nullptr)
            pistache->drawWithin (g, Rectangle<float> (12, (getHeight() - 34) / 2.0f, 34, 34), RectanglePlacement::centred, 1.0f);
        g.setFont (uiFont (17.0f, true));
        g.setColour (Colour (0xffe6e8ec));
        g.drawText (String::fromUTF8 ("Chèn"), 52, 8, 60, 24, Justification::centredLeft);
        g.setColour (Col::accent);
        g.drawText ("Rec", 94, 8, 40, 24, Justification::centredLeft);
        g.setColour (Colour (0xffe6e8ec));
        g.drawText ("Pro", 127, 8, 40, 24, Justification::centredLeft);
        g.setColour (Col::text3);
        g.setFont (uiFont (9.5f, true));
        g.drawText (String::fromUTF8 ("STUDIO DES CHÈNEVIÈRES"), 52, 30, 160, 14, Justification::centredLeft);

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
                case 5: EngineHelpers::toggleRecord (edit); break;
                case 6: tr.looping = ! tr.looping.get(); break;
                case 7: edit.clickTrackEnabled = ! edit.clickTrackEnabled.get(); break;
                default: break;
            }
            repaint();
            return;
        }
    }

private:
    te::Edit& edit;
    std::unique_ptr<Drawable> pistache;
    static constexpr int numTransportButtons = 8;

    void timerCallback() override { repaint(); }

    static void group (Graphics& g, Rectangle<int> r)
    {
        g.setColour (Colour (0xff17181b));
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (Colour (0xff0b0c0e));
        g.drawRoundedRectangle (r.toFloat(), 5.0f, 1.0f);
    }

    Rectangle<int> modesRect() const          { return { 210, (getHeight() - 34) / 2, 260, 34 }; }
    Rectangle<int> modeRect (int i) const     { auto r = modesRect().reduced (3); return r.withWidth (r.getWidth() / 4).translated (i * r.getWidth() / 4, 0); }
    Rectangle<int> transportRect() const      { return { getWidth() / 2 - 420, (getHeight() - 34) / 2, numTransportButtons * 34 + 6, 34 }; }
    Rectangle<int> transportButtonRect (int i) const { auto r = transportRect(); return { r.getX() + 3 + i * 34, r.getY() + 3, 32, 28 }; }
    Rectangle<int> lcdRect() const            { return { transportRect().getRight() + 12, (getHeight() - 46) / 2, 520, 46 }; }
    Rectangle<int> cpuRect() const            { return { getWidth() - 170, (getHeight() - 20) / 2, 156, 20 }; }

    void drawTransportButton (Graphics& g, int i, Rectangle<int> r, te::TransportControl& tr)
    {
        bool on = (i == 4 && tr.isPlaying()) || (i == 5 && tr.isRecording())
               || (i == 6 && tr.looping.get()) || (i == 7 && edit.clickTrackEnabled.get());
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

    static constexpr int headerW = 250, rulerH = 46, markerH = 18;

    //==========================================================================
    void paint (Graphics& g) override
    {
        auto tracks = te::getAudioTracks (edit);
        auto lanes = laneArea();

        g.fillAll (Col::bg);

        // ---- pistes
        {
            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (0, rulerH, getWidth(), lanes.getHeight());
            int y = rulerH - scrollY();
            int n = 1;
            for (auto* t : tracks)
            {
                auto h = trackHeight (*t);
                Rectangle<int> row (0, y, getWidth(), h);
                if (row.getBottom() >= rulerH && row.getY() <= getHeight())
                {
                    paintLane (g, *t, row.withTrimmedLeft (headerW));
                    paintHeader (g, *t, n, row.withWidth (headerW));
                }
                y += h; ++n;
            }
            g.setColour (Colour (0xff121315));
            g.fillRect (headerW, y, getWidth() - headerW, getHeight() - y);
            g.setColour (Col::panel1);
            g.fillRect (0, y, headerW, getHeight() - y);
        }

        paintRuler (g);

        // ---- curseur de lecture
        auto px = timeToX (edit.getTransport().getPosition().inSeconds());
        if (px >= headerW)
        {
            g.setColour (Col::accent.withAlpha (0.15f));
            g.fillRect ((float) px - 3, (float) rulerH, 6.0f, (float) getHeight());
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
        vScroll.setBounds (getWidth() - 12, rulerH, 12, getHeight() - rulerH - 12);
        updateScrollbars();
    }

    //==========================================================================
    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus();
        auto p = e.getPosition();

        if (p.y < rulerH && p.x > headerW)
        {
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (snap (xToTime (p.x))));
            draggingPlayhead = true;
            return;
        }

        auto* t = trackAt (p.y);
        if (t == nullptr) { selectedClip = nullptr; repaint(); return; }

        if (p.x < headerW)
        {
            headerClick (*t, p);
            return;
        }

        selectedTrack = t;
        selectedClip = clipAt (*t, xToTime (p.x));
        if (selectedClip != nullptr)
        {
            dragStartTime = selectedClip->getPosition().getStart().inSeconds();
            dragMouseTime = xToTime (p.x);
        }
        else
        {
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (snap (xToTime (p.x))));
        }
        repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (draggingPlayhead)
        {
            edit.getTransport().setPosition (te::TimePosition::fromSeconds (snap (jmax (0.0, xToTime (e.x)))));
            return;
        }

        if (selectedClip != nullptr && e.getDistanceFromDragStart() > 3 && transportBar.mode != TransportBar::Mode::spot)
        {
            auto newStart = jmax (0.0, dragStartTime + xToTime (e.x) - dragMouseTime);
            if (transportBar.mode == TransportBar::Mode::grid) newStart = snap (newStart);
            selectedClip->setStart (te::TimePosition::fromSeconds (newStart), false, true);
            repaint();
        }
    }

    void mouseUp (const MouseEvent&) override { draggingPlayhead = false; }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        auto* t = trackAt (e.y);
        if (t != nullptr && e.x < headerW && e.y < trackTop (*t) + 26)
            renameTrack (*t);
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
        if ((k == KeyPress::deleteKey || k == KeyPress::backspaceKey) && selectedClip != nullptr)
        {
            selectedClip->removeFromParent();
            selectedClip = nullptr;
            repaint();
            return true;
        }
        return false;
    }

    //==========================================================================
    bool isInterestedInFileDrag (const StringArray& files) override
    {
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
            importFile (*t, File (f), start);
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

    te::AudioTrack* getSelectedTrack() const { return selectedTrack; }

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
    double dragStartTime = 0, dragMouseTime = 0;
    bool draggingPlayhead = false, dropHighlight = false;
    struct ThumbEntry { File file; std::unique_ptr<te::SmartThumbnail> thumb; };
    std::map<te::EditItemID, ThumbEntry> thumbs;
    std::map<te::EditItemID, te::LevelMeasurer::Client> meterClients;
    std::map<te::EditItemID, float> meterLevels;
    bool wasRecording = false;

    Rectangle<int> laneArea() const { return { headerW, rulerH, getWidth() - headerW - 12, getHeight() - rulerH - 12 }; }
    int scrollY() const             { return (int) vScroll.getCurrentRangeStart(); }
    int trackHeight (te::Track&) const { return 86; }

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
        int y = rulerH - scrollY();
        for (auto* tr : te::getAudioTracks (edit)) { if (tr == &t) return y; y += trackHeight (*tr); }
        return y;
    }

    te::AudioTrack* trackAt (int y) const
    {
        if (y < rulerH) return nullptr;
        int ty = rulerH - scrollY();
        for (auto* t : te::getAudioTracks (edit))
        {
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
        for (auto* t : te::getAudioTracks (edit)) total += trackHeight (*t);
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

    void paintHeader (Graphics& g, te::AudioTrack& t, int number, Rectangle<int> r)
    {
        bool sel = &t == selectedTrack;
        g.setGradientFill (ColourGradient (sel ? Colour (0xff2d3039) : Col::panel2, (float) r.getX(), 0,
                                           sel ? Colour (0xff23252b) : Col::panel1, (float) r.getRight(), 0, false));
        g.fillRect (r);
        g.setColour (Col::line);
        g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);
        g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());

        g.setColour (t.getColour());
        g.fillRect (r.getX(), r.getY(), 5, r.getHeight() - 1);

        g.setColour (Col::text3);
        g.setFont (uiFont (11.0f));
        g.drawText (String (number), r.getX() + 10, r.getY() + 6, 18, 18, Justification::centredLeft);
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
        for (auto* in : edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, 0)) input = in->getInputDevice().getName();
        g.setColour (Col::text3);
        g.setFont (uiFont (11.0f));
        g.drawText (input, r.getX() + 30, r.getY() + 27, r.getWidth() - 60, 14, Justification::centredLeft, true);

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
                if (i == 0) t.setMute (! t.isMuted (false));
                if (i == 1) t.setSolo (! t.isSolo (false));
                if (i == 2) EngineHelpers::armTrack (t, ! EngineHelpers::isTrackArmed (t));
                if (i == 3) EngineHelpers::enableInputMonitoring (t, ! EngineHelpers::isInputMonitoringEnabled (t));
            }
        }
        else if (p.y >= top + 44 && p.y <= top + 62 && p.x >= 30 && p.x <= headerW - 70)
        {
            if (auto* v = t.getVolumePlugin())
                v->setVolumeDb (faderPosToDb (jlimit (0.0f, 1.0f, (p.x - 30.0f) / (headerW - 100.0f))));
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

            if (auto* wac = dynamic_cast<te::WaveAudioClip*> (c))
                drawWaveform (g, *wac, cr.reduced (0, 2), col);

            g.setColour (sel ? Colours::white : col.darker (0.35f));
            g.drawRoundedRectangle (Rectangle<float> (x1, (float) r.getY() + 2, x2 - x1, (float) r.getHeight() - 5).reduced (0.5f), 3.0f, sel ? 1.5f : 1.0f);
        }

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

    void drawWaveform (Graphics& g, te::WaveAudioClip& c, Rectangle<float> area, Colour col)
    {
        auto& entry = thumbs[c.itemID];
        te::AudioFile af (c.getPlaybackFile());
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
    void valueTreeChildRemoved (ValueTree&, ValueTree& c, int) override
    {
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
