#pragma once

// =============================================================================
//  VariVox intégré (zone basse de la fenêtre Édition), façon VariAudio :
//  la voix du clip est analysée en notes, on les corrige (hauteur, segments,
//  niveau) et le clip joue aussitôt la version corrigée dans le projet.
//  Le moteur et l'éditeur de notes sont ceux du plugin Chènevières VariVox.
// =============================================================================

namespace chen
{

//==============================================================================
/** Écoute d'une note au clic (transport arrêté), mixée dans la sortie audio. */
class VariVoxAuditionOutput : public AudioIODeviceCallback
{
public:
    explicit VariVoxAuditionOutput (AudioDeviceManager& d) : dm (d) { dm.addAudioCallback (this); }
    ~VariVoxAuditionOutput() override { dm.removeAudioCallback (this); }

    Audition audition;

    void audioDeviceAboutToStart (AudioIODevice* d) override
    {
        sampleRate = d->getCurrentSampleRate();
        audition.prepare (2, jmax (64, d->getCurrentBufferSizeSamples() * 2));
    }
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const*, int, float* const* out, int numOut, int n,
                                           const AudioIODeviceCallbackContext&) override
    {
        for (int c = 0; c < numOut; ++c)
            if (out[c] != nullptr) FloatVectorOperations::clear (out[c], n);
        const int nc = jmin (2, numOut);   // première paire de sorties (pas les 64 voies d'une carte MADI)
        if (nc <= 0) return;
        AudioBuffer<float> b (out, nc, n);
        audition.render (b, sampleRate);
    }

private:
    AudioDeviceManager& dm;
    double sampleRate = 48000.0;
};

//==============================================================================
class VariVoxKnob : public Component
{
public:
    VariVoxKnob (const char* l, const char* tip) : label (String::fromUTF8 (l))
    {
        slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (200);
        slider.setTooltip (String::fromUTF8 (tip));
        slider.setColour (Slider::rotarySliderFillColourId, Col::accent);
        slider.setColour (Slider::rotarySliderOutlineColourId, Colour (0xff2a2d33));
        slider.setColour (Slider::thumbColourId, Col::text);
        addAndMakeVisible (slider);
    }

    Slider slider;
    std::function<String (double)> format;

    void resized() override { slider.setBounds (getLocalBounds().withHeight (getHeight() - 14).reduced (4, 0)); }

    void paint (Graphics& g) override
    {
        g.setColour (isEnabled() ? Col::text2 : Col::text3);
        g.setFont (uiFont (9.5f, true));
        g.drawText (label, getLocalBounds().removeFromBottom (14), Justification::centred);
    }

    void paintOverChildren (Graphics& g) override
    {
        g.setColour (isEnabled() ? Col::text : Col::text3);
        g.setFont (monoFont (10.0f));
        g.drawText (format ? format (slider.getValue()) : String (slider.getValue()), slider.getBounds(), Justification::centred);
    }

private:
    String label;
};

