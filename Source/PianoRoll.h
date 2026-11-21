#pragma once

// =============================================================================
//  Éditeur clavier (zone basse de la fenêtre Édition), façon Cubase :
//  clavier à gauche, grille, notes, vélocités en bas, quantification.
// =============================================================================

namespace chen
{

class PianoRoll : public Component, private Timer
{
public:
    explicit PianoRoll (te::Edit& e) : edit (e)
    {
        setWantsKeyboardFocus (true);
        startTimerHz (25);
    }

    std::function<void()> onClose;

    void setClip (te::MidiClip* c)
    {
        clip = c;
        selected.clear();
        if (clip != nullptr)
        {
            // centre la vue sur les notes existantes
            auto range = clip->getSequence().getNoteNumberRange();
            topNote = clip->getSequence().getNumNotes() > 0 ? jmin (127, range.getEnd() + 8) : 72;
            scrollBeats = 0;
        }
        repaint();
    }

    te::MidiClip* getClip() const { return clip.get(); }

    //==========================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff17181b));

        // barre de titre
        auto title = getLocalBounds().removeFromTop (titleH);
        g.setGradientFill (ColourGradient (Col::panel2, 0, 0, Col::panel1, 0, (float) titleH, false));
        g.fillRect (title);
        g.setColour (Col::accent);
        g.setFont (uiFont (12.0f, true));
        g.drawText (String::fromUTF8 ("ÉDITEUR CLAVIER"), 10, 0, 140, titleH, Justification::centredLeft);
        g.setColour (Col::text);
        g.setFont (uiFont (12.5f));
        g.drawText (clip ? clip->getName() + String::fromUTF8 ("  ·  ") + (clip->getTrack() ? clip->getTrack()->getName() : String()) : String ("Aucun clip MIDI"),
                    150, 0, 400, titleH, Justification::centredLeft);
        g.setColour (Col::text3);
        g.setFont (uiFont (11.0f));
        g.drawText (String::fromUTF8 ("clic : ajouter  ·  glisser : déplacer / bord droit : longueur  ·  Alt+clic ou clic droit : supprimer  ·  Q : quantifier  ·  Échap : fermer"),
                    560, 0, getWidth() - 600, titleH, Justification::centredLeft);
        g.setColour (Col::text2);
        g.drawText (String::fromUTF8 ("✕"), getWidth() - 30, 0, 24, titleH, Justification::centred);

        if (clip == nullptr) return;

        auto grid = gridArea();
        auto vel = velocityArea();

        // clavier
        for (int n = bottomNote(); n <= topNote; ++n)
        {
            auto y = noteToY (n);
            bool black = isBlack (n);
            g.setColour (black ? Colour (0xff1d1e22) : Colour (0xffd6d8dc));
            g.fillRect (0.0f, y, (float) keysW, (float) noteH);
            g.setColour (Colour (0xff0c0d0f));
            g.fillRect (0.0f, y + noteH - 1, (float) keysW, 1.0f);
            if (n % 12 == 0)
            {
                g.setColour (Colour (0xff3a3c42));
                g.setFont (uiFont (10.0f, true));
                g.drawText ("C" + String (n / 12 - 2), 2, (int) y, keysW - 6, noteH, Justification::centredRight);
            }

            // fond de grille
            g.setColour (black ? Colour (0xff18191c) : Colour (0xff1d1e22));
            g.fillRect ((float) grid.getX(), y, (float) grid.getWidth(), (float) noteH);
            if (n % 12 == 0)
            {
                g.setColour (Colours::white.withAlpha (0.06f));
                g.fillRect ((float) grid.getX(), y + noteH - 1, (float) grid.getWidth(), 1.0f);
            }
        }

