#pragma once

// =============================================================================
//  Inspecteur (à gauche de l'Édition), façon Cubase :
//  piste sélectionnée, playlist, entrée, clip sélectionné, tranche complète.
// =============================================================================

namespace chen
{

class Inspector : public Component, private Timer
{
public:
    Inspector (te::Edit& e, ArrangeView& a) : edit (e), arrange (a)
    {
        addChildComponent (clipGain);
        clipGain.setSliderStyle (Slider::LinearHorizontal);
        clipGain.setTextBoxStyle (Slider::TextBoxRight, false, 64, 18);
        clipGain.setRange (-24.0, 24.0, 0.1);
        clipGain.setDoubleClickReturnValue (true, 0.0);
        clipGain.setTextValueSuffix (" dB");
        clipGain.onValueChange = [this]
        {
            if (auto* c = dynamic_cast<te::AudioClipBase*> (arrange.getSelectedClip()))
                c->setGainDB ((float) clipGain.getValue());
            arrange.repaint();
        };
        addChildComponent (playlistAnchor);
        startTimerHz (8);
    }

    static constexpr int width = 236;

    void refresh()
    {
        auto* t = arrange.getSelectedTrack();
        if (t != track.get())
        {
            track = t;
            strip = nullptr;
            if (t != nullptr)
            {
                strip = std::make_unique<ChannelStrip> (*t, te::getAudioTracks (edit).indexOf (t) + 1);
                addAndMakeVisible (*strip);
            }
        }

        auto* c = dynamic_cast<te::AudioClipBase*> (arrange.getSelectedClip());
        clipGain.setVisible (c != nullptr);
        if (c != nullptr && ! clipGain.isMouseButtonDown())
            clipGain.setValue (c->getGainDB(), dontSendNotification);

        resized();
        repaint();
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Col::panel1);
        g.setColour (Col::line);
        g.fillRect (getWidth() - 1, 0, 1, getHeight());

        if (track == nullptr)
        {
            g.setColour (Col::text3);
            g.setFont (uiFont (12.5f));
            g.drawFittedText (String::fromUTF8 ("Sélectionne une piste\npour l'inspecter"), getLocalBounds().reduced (20), Justification::centred, 3);
            return;
        }

        // en-tête couleur
        auto head = Rectangle<int> (0, 0, getWidth() - 1, 30);
        g.setGradientFill (ColourGradient (track->getColour().brighter (0.1f), 0, 0, track->getColour().darker (0.15f), 0, 30, false));
        g.fillRect (head);
        g.setColour (Colour (0xff1a0f0e));
        g.setFont (uiFont (14.0f, true));
        g.drawText (track->getName(), head.reduced (10, 0), Justification::centredLeft, true);
        g.setFont (uiFont (11.0f));
        g.drawText ("Audio", head.reduced (10, 0), Justification::centredRight);

        int y = 34;
        auto row = [&] (const String& label, const String& value, Colour vc = Col::text)
        {
            g.setColour (Col::text2);
            g.setFont (uiFont (11.5f));
            g.drawText (label, 8, y, 64, 20, Justification::centredLeft);
            Rectangle<float> f (74.0f, (float) y + 1, getWidth() - 82.0f, 18.0f);
            g.setColour (Colour (0xff141518));
            g.fillRoundedRectangle (f, 2);
            g.setColour (Col::line);
            g.drawRoundedRectangle (f, 2, 1);
            g.setColour (vc);
            g.drawText (value, f.reduced (6, 0), Justification::centredLeft, true);
            y += 22;
        };

        String input = String::fromUTF8 ("Aucune");
        for (auto* in : edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, *track, 0)) input = in->getInputDevice().getName();

        row ("Playlist", String::fromUTF8 ("▾ ") + Playlists::getCurrent (*dynamic_cast<te::ClipTrack*> (track.get())), Col::accent);
        row (String::fromUTF8 ("Entrée"), String::fromUTF8 ("▾ ") + input);
        row ("Sortie", String::fromUTF8 ("Stéréo Out"));

        // clip sélectionné
        y += 4;
        g.setColour (Col::panel2);
        g.fillRect (0, y, getWidth() - 1, 20);
        g.setColour (Col::text2);
        g.setFont (uiFont (10.5f, true));
        g.drawText (String::fromUTF8 ("CLIP SÉLECTIONNÉ"), 8, y, getWidth() - 16, 20, Justification::centredLeft);
        y += 24;

        if (auto* c = arrange.getSelectedClip())
        {
            auto pos = c->getPosition();
            auto bb = edit.tempoSequence.toBarsAndBeats (pos.getStart());
            row ("Nom", c->getName());
            row (String::fromUTF8 ("Début"), String (bb.bars + 1) + ". " + String ((int) std::floor (bb.beats.inBeats()) + 1)
                                              + "   (" + String (pos.getStart().inSeconds(), 3) + " s)");
            row ("Longueur", String (pos.getLength().inSeconds(), 3) + " s");
            if (auto* a = dynamic_cast<te::AudioClipBase*> (c))
                row ("Fondus", String (roundToInt (a->getFadeIn().inSeconds() * 1000)) + String::fromUTF8 (" ms  ·  ")
                               + String (roundToInt (a->getFadeOut().inSeconds() * 1000)) + " ms");
            g.setColour (Col::text2);
            g.setFont (uiFont (11.5f));
            g.drawText ("Gain", 8, y, 64, 20, Justification::centredLeft);
        }
        else
        {
            g.setColour (Col::text3);
            g.setFont (uiFont (11.5f));
            g.drawText (String::fromUTF8 ("Aucun clip sélectionné"), 8, y, getWidth() - 16, 20, Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto clipRows = arrange.getSelectedClip() != nullptr ? 4 + (dynamic_cast<te::AudioClipBase*> (arrange.getSelectedClip()) ? 1 : 0) : 0;
        auto gainY = 34 + 3 * 22 + 4 + 24 + clipRows * 22;
        clipGain.setBounds (70, gainY, getWidth() - 76, 20);
        auto stripTop = gainY + (arrange.getSelectedClip() ? 28 : 26);
        if (strip) strip->setBounds (0, stripTop, getWidth() - 1, jmax (520, getHeight() - stripTop));
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (track == nullptr) return;
        auto y = e.y;
        if (y >= 34 && y < 56)      { playlistAnchor.setBounds (74, 35, getWidth() - 82, 18); Playlists::showMenu (*dynamic_cast<te::ClipTrack*> (track.get()), &playlistAnchor); }
        else if (y >= 56 && y < 78) { if (auto* at = dynamic_cast<te::AudioTrack*> (track.get())) arrange.openInputMenu (*at); }
    }

private:
    te::Edit& edit;
    ArrangeView& arrange;
    te::Track::Ptr track;
    std::unique_ptr<ChannelStrip> strip;
    Slider clipGain;
    Component playlistAnchor;

    void timerCallback() override
    {
        if (track != nullptr && ! te::getAudioTracks (edit).contains (dynamic_cast<te::AudioTrack*> (track.get())))
        {
            strip = nullptr;
            track = nullptr;
        }
        refresh();
    }
};

} // namespace chen