//==============================================================================
class VariVoxPanel : public Component,
                     public SettableTooltipClient,
                     private Timer,
                     private ChangeListener
{
public:
    explicit VariVoxPanel (te::Edit& e) : edit (e), output (e.engine.getDeviceManager().deviceManager)
    {
        setWantsKeyboardFocus (false);
        addAndMakeVisible (view);
        view.onSelectionChanged = [this] { refreshDeck(); };
        view.onEdited = [this] { refreshDeck(); };   // (aussi à la fin de l'analyse : les vraies corrections passent par onCommitted)
        view.onAudition = [this] (const TakeRef& r, vv::int64 a, vv::int64 b) { output.audition.play (r.take, a, b); };
        view.onSeek = [this] (double t) { edit.getTransport().setPosition (te::TimePosition::fromSeconds (jmax (0.0, t))); };

        // onglets
        const char* tabNames[] = { "Hauteur", "Segments", "Niveau" };
        const char* tabTips[] = { "Glisser une note : la changer de note. Double-clic : la mettre juste. Q : quantifier la sélection.",
                                  "Double-clic : couper une note. Glisser un bord : déplacer la coupure. Alt + clic : coller à la suivante.",
                                  "Niveleur : syllabes au même niveau, respirations et sibilantes baissées. Glisser un bloc : son volume." };
        for (int i = 0; i < 3; ++i)
        {
            auto& t = tabs[i];
            t.setButtonText (String::fromUTF8 (tabNames[i]));
            t.setTooltip (String::fromUTF8 (tabTips[i]));
            t.setClickingTogglesState (false);
            t.setColour (TextButton::buttonOnColourId, Col::accent);
            t.setColour (TextButton::textColourOnId, Colour (0xff15100a));
            t.onClick = [this, i] { setTool ((NoteView::Tool) i); };
            addAndMakeVisible (t);
        }

        auto setupButton = [this] (TextButton& b, const char* text, const char* tip)
        {
            b.setButtonText (String::fromUTF8 (text));
            b.setTooltip (String::fromUTF8 (tip));
            b.setColour (TextButton::buttonOnColourId, Col::accent);
            b.setColour (TextButton::textColourOnId, Colour (0xff15100a));
            addAndMakeVisible (b);
        };
        setupButton (undoBtn, "↶", "Annuler (Ctrl+Z)");
        setupButton (redoBtn, "↷", "Rétablir (Ctrl+Maj+Z)");
        setupButton (slotA, "A", "Version A des corrections");
        setupButton (slotB, "B", "Version B des corrections (part d'une copie de A) : compare deux réglages");
        setupButton (bypassBtn, "Original", "Écouter la voix d'origine dans le projet (sans correction)");
        setupButton (detectBtn, "Détecter", "Trouve la tonalité de la voix");
        setupButton (allBtn, "Tout", "Tout sélectionner (Ctrl+A)");
        setupButton (noneBtn, "Aucune", "Aucune sélection : les potards agissent sur toutes les notes");
        setupButton (revertBtn, "Retirer VariVox", "Le clip rejoue la prise d'origine et les corrections sont effacées");
        undoBtn.onClick = [this] { view.undo(); markEdited(); refreshDeck(); };
        redoBtn.onClick = [this] { view.redo(); markEdited(); refreshDeck(); };
        slotA.onClick = [this] { if (take) { take->setActiveSlot (0); markEdited(); refreshDeck(); } };
        slotB.onClick = [this] { if (take) { take->setActiveSlot (1); markEdited(); refreshDeck(); } };
        bypassBtn.onClick = [this] { setBypass (! bypassed); };
        allBtn.onClick = [this] { view.selectAll(); };
        noneBtn.onClick = [this] { view.clearSelection(); };
        revertBtn.onClick = [this] { revertToOriginal(); };
        detectBtn.onClick = [this]
        {
            if (take == nullptr || take->getModel().notes.empty()) return;
            const auto key = vv::detectKey (take->getModel().notes, take->getSampleRate());
            view.editSettings ([key] (vv::Settings& s) { s.scale = key; });
            markEdited();
            refreshDeck();
        };

        // gamme
        scaleBox.addItem ("Chromatique", 1);
        for (int root = 0; root < 12; ++root)
        {
            scaleBox.addItem (String::fromUTF8 (vv::noteNameFr (root)) + " majeur", 2 + root * 2);
            scaleBox.addItem (String::fromUTF8 (vv::noteNameFr (root)) + " mineur", 3 + root * 2);
        }
        scaleBox.setTooltip (String::fromUTF8 ("Gamme vers laquelle « Quantifier » ramène les notes. Chromatique : le demi-ton le plus proche."));
        scaleBox.onChange = [this]
        {
            const int id = scaleBox.getSelectedId();
            if (id <= 0 || take == nullptr) return;
            vv::Scale sc;
            if (id > 1) { sc.root = (id - 2) / 2; sc.mode = (id - 2) % 2 == 0 ? 1 : 2; }
            view.editSettings ([sc] (vv::Settings& s) { s.scale = sc; });
            markEdited();
        };
        addAndMakeVisible (scaleBox);

        // potards
        auto pct = [] (double v) { return String ((int) std::lround (v)) + " %"; };
        auto db  = [] (double v) { return String (v, std::abs (v - std::round (v)) > 0.01 ? 1 : 0) + " dB"; };
        setupNoteKnob (quantKnob, 0, 100, 1, 0, [] (vv::Note& n, double v) { n.quant = (float) (v / 100.0); n.cents = 0.0f; });
        quantKnob.format = pct;
        setupNoteKnob (straightKnob, 0, 100, 1, 0, [] (vv::Note& n, double v) { n.straight = (float) (v / 100.0); });
        straightKnob.format = pct;
        setupNoteKnob (formantKnob, -12, 12, 0.1, 0, [] (vv::Note& n, double v) { n.formant = (float) v; });
        formantKnob.format = [] (double v) { return (v > 0.05 ? "+" : "") + String (v, 1); };
        setupNoteKnob (levelKnob, 0, 100, 1, 0, [] (vv::Note& n, double v) { n.level = (float) (v / 100.0); });
        levelKnob.format = pct;
        setupNoteKnob (breathKnob, -30, 0, 0.5, 0, {}, [] (vv::Noise& z, double v) { if (z.type == vv::Noise::breath) z.gainDb = (float) v; });
        breathKnob.format = db;
        breathKnob.slider.setColour (Slider::rotarySliderFillColourId, Colour (0xff7d9cc9));
        setupNoteKnob (sibKnob, -12, 0, 0.5, 0, {}, [] (vv::Noise& z, double v) { if (z.type == vv::Noise::sibilant) z.gainDb = (float) v; });
        sibKnob.format = db;
        sibKnob.slider.setColour (Slider::rotarySliderFillColourId, Colour (0xff57c2b6));

        setupSettingsKnob (transKnob, 0, 200, 1, 40, [] (vv::Settings& s, double v) { s.transitionMs = (float) v; });
        transKnob.format = [] (double v) { return String ((int) std::lround (v)) + " ms"; };
        setupSettingsKnob (targetKnob, -40, -6, 0.5, -18, [] (vv::Settings& s, double v) { s.levelTarget = (float) v; });
        targetKnob.format = db;

        for (int i = 0; i < 3; ++i)
        {
            static const float ranges[3] = { 6.0f, 12.0f, 24.0f };
            auto& b = rangeBtns[i];
            b.setButtonText (String::fromUTF8 (i == 0 ? "±6 dB" : (i == 1 ? "±12 dB" : "±24 dB")));
            b.setTooltip (String::fromUTF8 ("Correction maximale du niveleur, pour ne pas écraser l'interprétation."));
            b.setColour (TextButton::buttonOnColourId, Col::accent);
            b.setColour (TextButton::textColourOnId, Colour (0xff15100a));
            const float rv = ranges[i];
            b.onClick = [this, rv] { view.editSettings ([rv] (vv::Settings& st) { st.levelRange = rv; }); markEdited(); refreshDeck(); };
            addChildComponent (b);
        }

        updateMode();
        startTimerHz (20);
    }

    ~VariVoxPanel() override
    {
        stopTimer();
        output.audition.stop();
        flush();
        if (take) take->removeChangeListener (this);
    }

    std::function<void()> onClose;

    static bool canEdit (te::Clip* c) { return dynamic_cast<te::WaveAudioClip*> (c) != nullptr; }

    /** Ouvre un clip audio dans VariVox (nullptr : ferme). */
    void setClip (te::WaveAudioClip* c)
    {
        flush();
        output.audition.stop();
        if (take) take->removeChangeListener (this);
        take = nullptr;
        clip = c;
        bypassed = false;
        problem = {};
        view.setTakes ({});

        if (clip != nullptr)
        {
            if (std::abs (clip->getSpeedRatio() - 1.0) > 1.0e-6 || clip->getAutoTempo() || clip->isLooping())
                problem = String::fromUTF8 ("VariVox ne travaille pas encore sur un clip étiré (warp) ou en boucle.");
            else
                loadTake();
        }
        updateTakes();
        refreshDeck();
        repaint();
    }

    te::WaveAudioClip* getClip() const { return clip.get(); }

    /** Enregistre les corrections en cours dans le clip (sauvegarde du projet). */
    void flush()
    {
        if (take == nullptr || clip == nullptr) return;
        if (edited) applyRendered (true);
        storeState();
    }

    //==========================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff0d0e11));

        auto title = getLocalBounds().removeFromTop (titleH);
        g.setGradientFill (ColourGradient (Col::panel2, 0, 0, Col::panel1, 0, (float) titleH, false));
        g.fillRect (title);
        g.setColour (Col::accent);
        g.setFont (uiFont (12.0f, true));
        g.drawText ("VARIVOX", 10, 0, 70, titleH, Justification::centredLeft);
        g.setColour (Col::text3);
        g.setFont (uiFont (11.0f));
        g.drawText (String::fromUTF8 ("correction de justesse"), 76, 0, 140, titleH, Justification::centredLeft);
        g.setColour (Col::text);
        g.setFont (uiFont (12.5f));
        g.drawText (clip ? clip->getName() + String::fromUTF8 ("  ·  ") + (clip->getTrack() ? clip->getTrack()->getName() : String())
                         : String::fromUTF8 ("Double-clique un clip audio pour l'ouvrir ici"),
                    220, 0, 420, titleH, Justification::centredLeft);
        g.setColour (statusColour());
        g.setFont (uiFont (11.0f));
        g.drawText (statusText(), getWidth() - 360, 0, 320, titleH, Justification::centredRight);
        g.setColour (Col::text2);
        g.drawText (String::fromUTF8 ("✕"), getWidth() - 30, 0, 24, titleH, Justification::centred);

        auto deck = getLocalBounds().withTrimmedTop (titleH).removeFromTop (deckH);
        g.setColour (Colour (0xff121418));
        g.fillRect (deck);
        g.setColour (Colour (0xff1f2228));
        g.fillRect (deck.removeFromBottom (1));
        for (auto x : separators) g.fillRect (x, titleH + 10, 1, deckH - 20);

        if (problem.isNotEmpty())
        {
            g.setColour (Col::text2);
            g.setFont (uiFont (13.0f));
            g.drawText (problem, view.getBounds(), Justification::centred);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().withTrimmedTop (titleH);
        auto deck = r.removeFromTop (deckH).reduced (10, 8);
        view.setBounds (r);
        separators.clear();

        auto col = deck.removeFromLeft (230);
        auto row1 = col.removeFromTop (28);
        for (auto& t : tabs) t.setBounds (row1.removeFromLeft (74).reduced (1, 0));
        col.removeFromTop (8);
        auto row2 = col.removeFromTop (26);
        undoBtn.setBounds (row2.removeFromLeft (34)); row2.removeFromLeft (4);
        redoBtn.setBounds (row2.removeFromLeft (34)); row2.removeFromLeft (10);
        slotA.setBounds (row2.removeFromLeft (30)); row2.removeFromLeft (2);
        slotB.setBounds (row2.removeFromLeft (30)); row2.removeFromLeft (10);
        bypassBtn.setBounds (row2.removeFromLeft (76));
        separators.add (deck.getX() + 6);
        deck.removeFromLeft (14);

        const int kw = 70;
        if (! levelMode())
        {
            for (auto* k : { &quantKnob, &straightKnob, &transKnob, &formantKnob }) k->setBounds (deck.removeFromLeft (kw));
            separators.add (deck.getX() + 6);
            deck.removeFromLeft (14);
            auto c = deck.removeFromLeft (170);
            scaleBox.setBounds (c.removeFromTop (28));
            c.removeFromTop (8);
            detectBtn.setBounds (c.removeFromTop (26).removeFromLeft (90));
        }
        else
        {
            for (auto* k : { &levelKnob, &targetKnob, &breathKnob, &sibKnob }) k->setBounds (deck.removeFromLeft (kw));
            separators.add (deck.getX() + 6);
            deck.removeFromLeft (14);
            auto c = deck.removeFromLeft (190);
            auto rr = c.removeFromTop (28);
            for (auto& b : rangeBtns) b.setBounds (rr.removeFromLeft (62).reduced (1, 0));
        }
        separators.add (deck.getX() + 6);
        deck.removeFromLeft (14);
        auto c = deck.removeFromLeft (150);
        auto sr = c.removeFromTop (28);
        allBtn.setBounds (sr.removeFromLeft (70)); sr.removeFromLeft (6);
        noneBtn.setBounds (sr.removeFromLeft (74));
        revertBtn.setBounds (deck.removeFromRight (130).removeFromTop (26));
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (e.y < titleH && e.x > getWidth() - 34 && onClose) onClose();
    }

    bool keyPressed (const KeyPress& k) override
    {
        if (k == KeyPress::escapeKey && onClose) { onClose(); return true; }
        return false;
    }

    void grabFocusOnEditor() { view.zoomToFit(); view.grabKeyboardFocus(); }

private:
    static constexpr int titleH = 28, deckH = 92;

    te::Edit& edit;
    VariVoxAuditionOutput output;
    NoteView view;
    te::WaveAudioClip::Ptr clip;
    std::shared_ptr<Take> take;
    std::map<String, std::shared_ptr<Take>> takes;   // clips déjà ouverts (annulations gardées)

    TextButton tabs[3];
    TextButton undoBtn, redoBtn, slotA, slotB, bypassBtn, detectBtn, allBtn, noneBtn, revertBtn;
    TextButton rangeBtns[3];
    ComboBox scaleBox;
    VariVoxKnob quantKnob { "QUANTIFIER", "Rapproche chaque note de la note juste (de la gamme choisie). 100 % : note parfaitement juste." },
                straightKnob { "REDRESSER", "Aplatit la courbe de hauteur dans la note : vibrato, attaques par en dessous, flottements." },
                transKnob { "TRANSITIONS", "Durée des enchaînements entre deux notes corrigées : court = net, long = naturel." },
                formantKnob { "FORMANT", "Timbre de la voix sans changer la note : à gauche plus sombre, à droite plus clair." },
                levelKnob { "NIVELER", "Rapproche chaque syllabe du niveau visé. 100 % : toutes au même niveau (dans la limite de la plage)." },
                targetKnob { "CIBLE", "Niveau visé des syllabes. Double-clic : niveau médian de la voix." },
                breathKnob { "RESPIRATIONS", "Baisse les respirations (sélectionnées, ou toutes)." },
                sibKnob { "SIBILANTES", "Adoucit les « s », « ch »… (sélectionnés, ou tous)." };
    Array<int> separators;

    bool gestureActive = false, edited = false, bypassed = false;
    uint32 lastEditMs = 0;
    String problem;
    File lastRendered;

    //==========================================================================
    bool levelMode() const { return view.getTool() == NoteView::Tool::level; }

    void setTool (NoteView::Tool t)
    {
        view.setTool (t);
        updateMode();
        refreshDeck();
        resized();
        repaint();
    }

    void updateMode()
    {
        const bool lv = levelMode();
        for (auto* k : { &quantKnob, &straightKnob, &transKnob, &formantKnob }) { addChildComponent (*k); k->setVisible (! lv); }
        for (auto* k : { &levelKnob, &targetKnob, &breathKnob, &sibKnob }) { addChildComponent (*k); k->setVisible (lv); }
        scaleBox.setVisible (! lv);
        detectBtn.setVisible (! lv);
        for (auto& b : rangeBtns) b.setVisible (lv);
        for (int i = 0; i < 3; ++i) tabs[i].setToggleState ((int) view.getTool() == i, dontSendNotification);
    }

    void setupNoteKnob (VariVoxKnob& k, double lo, double hi, double step, double def,
                        std::function<void (vv::Note&, double)> applyNote,
                        std::function<void (vv::Noise&, double)> applyNoise = {})
    {
        k.slider.setRange (lo, hi, step);
        k.slider.setDoubleClickReturnValue (true, def);
        k.slider.onDragStart = [this] { view.beginGesture(); gestureActive = true; };
        k.slider.onDragEnd = [this] { view.endGesture(); gestureActive = false; markEdited(); };
        k.slider.onValueChange = [this, &k, applyNote, applyNoise]
        {
            const double v = k.slider.getValue();
            NoteView::NoteFn fn;
            NoteView::NoiseFn nfn;
            if (applyNote) fn = [applyNote, v] (vv::Note& n, const vv::Settings&) { applyNote (n, v); };
            if (applyNoise) nfn = [applyNoise, v] (vv::Noise& z, const vv::Settings&) { applyNoise (z, v); };
            if (gestureActive) view.applyGesture (fn, nfn);
            else { view.editNotes (fn, nfn); markEdited(); }
            k.repaint();
        };
    }

    void setupSettingsKnob (VariVoxKnob& k, double lo, double hi, double step, double def,
                            std::function<void (vv::Settings&, double)> apply)
    {
        k.slider.setRange (lo, hi, step);
        k.slider.setDoubleClickReturnValue (true, def);
        k.slider.onDragStart = [this] { view.beginGesture(); gestureActive = true; };
        k.slider.onDragEnd = [this] { view.endGesture(); gestureActive = false; markEdited(); };
        k.slider.onValueChange = [this, &k, apply]
        {
            const double v = k.slider.getValue();
            auto fn = [apply, v] (vv::Settings& s) { apply (s, v); };
            if (gestureActive) view.applySettingsGesture (fn);
            else { view.editSettings (fn); markEdited(); }
            k.repaint();
        };
    }

    void markEdited()
    {
        if (take == nullptr) return;
        edited = true;
        lastEditMs = Time::getMillisecondCounter();
    }

    //==========================================================================
    static const Identifier& stateProp()    { static Identifier id ("chenVariVox"); return id; }
    static const Identifier& originalProp() { static Identifier id ("chenOriginalSource"); return id; }

    File originalFile() const
    {
        auto o = clip->state.getProperty (originalProp()).toString();
        if (o.isNotEmpty() && File::isAbsolutePath (o) && File (o).existsAsFile()) return File (o);
        return clip->getOriginalFile();
    }

    File renderFolder() const
    {
        auto f = edit.editFileRetriever ? edit.editFileRetriever() : File();
        auto base = f != File() ? f.getParentDirectory() : File::getSpecialLocation (File::tempDirectory);
        auto name = f != File() ? f.getFileNameWithoutExtension() : String ("ChenRecPro");
        auto dir = base.getChildFile (name + " - VariVox");
        dir.createDirectory();
        return dir;
    }

    void loadTake()
    {
        const auto key = clip->itemID.toString();
        if (auto it = takes.find (key); it != takes.end())
        {
            take = it->second;
            take->addChangeListener (this);
            return;
        }

        auto src = originalFile();
        std::unique_ptr<AudioFormatReader> reader (edit.engine.getAudioFileFormatManager().readFormatManager.createReaderFor (src));
        if (reader == nullptr)
        {
            problem = String::fromUTF8 ("Impossible de lire le fichier audio du clip : ") + src.getFileName();
            return;
        }
        if (reader->lengthInSamples > (int64) (reader->sampleRate * 60.0 * 20.0))
        {
            problem = String::fromUTF8 ("Clip trop long pour VariVox (20 minutes au plus).");
            return;
        }

        AudioBuffer<float> buf ((int) jlimit (1u, 2u, reader->numChannels), (int) reader->lengthInSamples);
        reader->read (&buf, 0, buf.getNumSamples(), 0, true, buf.getNumChannels() > 1);

        take = std::make_shared<Take>();
        auto saved = clip->state.getProperty (stateProp()).toString();
        if (saved.isNotEmpty())
        {
            MemoryOutputStream mo;
            if (Base64::convertFromBase64 (mo, saved))
            {
                MemoryInputStream mi (mo.getData(), mo.getDataSize(), false);
                take->readState (mi);
            }
        }
        take->setAudio (std::move (buf), reader->sampleRate);
        take->onCommitted = [this] { markEdited(); };
        take->addChangeListener (this);
        takes[key] = take;

        auto cur = clip->getSourceFileReference().getFile();
        if (clip->state.hasProperty (originalProp()) && cur != src && cur.existsAsFile()) lastRendered = cur;
    }

    void updateTakes()
    {
        if (take == nullptr || clip == nullptr) { view.setTakes ({}); return; }
        TakeRef ref;
        ref.take = take;
        const double sr = jmax (1.0, take->getSampleRate());
        const double offset = clip->getPosition().getOffset().inSeconds();
        ref.timelineStart = clip->getPosition().getStart().inSeconds() - offset;
        ref.clipStart = (vv::int64) std::llround (offset * sr);
        ref.clipEnd = jmin (take->getNumSamples(), (vv::int64) std::llround ((offset + clip->getPosition().getLength().inSeconds()) * sr));
        if (! (view.getTakes().size() == 1 && view.getTakes().front() == ref))
        {
            view.setTakes ({ ref });
            view.zoomToFit();
        }
    }

    void storeState()
    {
        if (take == nullptr || clip == nullptr || take->getAnalysis() == nullptr) return;
        MemoryOutputStream mo;
        take->writeState (mo);
        clip->state.setProperty (stateProp(), Base64::toBase64 (mo.getData(), mo.getDataSize()), nullptr);
    }

    /** Écrit la voix corrigée et fait jouer ce fichier par le clip. */
    void applyRendered (bool evenIfBusy)
    {
        if (take == nullptr || clip == nullptr) return;
        if (! evenIfBusy && (! take->isRenderUpToDate() || take->getState() != Take::State::ready)) return;
        auto rendered = take->getRenderedAudio();
        if (rendered == nullptr || rendered->getNumSamples() == 0) return;
        edited = false;

        auto file = renderFolder().getChildFile (File::createLegalFileName (clip->getName()) + "_" + clip->itemID.toString()
                                                 + "_" + String (Time::currentTimeMillis() % 100000000) + ".wav");
        {
            WavAudioFormat wav;
            std::unique_ptr<OutputStream> os (file.createOutputStream().release());
            if (os == nullptr) return;
            std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), take->getSampleRate(),
                                                                       (unsigned int) rendered->getNumChannels(), 24, {}, 0));
            if (w == nullptr) return;
            os.release();
            w->writeFromAudioSampleBuffer (*rendered, 0, rendered->getNumSamples());
        }

        if (! clip->state.hasProperty (originalProp()))
            clip->state.setProperty (originalProp(), clip->getOriginalFile().getFullPathName(), nullptr);
        auto previous = lastRendered;
        lastRendered = file;
        if (! bypassed) clip->getSourceFileReference().setToFile (file, te::SourceFileReference::PathStyle::alwaysAbsolute, false);
        storeState();
        if (previous != File() && previous != file && previous.isAChildOf (renderFolder())) previous.deleteFile();
    }

    void setBypass (bool b)
    {
        if (clip == nullptr || lastRendered == File()) { bypassed = false; refreshDeck(); return; }
        bypassed = b;
        if (take) take->bypass = b;
        clip->getSourceFileReference().setToFile (b ? originalFile() : lastRendered, te::SourceFileReference::PathStyle::alwaysAbsolute, false);
        refreshDeck();
    }

    void revertToOriginal()
    {
        if (clip == nullptr) return;
        auto orig = originalFile();
        clip->getSourceFileReference().setToFile (orig, te::SourceFileReference::PathStyle::alwaysAbsolute, false);
        clip->state.removeProperty (stateProp(), nullptr);
        clip->state.removeProperty (originalProp(), nullptr);
        if (lastRendered.isAChildOf (renderFolder())) lastRendered.deleteFile();
        lastRendered = File();
        takes.erase (clip->itemID.toString());
        if (take) take->removeChangeListener (this);
        take = nullptr;
        edited = false;
        te::WaveAudioClip::Ptr c = clip;
        clip = nullptr;
        setClip (c.get());   // ré-analyse la prise d'origine, sans correction
    }

    //==========================================================================
    void changeListenerCallback (ChangeBroadcaster*) override { refreshDeck(); }

    void timerCallback() override
    {
        if (clip != nullptr && clip->getTrack() == nullptr) { setClip (nullptr); if (onClose) onClose(); return; }
        if (take == nullptr) return;

        auto& tc = edit.getTransport();
        view.setPlayhead (tc.getPosition().inSeconds(), tc.isPlaying());
        updateTakes();

        String msg;
        if (take->getState() == Take::State::analysing)
            msg = String::fromUTF8 ("Analyse de la voix… ") + String ((int) (take->getProgress() * 100.0f)) + " %";
        else if (take->getState() == Take::State::failed)
            msg = String::fromUTF8 ("Analyse impossible : pas de voix détectée dans ce clip.");
        view.setMessage (msg);

        if (edited && ! gestureActive && ! tc.isRecording()
            && Time::getMillisecondCounter() - lastEditMs > 350)
            applyRendered (false);

        repaint (0, 0, getWidth(), titleH);
    }

    String statusText() const
    {
        if (take == nullptr) return {};
        if (take->getState() == Take::State::analysing) return String::fromUTF8 ("analyse en cours…");
        if (bypassed) return String::fromUTF8 ("le projet joue la voix d'origine");
        if (edited || ! take->isRenderUpToDate()) return String::fromUTF8 ("calcul de la correction…");
        if (lastRendered != File()) return String::fromUTF8 ("● le clip joue la voix corrigée");
        return String::fromUTF8 ("prêt : corrige, le clip suit en direct");
    }

    Colour statusColour() const
    {
        if (bypassed) return Col::text2;
        if (take != nullptr && lastRendered != File() && ! edited && take->isRenderUpToDate()) return Colour (0xff6fd39a);
        return Col::text3;
    }

    void refreshDeck()
    {
        const bool has = take != nullptr && take->getAnalysis() != nullptr;
        for (auto* b : { &slotA, &slotB, &detectBtn, &allBtn, &noneBtn, &revertBtn }) b->setEnabled (has);
        bypassBtn.setEnabled (has && lastRendered != File());
        bypassBtn.setToggleState (bypassed, dontSendNotification);
        undoBtn.setEnabled (has && view.canUndo());
        redoBtn.setEnabled (has && view.canRedo());
        if (take)
        {
            slotA.setToggleState (take->getActiveSlot() == 0, dontSendNotification);
            slotB.setToggleState (take->getActiveSlot() == 1, dontSendNotification);
        }

        const auto targets = view.targetNotes();
        const vv::Note* first = nullptr;
        const vv::Noise* firstBreath = nullptr;
        const vv::Noise* firstSib = nullptr;
        for (auto& id : targets)
        {
            if (auto* n = view.getNote (id)) { if (first == nullptr) first = n; }
            else if (auto* z = view.getNoise (id))
            {
                if (z->type == vv::Noise::breath && firstBreath == nullptr) firstBreath = z;
                if (z->type == vv::Noise::sibilant && firstSib == nullptr) firstSib = z;
            }
        }
        // les bruits ne sont « visés » que dans l'onglet Niveau : chercher aussi dans tout ce qui est visible
        if (firstBreath == nullptr || firstSib == nullptr)
            for (auto& id : view.allTargets())
                if (auto* z = view.getNoise (id))
                {
                    if (z->type == vv::Noise::breath && firstBreath == nullptr) firstBreath = z;
                    if (z->type == vv::Noise::sibilant && firstSib == nullptr) firstSib = z;
                }

        if (! gestureActive)
        {
            quantKnob.slider.setValue (first ? first->quant * 100.0 : 0.0, dontSendNotification);
            straightKnob.slider.setValue (first ? first->straight * 100.0 : 0.0, dontSendNotification);
            formantKnob.slider.setValue (first ? first->formant : 0.0, dontSendNotification);
            levelKnob.slider.setValue (first ? first->level * 100.0 : 0.0, dontSendNotification);
            breathKnob.slider.setValue (firstBreath ? firstBreath->gainDb : 0.0, dontSendNotification);
            sibKnob.slider.setValue (firstSib ? firstSib->gainDb : 0.0, dontSendNotification);
            if (has)
            {
                const auto& m = take->getModel();
                transKnob.slider.setValue (m.settings.transitionMs, dontSendNotification);
                vv::Model autoModel = m;
                autoModel.settings.levelTarget = std::numeric_limits<float>::quiet_NaN();
                const double autoTarget = jlimit (-40.0, -6.0, (double) vv::levelTarget (take->getAnalysis()->track, autoModel));
                targetKnob.slider.setDoubleClickReturnValue (true, autoTarget);
                targetKnob.slider.setValue (jlimit (-40.0, -6.0, (double) vv::levelTarget (take->getAnalysis()->track, m)), dontSendNotification);
                const auto& sc = m.settings.scale;
                scaleBox.setSelectedId (sc.mode == 0 ? 1 : 2 + sc.root * 2 + (sc.mode == 2 ? 1 : 0), dontSendNotification);
                const float rg = m.settings.levelRange;
                rangeBtns[0].setToggleState (rg < 9.0f, dontSendNotification);
                rangeBtns[1].setToggleState (rg >= 9.0f && rg < 18.0f, dontSendNotification);
                rangeBtns[2].setToggleState (rg >= 18.0f, dontSendNotification);
            }
        }
        for (auto* k : { &quantKnob, &straightKnob, &formantKnob, &levelKnob }) k->setEnabled (first != nullptr);
        breathKnob.setEnabled (firstBreath != nullptr);
        sibKnob.setEnabled (firstSib != nullptr);
        for (auto* k : { &transKnob, &targetKnob }) k->setEnabled (has);
        scaleBox.setEnabled (has);
        for (auto& b : rangeBtns) b.setEnabled (has);
        repaint();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VariVoxPanel)
};

} // namespace chen