        // lignes de temps
        {
            Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (grid.getUnion (vel));
            auto b0 = (int) std::floor (scrollBeats), b1 = (int) std::ceil (scrollBeats + grid.getWidth() / pixelsPerBeat);
            for (int b = b0; b <= b1; ++b)
                for (int q = 0; q < 4; ++q)
                {
                    auto x = beatToX (b + q * 0.25);
                    bool bar = q == 0 && (b % 4 == 0);
                    g.setColour (Colours::white.withAlpha (bar ? 0.12f : q == 0 ? 0.06f : 0.025f));
                    g.fillRect (x, (float) grid.getY(), 1.0f, (float) (vel.getBottom() - grid.getY()));
                    if (bar)
                    {
                        g.setColour (Col::text2);
                        g.setFont (uiFont (10.5f, true));
                        g.drawText (String (b / 4 + 1), (int) x + 3, grid.getY() - 16, 40, 14, Justification::centredLeft);
                    }
                }

            // limite du clip
            auto len = clipLengthBeats();
            g.setColour (Colours::black.withAlpha (0.35f));
            auto endX = beatToX (len);
            g.fillRect (endX, (float) grid.getY(), (float) getWidth() - endX, (float) (vel.getBottom() - grid.getY()));
            g.setColour (Col::accent.withAlpha (0.6f));
            g.fillRect (endX, (float) grid.getY(), 1.5f, (float) grid.getHeight());

            // notes
            auto col = clip->getTrack() ? clip->getTrack()->getColour() : Col::accent;
            for (auto* n : clip->getSequence().getNotes())
            {
                auto r = noteRect (*n);
                if (r.getBottom() < grid.getY() || r.getY() > grid.getBottom()) continue;
                bool sel = selected.contains (n);
                auto v = n->getVelocity() / 127.0f;
                g.setColour (col.withMultipliedBrightness (0.55f + 0.6f * v));
                g.fillRoundedRectangle (r, 2.0f);
                g.setColour (sel ? Colours::white : Colours::black.withAlpha (0.6f));
                g.drawRoundedRectangle (r, 2.0f, sel ? 1.5f : 1.0f);

                // vélocité
                auto vx = r.getX();
                auto vh = (vel.getHeight() - 8) * v;
                g.setColour (sel ? Colours::white : col);
                g.fillRect (vx, vel.getBottom() - 4 - vh, 3.0f, vh);
                g.fillEllipse (vx - 2, vel.getBottom() - 4 - vh - 3, 7, 7);
            }

            // curseur de lecture
            auto ph = edit.getTransport().getPosition();
            if (clip->getPosition().time.contains (ph))
            {
                auto x = beatToX (clip->getContentBeatAtTime (ph).inBeats());
                g.setColour (Col::accent);
                g.fillRect (x, (float) grid.getY(), 1.5f, (float) (vel.getBottom() - grid.getY()));
            }
        }

        // séparation vélocités
        g.setColour (Col::line);
        g.fillRect (0, vel.getY() - 2, getWidth(), 2);
        g.setColour (Col::panel1);
        g.fillRect (0, vel.getY(), keysW, vel.getHeight());
        g.setColour (Col::text3);
        g.setFont (uiFont (10.0f, true));
        g.drawText (String::fromUTF8 ("VÉLOCITÉ"), 4, vel.getY() + 4, keysW - 8, 14, Justification::centredLeft);
    }

    //==========================================================================
    void mouseDown (const MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.y < titleH)
        {
            if (e.x > getWidth() - 34 && onClose) onClose();
            return;
        }
        if (clip == nullptr) return;

        auto p = e.position;
        auto& seq = clip->getSequence();
        auto* um = &edit.getUndoManager();

        if (velocityArea().contains (e.getPosition()))
        {
            draggingVelocity = true;
            setVelocityAt (p);
            return;
        }

        if (! gridArea().contains (e.getPosition()))
        {
            if (e.x < keysW) audition (yToNote (p.y));
            return;
        }

        auto* n = noteAt (p);

        if (n != nullptr && (e.mods.isAltDown() || e.mods.isPopupMenu()))
        {
            selected.removeFirstMatchingValue (n);
            seq.removeNote (*n, um);
            repaint();
            return;
        }

        if (n == nullptr)
        {
            // nouvelle note, calée sur la grille
            auto beat = std::floor (xToBeat (p.x) * 4.0) / 4.0;
            if (beat < 0 || beat >= clipLengthBeats()) return;   // hors du clip
            auto pitch = yToNote (p.y);
            n = seq.addNote (pitch, te::BeatPosition::fromBeats (beat), te::BeatDuration::fromBeats (lastLength), lastVelocity, 0, um);
            audition (pitch);
        }

