#pragma once

// =============================================================================
//  Console (F3) — tranches façon Pro Tools : inserts A–J, envois, pan, fader,
//  vu-mètres. Gestion des plugins par menu hiérarchique.
// =============================================================================

namespace chen
{

//==============================================================================
/** Menu d'insertion façon Pro Tools : aucun insert / plug-in ▸ / instrument ▸ … */
struct InsertMenu
{
    static bool isChenevieres (const PluginDescription& d)
    {
        auto s = (d.manufacturerName + " " + d.name).toLowerCase();
        return s.contains ("chenevi") || s.contains (String::fromUTF8 ("chènevi")) || s.contains ("analog beagle");
    }

    struct Choice { String builtinType; PluginDescription desc; bool remove = false, manager = false; };

    static void show (te::Engine& engine, Component& target, bool slotFilled, std::function<void (Choice)> cb, bool instrumentsOnly = false)
    {
        auto& list = engine.getPluginManager().knownPluginList;
        auto types = list.getTypes();

        std::map<int, Choice> results;
        int nextId = 1;
        auto add = [&] (PopupMenu& m, const String& name, Choice c, const String& sub = {}, bool enabled = true)
        {
            PopupMenu::Item it (name);
            it.itemID = nextId;
            it.isEnabled = enabled;
            if (sub.isNotEmpty()) it.shortcutKeyDescription = sub;
            m.addItem (it);
            results[nextId++] = c;
        };

        PopupMenu root, fx, inst, fav, byCat, byMaker, builtin;

        // ★ Chènevières
        for (auto& d : types)
            if (! d.isInstrument && isChenevieres (d))
                add (fav, d.name, { {}, d }, d.pluginFormatName);
        if (fav.getNumItems() == 0)
            fav.addItem (-1, String::fromUTF8 ("Aucun plugin Chènevières trouvé — lance une analyse"), false);

        // par catégorie / par fabricant
        std::map<String, PopupMenu> cats, makers;
        for (auto& d : types)
        {
            if (d.isInstrument) continue;
            auto cat = d.category.upToFirstOccurrenceOf ("|", false, false).trim();
            add (cats[cat.isEmpty() ? String ("Autres") : cat], d.name, { {}, d }, d.manufacturerName);
            add (makers[d.manufacturerName.isEmpty() ? String ("Inconnu") : d.manufacturerName], d.name, { {}, d }, d.pluginFormatName);
        }
        for (auto& [n, m] : cats)   byCat.addSubMenu (n, m);
        for (auto& [n, m] : makers) byMaker.addSubMenu (n, m);

        // plugins intégrés au moteur
        add (builtin, String::fromUTF8 ("EQ 4 bandes"),  { te::EqualiserPlugin::xmlTypeName, {} });
        add (builtin, "Compresseur",                     { te::CompressorPlugin::xmlTypeName, {} });
        add (builtin, String::fromUTF8 ("Réverb"),       { te::ReverbPlugin::xmlTypeName, {} });
        add (builtin, String::fromUTF8 ("Délai"),        { te::DelayPlugin::xmlTypeName, {} });
        add (builtin, "Chorus",                          { te::ChorusPlugin::xmlTypeName, {} });
        add (builtin, "Phaser",                          { te::PhaserPlugin::xmlTypeName, {} });
        add (builtin, "Filtre passe-bas / passe-haut",   { te::LowPassPlugin::xmlTypeName, {} });

        fx.addSectionHeader ("FAVORIS");
        fx.addSubMenu (String::fromUTF8 ("★ Chènevières"), fav);
        fx.addSectionHeader (String::fromUTF8 ("CATÉGORIES"));
        fx.addSubMenu (String::fromUTF8 ("Par catégorie"), byCat, byCat.getNumItems() > 0);
        fx.addSubMenu ("Par fabricant", byMaker, byMaker.getNumItems() > 0);
        fx.addSeparator();
        fx.addSubMenu (String::fromUTF8 ("Intégrés ChènRecPro"), builtin);

        std::map<String, PopupMenu> instMakers;
        for (auto& d : types)
            if (d.isInstrument)
                add (instMakers[d.manufacturerName.isEmpty() ? String ("Inconnu") : d.manufacturerName], d.name, { {}, d }, d.pluginFormatName);
        for (auto& [n, m] : instMakers) inst.addSubMenu (n, m);
        inst.addSeparator();
        add (inst, String::fromUTF8 ("4OSC (synthé intégré)"), { te::FourOscPlugin::xmlTypeName, {} });
        add (inst, String::fromUTF8 ("Sampler (intégré)"),     { te::SamplerPlugin::xmlTypeName, {} });

        add (root, "aucun insert", { {}, {}, true }, {}, slotFilled);
        root.addSeparator();
        root.addSubMenu ("plug-in", fx);
        if (instrumentsOnly)
        {
            inst.showMenuAsync (PopupMenu::Options().withTargetComponent (&target),
                                [results, cb] (int r) { if (auto it = results.find (r); it != results.end()) cb (it->second); });
            return;
        }
        root.addSubMenu ("instrument", inst, inst.getNumItems() > 0);
        root.addSeparator();
        add (root, String::fromUTF8 ("Gestionnaire de plugins…"), { {}, {}, false, true });

        root.showMenuAsync (PopupMenu::Options().withTargetComponent (&target).withPreferredPopupDirection (PopupMenu::Options::PopupDirection::downwards),
                            [results, cb] (int r)
                            {
                                if (auto it = results.find (r); it != results.end())
                                    cb (it->second);
                            });
    }
};

//==============================================================================
/** Ouvre le gestionnaire / analyseur de plugins (VST3, AU). */
inline void showPluginManager (te::Engine& engine)
{
    auto& pm = engine.getPluginManager();
    auto deadMansPedal = engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal");
    auto* c = new PluginListComponent (pm.pluginFormatManager, pm.knownPluginList, deadMansPedal,
                                       &engine.getPropertyStorage().getPropertiesFile(), true);
    c->setNumberOfThreadsForScanning (4);
    c->setSize (820, 560);

    DialogWindow::LaunchOptions o;
    o.dialogTitle = String::fromUTF8 ("Gestionnaire de plugins — ChènRecPro");
    o.dialogBackgroundColour = Col::panel1;
    o.content.setOwned (c);
    o.resizable = true;
    o.useNativeTitleBar = true;
    o.launchAsync();
}

//==============================================================================
class ChannelStrip : public Component, private Timer
{
public:
    static constexpr int numInserts = 10;
    static constexpr int stripWidth = 104;

    ChannelStrip (te::Track& t, int number, bool isMaster = false)
        : track (&t), trackNumber (number), master (isMaster)
    {
        init();
    }

    ChannelStrip (te::Edit& e)  // tranche Master
        : masterEdit (&e), master (true)
    {
        init();
    }

    ~ChannelStrip() override
    {
        if (attachedMeasurer != nullptr)
            attachedMeasurer->removeClient (meterClient);
    }

    te::VolumeAndPanPlugin* getVolume() const
    {
        if (masterEdit != nullptr) return masterEdit->getMasterVolumePlugin().get();
        if (auto* at = dynamic_cast<te::AudioTrack*> (track)) return at->getVolumePlugin();
        return nullptr;
    }

    te::Edit& getEdit() const { return masterEdit != nullptr ? *masterEdit : track->edit; }

    void paint (Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto base = master ? Colour (0xff2d291f) : Colour (0xff232429);
        g.setGradientFill (ColourGradient (base, 0, 0, base.darker (0.25f), 0, r.getBottom(), false));
        g.fillAll();
        g.setColour (Col::line);
        g.fillRect (getWidth() - 1, 0, 1, getHeight());

        g.setColour (Col::text3);
        g.setFont (uiFont (10.5f));
        g.drawText (master ? String ("MASTER") : String (trackNumber), 0, 2, getWidth(), 14, Justification::centred);

        // --- inserts A–J
        sectionLabel (g, insertsTop - 12, "INSERTS A-E");
        sectionLabel (g, insertsTop + 5 * slotH + 4, "INSERTS F-J");
        auto plugins = getPlugins();
        for (int i = 0; i < numInserts; ++i)
        {
            auto sr = slotRect (i).toFloat();
            auto* p = i < plugins.size() ? plugins[i] : nullptr;
            g.setColour (p ? Colour (0xff2b2c31) : Colour (0xff121315));
            g.fillRoundedRectangle (sr, 2.0f);
            g.setColour (Colour (0xff0a0a0b));
            g.drawRoundedRectangle (sr, 2.0f, 1.0f);
            if (p != nullptr)
            {
                bool chen = p->getName().containsIgnoreCase ("chenevi") || p->getName().contains (String::fromUTF8 ("Chènevi"));
                if (chen) { g.setColour (Col::accent); g.fillRect (sr.getX(), sr.getY(), 2.0f, sr.getHeight()); }
                g.setColour (p->isEnabled() ? Col::meterMid : Colour (0xff6a5530));
                g.fillEllipse (sr.getX() + 5, sr.getCentreY() - 2.5f, 5, 5);
                g.setColour (p->isEnabled() ? Colour (0xffdfe2e6) : Colour (0xff7d6a4a));
                g.setFont (uiFont (11.0f));
                g.drawText (shortName (p->getName()), sr.withTrimmedLeft (14).withTrimmedRight (2), Justification::centredLeft, true);
            }
            else if (i == hoverSlot)
            {
                g.setColour (Col::text3);
                g.setFont (uiFont (10.5f));
                g.drawText (insertLetter (i), sr.reduced (5, 0), Justification::centredLeft);
            }
        }

        // --- envois A–E
        if (! master)
        {
            sectionLabel (g, sendRect (0).getY() - 12, String::fromUTF8 ("ENVOIS A-E"));
            auto sends = Mixing::getSends (*track);
            for (int i = 0; i < numSends; ++i)
            {
                auto sr = sendRect (i).toFloat();
                auto* snd = i < sends.size() ? sends[i] : nullptr;
                g.setColour (snd ? Colour (0xff222a36) : Colour (0xff121315));
                g.fillRoundedRectangle (sr, 2.0f);
                g.setColour (Colour (0xff0a0a0b));
                g.drawRoundedRectangle (sr, 2.0f, 1.0f);
                if (snd != nullptr)
                {
                    // niveau en fond de barre
                    auto pos = jlimit (0.0f, 1.0f, dbToFaderPos (snd->getGainDb()));
                    g.setColour (Colour (0xff5c94dd).withAlpha (snd->isMute() ? 0.15f : 0.35f));
                    g.fillRoundedRectangle (sr.withWidth (sr.getWidth() * pos), 2.0f);
                    g.setColour (snd->isMute() ? Col::text3 : Colour (0xff9cc0f0));
                    g.setFont (uiFont (10.5f));
                    g.drawText (shortName (Mixing::busName (getEdit(), snd->getBusNumber())), sr.reduced (5, 0).withTrimmedRight (34), Justification::centredLeft, true);
                    g.setColour (Col::text2);
                    g.drawText (dbString (snd->getGainDb()), sr.reduced (4, 0), Justification::centredRight);
                }
            }
        }

        // --- E/S
        auto ioY = ioTop();
        sectionLabel (g, ioY - 12, "E/S");
        drawField (g, { 4.0f, (float) ioY, getWidth() - 8.0f, 17.0f }, inputName());
        String out = master ? String ("Sortie 1-2") : String::fromUTF8 ("Stéréo Out");
        if (auto* at = dynamic_cast<te::AudioTrack*> (track)) out = Mixing::outputName (*at);
        drawField (g, { 4.0f, (float) ioY + 19, getWidth() - 8.0f, 17.0f }, String::fromUTF8 ("▸ ") + out);

        // --- boutons
        if (! master)
        {
            auto b = buttonsRect().toFloat();
            auto w = (b.getWidth() - 6) / 4.0f;
            auto* at = dynamic_cast<te::AudioTrack*> (track);
            drawPill (g, { b.getX(),             b.getY(), w, b.getHeight() }, String::fromUTF8 ("●"), at && EngineHelpers::isTrackArmed (*at), Colour (0xffc43b40), Colours::white);
            drawPill (g, { b.getX() + w + 2,     b.getY(), w, b.getHeight() }, "I",  at && EngineHelpers::isInputMonitoringEnabled (*at), Colour (0xff2f6fb3), Colours::white);
            drawPill (g, { b.getX() + 2 * w + 4, b.getY(), w, b.getHeight() }, "S",  track->isSolo (false), Colour (0xffd6c33a));
            drawPill (g, { b.getX() + 3 * w + 6, b.getY(), w, b.getHeight() }, "M",  track->isMuted (false), Colour (0xffc98a1a));
        }

        // --- fader + mètres
        auto fr = faderArea().toFloat();
        g.setFont (uiFont (9.5f));
        for (float db : { 6.0f, 0.0f, -5.0f, -10.0f, -20.0f, -30.0f, -40.0f, -60.0f })
        {
            auto y = fr.getBottom() - fr.getHeight() * dbToFaderPos (db);
            g.setColour (Col::text3);
            g.drawText (db > 0 ? "+" + String ((int) db) : String ((int) db), (int) fr.getX() - 2, (int) y - 6, 22, 12, Justification::centredRight);
            g.setColour (Colour (0xff2c2e33));
            g.fillRect (fr.getX() + 24, y, 8.0f, 1.0f);
        }

        auto track_ = Rectangle<float> (fr.getX() + 34, fr.getY(), 4, fr.getHeight());
        g.setColour (Colour (0xff08090a));
        g.fillRoundedRectangle (track_, 1.5f);

        auto vol = getVolume();
        auto db = vol ? vol->getVolumeDb() : 0.0f;
        auto capY = fr.getBottom() - fr.getHeight() * dbToFaderPos (db);
        Rectangle<float> cap (track_.getCentreX() - 13, capY - 17, 26, 34);
        g.setColour (Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (cap.translated (0, 3), 2.0f);
        ColourGradient cg (Colour (0xffe2e5ea), 0, cap.getY(), Colour (0xff82868d), 0, cap.getBottom(), false);
        cg.addColour (0.44, Colour (0xffa9adb4));
        cg.addColour (0.46, Colour (0xff25262a));
        cg.addColour (0.54, Colour (0xff25262a));
        cg.addColour (0.56, Colour (0xffa9adb4));
        g.setGradientFill (cg);
        g.fillRoundedRectangle (cap, 2.0f);

        auto mx = fr.getRight() - 16;
        drawMeter (g, { mx, fr.getY(), 6, fr.getHeight() }, dbToMeter (levels[0]), dbToMeter (peaks[0]));
        drawMeter (g, { mx + 8, fr.getY(), 6, fr.getHeight() }, dbToMeter (levels[1]), dbToMeter (peaks[1]));

        // --- valeurs
        auto vy = (float) faderArea().getBottom() + 6;
        drawField (g, { 4.0f, vy, getWidth() / 2.0f - 5, 16.0f }, dbString (db), Justification::centred);
        g.setColour (Col::meterMid);
        drawField (g, { getWidth() / 2.0f + 1, vy, getWidth() / 2.0f - 5, 16.0f }, dbString (jmax (peaks[0], peaks[1])), Justification::centred, maxPeak > 0.0f ? Col::meterClip : Col::meterMid);

        // --- plaque de nom
        auto np = nameRect().toFloat();
        auto c = master ? Col::accent : track->getColour();
        g.setColour (c);
        g.fillRoundedRectangle (np, 2.0f);
        g.setColour (Colour (0xff141414));
        g.setFont (uiFont (12.5f, true));
        g.drawText (master ? String::fromUTF8 ("STÉRÉO OUT") : track->getName(), np.reduced (3, 0), Justification::centred, true);
        if (! master)
            if (auto gr = Groups::groupOf (*track); gr > 0)
            {
                auto b = Rectangle<float> (np.getX() + 2, np.getY() - 18, 16, 14);
                g.setColour (Groups::colour (gr));
                g.fillRoundedRectangle (b, 3.0f);
                g.setColour (Colour (0xff141414));
                g.setFont (uiFont (10.5f, true));
                g.drawText (Groups::letter (gr), b, Justification::centred);
            }
    }

    void resized() override
    {
        auto ioY = ioTop();
        panSlider.setBounds (getWidth() / 2 - 16, ioY + 42, 32, 32);
    }

    void mouseMove (const MouseEvent& e) override
    {
        auto s = slotAt (e.getPosition());
        if (s != hoverSlot) { hoverSlot = s; repaint(); }
    }

    void mouseExit (const MouseEvent&) override { hoverSlot = -1; repaint(); }

    void mouseDown (const MouseEvent& e) override
    {
        auto pos = e.getPosition();

        if (auto s = slotAt (pos); s >= 0)
        {
            auto plugins = getPlugins();
            auto* p = s < plugins.size() ? plugins[s] : nullptr;

            if (p != nullptr && (e.mods.isCommandDown() || e.mods.isCtrlDown()))
            {
                p->setEnabled (! p->isEnabled());    // Ctrl+clic = bypass
                repaint();
                return;
            }

            if (p != nullptr && ! e.mods.isPopupMenu())
            {
                p->showWindowExplicitly();
                return;
            }

            slotMenuAnchor.setBounds (slotRect (s));
            InsertMenu::show (getEdit().engine, slotMenuAnchor, p != nullptr,
                              [this, s, sp = SafePointer<ChannelStrip> (this)] (InsertMenu::Choice c)
                              {
                                  if (sp != nullptr) applyChoice (s, c);
                              });
            return;
        }

        if (! master)
            if (auto si = sendAt (pos); si >= 0)
            {
                sendClick (si, e);
                return;
            }

        if (! master && Rectangle<int> (4, ioTop() + 19, getWidth() - 8, 17).contains (pos))
        {
            if (auto* at = dynamic_cast<te::AudioTrack*> (track))
            {
                slotMenuAnchor.setBounds (4, ioTop() + 19, getWidth() - 8, 17);
                Mixing::showOutputMenu (*at, &slotMenuAnchor);
            }
            return;
        }

        if (faderArea().expanded (4).contains (pos))
        {
            if (e.mods.isAltDown()) { if (auto v = getVolume()) v->setVolumeDb (0.0f); repaint(); return; }
            dragging = true;
            faderAlone = e.mods.isCommandDown() || e.mods.isCtrlDown();   // Ctrl : hors groupe
            if (auto v = getVolume()) v->volParam->parameterChangeGestureBegin();   // pour l'écriture d'automation
            setFaderFromY (pos.y);
            return;
        }

        if (! master && buttonsRect().contains (pos))
        {
            auto b = buttonsRect();
            auto idx = (pos.x - b.getX()) * 4 / b.getWidth();
            auto* at = dynamic_cast<te::AudioTrack*> (track);
            if (idx == 0 && at) EngineHelpers::armTrack (*at, ! EngineHelpers::isTrackArmed (*at));
            if (idx == 1 && at) EngineHelpers::enableInputMonitoring (*at, ! EngineHelpers::isInputMonitoringEnabled (*at));
            if (idx == 2) Groups::setSolo (*track, ! track->isSolo (false));
            if (idx == 3) Groups::setMute (*track, ! track->isMuted (false));
            repaint();
            return;
        }

        if (nameRect().contains (pos))
            maxPeak = 0.0f;
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (dragging) setFaderFromY (e.getPosition().y);
        if (draggingSend != nullptr)
        {
            auto db = jlimit (-60.0f, 6.0f, sendStartDb - (e.getDistanceFromDragStartY() - e.getDistanceFromDragStartX()) * 0.25f);
            draggingSend->setGainDb (db);
            repaint();
        }
    }

    void mouseUp (const MouseEvent&) override
    {
        if (dragging)
            if (auto v = getVolume()) v->volParam->parameterChangeGestureEnd();
        dragging = false;
        draggingSend = nullptr;
    }

    void mouseDoubleClick (const MouseEvent& e) override
    {
        if (! master && nameRect().contains (e.getPosition()))
        {
            auto* aw = new AlertWindow ("Renommer la piste", {}, MessageBoxIconType::NoIcon);
            aw->addTextEditor ("name", track->getName());
            aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
            aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
            aw->enterModalState (true, ModalCallbackFunction::create ([this, aw, sp = SafePointer<ChannelStrip> (this)] (int r)
            {
                if (r == 1 && sp != nullptr) track->setName (aw->getTextEditorContents ("name"));
            }), true);
        }
    }

private:
    te::Track* track = nullptr;
    te::Edit* masterEdit = nullptr;
    int trackNumber = 0;
    bool master = false, dragging = false;
    int hoverSlot = -1;
    Slider panSlider { Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox };
    Component slotMenuAnchor;
    te::LevelMeasurer::Client meterClient;
    te::LevelMeasurer* attachedMeasurer = nullptr;
    float levels[2] { -100.0f, -100.0f }, peaks[2] { -100.0f, -100.0f }, maxPeak = 0.0f;
    int peakHold[2] {};

    static constexpr int insertsTop = 32, slotH = 18;

    void init()
    {
        addAndMakeVisible (panSlider);
        addChildComponent (slotMenuAnchor);
        panSlider.setRange (-1.0, 1.0, 0.01);
        panSlider.setDoubleClickReturnValue (true, 0.0);
        panSlider.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        if (auto v = getVolume()) panSlider.setValue (v->getPan(), dontSendNotification);
        panSlider.onValueChange = [this] { if (auto v = getVolume()) v->setPan ((float) panSlider.getValue()); };
        panSlider.setTooltip ("Panoramique (double-clic : centre)");
        setSize (master ? 134 : stripWidth, 900);
        startTimerHz (30);
    }

    te::AuxSendPlugin* draggingSend = nullptr;
    bool faderAlone = false;
    float sendStartDb = 0.0f;

    void sendClick (int index, const MouseEvent& e)
    {
        auto sends = Mixing::getSends (*track);
        auto* snd = index < sends.size() ? sends[index] : nullptr;
        auto& edit = getEdit();

        if (snd != nullptr && e.mods.isAltDown()) { snd->setGainDb (0.0f); repaint(); return; }

        if (snd != nullptr && ! e.mods.isPopupMenu())
        {
            draggingSend = snd;           // glisser vers le haut / la droite : plus fort
            sendStartDb = snd->getGainDb();
            return;
        }

        PopupMenu m;
        te::Plugin::Ptr keepSend (snd);
        te::Track::Ptr keepTrack (track);
        if (snd != nullptr)
        {
            m.addItem (1, snd->isMute() ? "Activer l'envoi" : "Couper l'envoi");
            m.addItem (2, "Supprimer l'envoi");
            m.addItem (3, String::fromUTF8 ("Niveau à 0 dB"));
        }
        else
        {
            m.addSectionHeader (String::fromUTF8 ("ENVOI VERS…"));
            auto fx = Mixing::getFxReturns (edit);
            for (auto& [bus, t] : fx)
                m.addItem (100 + bus, t->getName());
            if (! fx.empty()) m.addSeparator();
            m.addItem (10, String::fromUTF8 ("Nouvelle FX : Réverb"));
            m.addItem (11, String::fromUTF8 ("Nouvelle FX : Délai"));
            m.addItem (12, String::fromUTF8 ("Nouvelle FX vide"));
        }

        slotMenuAnchor.setBounds (sendRect (index));
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (&slotMenuAnchor),
                         [keepSend, keepTrack, &edit, sp = SafePointer<ChannelStrip> (this)] (int r)
        {
            if (r == 0) return;
            if (auto* s2 = dynamic_cast<te::AuxSendPlugin*> (keepSend.get()))
            {
                if (r == 1) s2->setMute (! s2->isMute());
                if (r == 2) s2->deleteFromParent();
                if (r == 3) s2->setGainDb (0.0f);
            }
            else if (keepTrack != nullptr)
            {
                if (r >= 100) Mixing::addSend (*keepTrack, r - 100);
                if (r >= 10 && r <= 12)
                    if (auto* fxTrack = Mixing::createFxTrack (edit, r == 10 ? Mixing::FxKind::reverb : r == 11 ? Mixing::FxKind::delay : Mixing::FxKind::empty))
                        if (auto* ret = Mixing::getAuxReturn (*fxTrack))
                            Mixing::addSend (*keepTrack, ret->busNumber.get());
            }
            if (sp != nullptr) sp->repaint();
        });
    }