        if (! e.mods.isShiftDown() && ! selected.contains (n)) selected.clear();
        selected.addIfNotAlreadyThere (n);

        dragNote = n;
        dragStartBeat = n->getBeatPosition().inBeats();
        dragStartLen  = n->getLengthBeats().inBeats();
        dragStartPitch = n->getNoteNumber();
        dragMouseBeat = xToBeat (p.x);
        dragMousePitch = yToNote (p.y);
        resizing = p.x > noteRect (*n).getRight() - 6;
        repaint();
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (clip == nullptr) return;
        auto* um = &edit.getUndoManager();

        if (draggingVelocity) { setVelocityAt (e.position); return; }
        if (dragNote == nullptr) return;

        if (resizing)
        {
            auto end = snapBeat (xToBeat (e.position.x));
            auto len = jmax (0.0625, end - dragStartBeat);
            dragNote->setStartAndLength (dragNote->getBeatPosition(), te::BeatDuration::fromBeats (len), um);
            lastLength = len;
        }
        else
        {
            auto db = snapBeat (dragStartBeat + xToBeat (e.position.x) - dragMouseBeat);
            auto newPitch = jlimit (0, 127, dragStartPitch + yToNote (e.position.y) - dragMousePitch);
            dragNote->setStartAndLength (te::BeatPosition::fromBeats (jmax (0.0, db)), dragNote->getLengthBeats(), um);
            if (newPitch != dragNote->getNoteNumber())
            {
                dragNote->setNoteNumber (newPitch, um);
                audition (newPitch);
            }
        }
        repaint();
    }

    void mouseUp (const MouseEvent&) override { dragNote = nullptr; draggingVelocity = false; resizing = false; }

    void mouseMove (const MouseEvent& e) override
    {
        if (clip == nullptr) return;
        auto* n = noteAt (e.position);
        setMouseCursor (n != nullptr && e.position.x > noteRect (*n).getRight() - 6 ? MouseCursor::LeftRightResizeCursor
                        : n != nullptr ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
    }

    void mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w) override
    {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
            pixelsPerBeat = jlimit (10.0, 400.0, pixelsPerBeat * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15));
        else if (e.mods.isShiftDown())
            scrollBeats = jmax (0.0, scrollBeats - w.deltaY * 8.0);
        else
            topNote = jlimit (20, 127, topNote + (w.deltaY > 0 ? 3 : -3));
        repaint();
    }

    bool keyPressed (const KeyPress& k) override
    {
        if (clip == nullptr) return false;
        auto& seq = clip->getSequence();
        auto* um = &edit.getUndoManager();

        if (k == KeyPress::escapeKey) { if (onClose) onClose(); return true; }

        if (k == KeyPress::deleteKey || k == KeyPress::backspaceKey)
        {
            for (auto* n : selected) seq.removeNote (*n, um);
            selected.clear();
            repaint();
            return true;
        }

        if (k.getKeyCode() == 'Q' || k.getKeyCode() == 'q')
        {
            auto& list = selected.isEmpty() ? seq.getNotes() : selected;
            for (auto* n : Array<te::MidiNote*> (list))
            {
                auto q = std::round (n->getBeatPosition().inBeats() * 4.0) / 4.0;
                n->setStartAndLength (te::BeatPosition::fromBeats (q), n->getLengthBeats(), um);
            }
            repaint();
            return true;
        }

        if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'A' || k.getKeyCode() == 'a'))
        {
            selected = seq.getNotes();
            repaint();
            return true;
        }

        if (k == KeyPress::upKey || k == KeyPress::downKey)
        {
            auto d = (k == KeyPress::upKey ? 1 : -1) * (k.getModifiers().isShiftDown() ? 12 : 1);
            for (auto* n : selected) n->setNoteNumber (jlimit (0, 127, n->getNoteNumber() + d), um);
            repaint();
            return true;
        }
        return false;
    }