    Array<te::Plugin*> getPlugins() const
    {
        if (masterEdit != nullptr)
        {
            Array<te::Plugin*> res;
            for (auto p : masterEdit->getMasterPluginList().getPlugins())
                res.add (p);
            return res;
        }
        return getInsertPlugins (*track);
    }

    void applyChoice (int slot, const InsertMenu::Choice& c)
    {
        auto& edit = getEdit();
        auto& list = masterEdit ? masterEdit->getMasterPluginList() : track->pluginList;
        auto plugins = getPlugins();

        if (c.manager) { showPluginManager (edit.engine); return; }

        if (c.remove)
        {
            if (slot < plugins.size()) plugins[slot]->deleteFromParent();
            repaint();
            return;
        }

        te::Plugin::Ptr p = c.builtinType.isNotEmpty()
                              ? edit.getPluginCache().createNewPlugin (c.builtinType, {})
                              : edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, c.desc);
        if (p == nullptr) return;

        int index = 0;
        if (slot < plugins.size())
        {
            index = list.indexOf (plugins[slot]);
            plugins[slot]->deleteFromParent();   // remplace le plugin existant
        }
        else if (! plugins.isEmpty())
        {
            index = list.indexOf (plugins.getLast()) + 1;
        }
        else if (track != nullptr)
        {
            if (auto* ret = Mixing::getAuxReturn (*track))   // piste FX : après le retour
                index = list.indexOf (ret) + 1;
        }