private:
    te::Edit& edit;
    te::MidiClip::Ptr clip;
    Array<te::MidiNote*> selected;
    double pixelsPerBeat = 60.0, scrollBeats = 0;
    int topNote = 72, noteH = 12;
    static constexpr int titleH = 26, keysW = 56, velH = 70;
    te::MidiNote* dragNote = nullptr;
    double dragStartBeat = 0, dragStartLen = 0, dragMouseBeat = 0, lastLength = 0.5;
    int dragStartPitch = 60, dragMousePitch = 60, lastVelocity = 100;
    bool resizing = false, draggingVelocity = false;
    te::MPESourceID auditionSource = te::createUniqueMPESourceID();

    Rectangle<int> gridArea() const     { return { keysW, titleH + 18, getWidth() - keysW, getHeight() - titleH - 18 - velH - 2 }; }
    Rectangle<int> velocityArea() const { return { keysW, getHeight() - velH, getWidth() - keysW, velH }; }

    int bottomNote() const        { return jmax (0, topNote - gridArea().getHeight() / noteH); }
    float noteToY (int n) const   { return (float) gridArea().getY() + (topNote - n) * noteH; }
    int yToNote (float y) const   { return jlimit (0, 127, topNote - (int) std::floor ((y - gridArea().getY()) / noteH)); }
    float beatToX (double b) const { return (float) (keysW + (b - scrollBeats) * pixelsPerBeat); }
    double xToBeat (float x) const { return scrollBeats + (x - keysW) / pixelsPerBeat; }
    double snapBeat (double b) const { return std::round (b * 4.0) / 4.0; }   // double-croche

    double clipLengthBeats() const
    {
        auto pos = clip->getPosition();
        return (clip->getContentBeatAtTime (pos.getEnd()) - clip->getContentBeatAtTime (pos.getStart())).inBeats()
               + clip->getContentBeatAtTime (pos.getStart()).inBeats();
    }

    Rectangle<float> noteRect (te::MidiNote& n) const
    {
        auto x = beatToX (n.getBeatPosition().inBeats());
        auto w = jmax (3.0f, (float) (n.getLengthBeats().inBeats() * pixelsPerBeat));
        return { x, noteToY (n.getNoteNumber()) + 1, w, (float) noteH - 2 };
    }

    te::MidiNote* noteAt (Point<float> p) const
    {
        auto& notes = clip->getSequence().getNotes();
        for (int i = notes.size(); --i >= 0;)
            if (noteRect (*notes[i]).contains (p)) return notes[i];
        return nullptr;
    }

    static bool isBlack (int n) { auto k = n % 12; return k == 1 || k == 3 || k == 6 || k == 8 || k == 10; }

    void setVelocityAt (Point<float> p)
    {
        auto vel = velocityArea();
        auto v = jlimit (1, 127, roundToInt ((vel.getBottom() - 4 - p.y) / (vel.getHeight() - 8) * 127.0f));
        auto beat = xToBeat (p.x);
        auto* um = &edit.getUndoManager();
        for (auto* n : clip->getSequence().getNotes())
            if (std::abs (n->getBeatPosition().inBeats() - beat) < 6.0 / pixelsPerBeat
                && (selected.isEmpty() || selected.contains (n)))
                n->setVelocity (v, um);
        lastVelocity = v;
        repaint();
    }

    /** Fait entendre la note sur l'instrument de la piste. */
    void audition (int note)
    {
        if (clip == nullptr) return;
        if (auto* at = dynamic_cast<te::AudioTrack*> (clip->getTrack()))
        {
            at->injectLiveMidiMessage (MidiMessage::noteOn (1, note, (uint8) lastVelocity), auditionSource);
            auto tp = te::Track::Ptr (at);
            auto src = auditionSource;
            Timer::callAfterDelay (250, [tp, note, src]
            {
                if (auto* t2 = dynamic_cast<te::AudioTrack*> (tp.get()))
                    t2->injectLiveMidiMessage (MidiMessage::noteOff (1, note), src);
            });
        }
    }

    void timerCallback() override
    {
        if (clip != nullptr && ! clip->state.getParent().isValid())
            setClip (nullptr);
        if (edit.getTransport().isPlaying()) repaint();
    }
};

} // namespace chen