        list.insertPlugin (p, index, nullptr);
        p->showWindowExplicitly();
        repaint();
    }

    String inputName() const
    {
        if (master) return "Mix";
        if (auto* r = Mixing::getAuxReturn (*track)) return "Bus " + String (r->busNumber.get() + 1);
        if (EngineHelpers::isInstrumentTrack (*track)) return String::fromUTF8 ("MIDI · ") + EngineHelpers::instrumentName (*track);
        if (auto* at = dynamic_cast<te::AudioTrack*> (track))
            for (auto* in : at->edit.getAllInputDevices())
                if (te::isOnTargetTrack (*in, *at, 0))
                    return in->getInputDevice().getName();
        return String::fromUTF8 ("—");
    }

    static String shortName (String n)
    {
        return n.replace (String::fromUTF8 ("Chènevières "), String::fromUTF8 ("Chèn. "))
                .replace ("Chenevieres ", String::fromUTF8 ("Chèn. "));
    }

    void sectionLabel (Graphics& g, int y, const String& s) const
    {
        g.setColour (Col::text3);
        g.setFont (uiFont (9.0f, true));
        g.drawText (s, 5, y, getWidth() - 10, 11, Justification::centredLeft);
    }

    void drawField (Graphics& g, Rectangle<float> r, const String& s, Justification j = Justification::centredLeft, Colour tc = Col::text) const
    {
        g.setColour (Colour (0xff121315));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (Colour (0xff0a0a0b));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);
        g.setColour (tc);
        g.setFont (uiFont (10.5f));
        g.drawText (s, r.reduced (4, 0), j, true);
    }

    Rectangle<int> slotRect (int i) const
    {
        int y = insertsTop + i * slotH + (i >= 5 ? 16 : 0);
        return { 4, y, getWidth() - 8, slotH - 2 };
    }

    int slotAt (Point<int> p) const
    {
        for (int i = 0; i < numInserts; ++i)
            if (slotRect (i).contains (p)) return i;
        return -1;
    }

    static constexpr int numSends = 5;
    Rectangle<int> sendRect (int i) const { return { 4, slotRect (9).getBottom() + 16 + i * slotH, getWidth() - 8, slotH - 2 }; }
    int sendAt (Point<int> p) const
    {
        for (int i = 0; i < numSends; ++i) if (sendRect (i).contains (p)) return i;
        return -1;
    }
    int ioTop() const              { return (master ? slotRect (9).getBottom() : sendRect (numSends - 1).getBottom()) + 18; }
    Rectangle<int> buttonsRect() const { return { 6, ioTop() + 80, getWidth() - 12, 18 }; }
    Rectangle<int> nameRect() const    { return { 4, getHeight() - 28, getWidth() - 8, 24 }; }
    Rectangle<int> faderArea() const
    {
        auto top = buttonsRect().getBottom() + 14;
        return { 6, top, getWidth() - 12, getHeight() - 30 - 28 - top };
    }

    void setFaderFromY (int y)
    {
        auto fr = faderArea();
        auto p = jlimit (0.0f, 1.0f, (float) (fr.getBottom() - y) / (float) fr.getHeight());
        if (auto* at = dynamic_cast<te::AudioTrack*> (track); at != nullptr && masterEdit == nullptr)
            Groups::setVolume (*at, faderPosToDb (p), faderAlone);
        else if (auto v = getVolume())
            v->setVolumeDb (faderPosToDb (p));
        repaint();
    }

    te::LevelMeasurer* findMeasurer()
    {
        if (masterEdit != nullptr)
        {
            if (auto* ctx = masterEdit->getCurrentPlaybackContext())
                return &ctx->masterLevels;
            return nullptr;
        }
        if (auto* at = dynamic_cast<te::AudioTrack*> (track))
            if (auto* lm = at->getLevelMeterPlugin())
                return &lm->measurer;
        return nullptr;
    }

    void timerCallback() override
    {
        auto* m = findMeasurer();
        if (m != attachedMeasurer)
        {
            if (attachedMeasurer != nullptr) attachedMeasurer->removeClient (meterClient);
            attachedMeasurer = m;
            if (m != nullptr) m->addClient (meterClient);
        }

        for (int ch = 0; ch < 2; ++ch)
        {
            auto l = meterClient.getAndClearAudioLevel (ch).dB;
            levels[ch] = jmax (l, levels[ch] - 1.6f);   // retombée douce
            if (l >= peaks[ch]) { peaks[ch] = l; peakHold[ch] = 45; }
            else if (--peakHold[ch] <= 0) peaks[ch] = jmax (-100.0f, peaks[ch] - 0.8f);
            if (l > 0.0f) maxPeak = l;
        }

        if (! panSlider.isMouseButtonDown())
            if (auto v = getVolume()) panSlider.setValue (v->getPan(), dontSendNotification);

        repaint (faderArea().expanded (2, 30));
        if (timerTick++ % 8 == 0) repaint();
    }

    int timerTick = 0;
};

//==============================================================================
class ConsoleComponent : public Component, private ValueTree::Listener, private AsyncUpdater
{
public:
    explicit ConsoleComponent (te::Edit& e) : edit (e)
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (false, true);
        edit.state.addListener (this);
        rebuild();
    }

    ~ConsoleComponent() override { edit.state.removeListener (this); }

    void paint (Graphics& g) override
    {
        g.fillAll (Colour (0xff0e0f11));
        auto top = getLocalBounds().removeFromTop (30);
        g.setGradientFill (ColourGradient (Colour (0xff26272c), 0, 0, Colour (0xff1f2024), 0, 30, false));
        g.fillRect (top);
        g.setColour (Colours::white);
        g.setFont (uiFont (13.0f, true));
        g.drawText ("CONSOLE", 12, 0, 100, 30, Justification::centredLeft);
        g.setColour (Col::text3);
        g.setFont (uiFont (11.5f));
        g.drawText (String::fromUTF8 ("F3 : revenir à l'Édition   ·   clic sur un insert vide : menu plug-in   ·   Ctrl+clic : bypass   ·   clic droit : remplacer / retirer   ·   Alt+clic fader : 0 dB"),
                    110, 0, getWidth() - 120, 30, Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromTop (30);
        viewport.setBounds (r);
        layoutStrips();
    }

private:
    te::Edit& edit;
    Viewport viewport;
    Component content;
    OwnedArray<ChannelStrip> strips;

    void rebuild()
    {
        strips.clear();
        int n = 1;
        for (auto* t : te::getAudioTracks (edit))
            content.addAndMakeVisible (strips.add (new ChannelStrip (*t, n++)));
        content.addAndMakeVisible (strips.add (new ChannelStrip (edit)));
        layoutStrips();
    }

    void layoutStrips()
    {
        auto h = jmax (600, viewport.getHeight() - viewport.getScrollBarThickness());
        int x = 1;
        for (auto* s : strips)
        {
            if (s == strips.getLast()) x += 10;
            s->setBounds (x, 0, s->getWidth(), h);
            x += s->getWidth() + 1;
        }
        content.setSize (jmax (x, viewport.getWidth()), h);
    }

    void handleAsyncUpdate() override { rebuild(); }

    void valueTreeChildAdded (ValueTree& p, ValueTree& c) override         { check (p, c); }
    void valueTreeChildRemoved (ValueTree& p, ValueTree& c, int) override  { check (p, c); }
    void valueTreeChildOrderChanged (ValueTree& p, int, int) override       { if (p == edit.state) triggerAsyncUpdate(); }

    void check (ValueTree& p, ValueTree& c)
    {
        if (p == edit.state && te::TrackList::isTrack (c))
            triggerAsyncUpdate();
    }
};

//==============================================================================
class ConsoleWindow : public DocumentWindow
{
public:
    ConsoleWindow (te::Edit& e, std::function<void()> onCloseFn)
        : DocumentWindow (String::fromUTF8 ("ChènRecPro — Console"), Col::bg, DocumentWindow::allButtons),
          onClose (std::move (onCloseFn))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new ConsoleComponent (e), false);
        setResizable (true, false);
        auto area = Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        setBounds (area.reduced (40));
        setVisible (true);
    }

    void closeButtonPressed() override { if (onClose) onClose(); }

    /** Les raccourcis de l'appli marchent aussi quand la console a le focus (F3 la referme). */
    std::function<bool (const KeyPress&)> forwardKey;

    bool keyPressed (const KeyPress& k) override
    {
        if (forwardKey && forwardKey (k)) return true;
        return DocumentWindow::keyPressed (k);
    }

private:
    std::function<void()> onClose;
};

} // namespace chen
