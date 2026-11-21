#pragma once

// =============================================================================
//  Fenêtre de plugin « façon Pro Tools » :
//  barre de commandes (piste, insert, préréglages, Comparer, Bypass, latence)
//  au-dessus de l'interface du plugin.
// =============================================================================

namespace chen
{

/** Les plugins « utilisateur » d'une piste (hors volume/pan et mesure internes). */
inline Array<te::Plugin*> getInsertPlugins (te::Track& t)
{
    Array<te::Plugin*> res;
    for (auto p : t.pluginList.getPlugins())
    {
        if (dynamic_cast<te::VolumeAndPanPlugin*> (p) != nullptr) continue;
        if (dynamic_cast<te::LevelMeterPlugin*> (p) != nullptr) continue;
        if (dynamic_cast<te::AuxSendPlugin*> (p) != nullptr) continue;
        if (dynamic_cast<te::AuxReturnPlugin*> (p) != nullptr) continue;
        res.add (p);
    }
    return res;
}

inline String insertLetter (int index)  { return String::charToString ((juce_wchar) ('a' + jlimit (0, 9, index))); }

//==============================================================================
struct ExternalEditorContent : public te::Plugin::EditorComponent
{
    explicit ExternalEditorContent (te::ExternalPlugin& p) : plugin (p)
    {
        JUCE_AUTORELEASEPOOL
        {
            if (auto pi = plugin.getAudioPluginInstance())
            {
                editor.reset (pi->createEditorAndMakeActive());
                if (editor == nullptr)
                    editor = std::make_unique<GenericAudioProcessorEditor> (*pi);
                addAndMakeVisible (*editor);
            }
        }
        fit();
    }

    bool allowWindowResizing() override  { return false; }
    ComponentBoundsConstrainer* getBoundsConstrainer() override { return editor ? editor->getConstrainer() : nullptr; }
    void resized() override              { if (editor) editor->setBounds (getLocalBounds()); }
    void childBoundsChanged (Component* c) override
    {
        if (c == editor.get()) { plugin.edit.pluginChanged (plugin); fit(); }
    }
    void fit() { setSize (jmax (200, editor ? editor->getWidth() : 0), jmax (80, editor ? editor->getHeight() : 0)); }

    te::ExternalPlugin& plugin;
    std::unique_ptr<AudioProcessorEditor> editor;
};

//==============================================================================
/** Interface automatique (boutons rotatifs) pour les plugins intégrés sans GUI. */
class BuiltinEditor : public te::Plugin::EditorComponent, private Timer
{
public:
    explicit BuiltinEditor (te::Plugin& p) : plugin (p)
    {
        for (auto* param : plugin.getAutomatableParameters())
        {
            auto* k = knobs.add (new Knob (*param));
            content.addAndMakeVisible (k->slider);
            content.addAndMakeVisible (k->label);
        }
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&content, false);
        viewport.setScrollBarsShown (true, false);

        cols = knobs.size() > 24 ? 8 : jlimit (1, 6, knobs.size());
        auto rows = (knobs.size() + cols - 1) / jmax (1, cols);
        content.setSize (cols * 110 + 20, rows * 130 + 10);
        setSize (jmax (480, content.getWidth() + 24), jlimit (140, 660, content.getHeight() + 60));
        startTimerHz (10);
    }

    bool allowWindowResizing() override { return false; }
    ComponentBoundsConstrainer* getBoundsConstrainer() override { return nullptr; }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff2f2b25), getWidth() * 0.3f, 0, Colour (0xff1b1915), getWidth() * 0.6f, (float) getHeight(), true));
        g.fillAll();
        g.setColour (Colour (0xfff2e6d0));
        g.setFont (uiFont (18.0f, true));
        g.drawText (plugin.getName().toUpperCase(), 20, 12, getWidth() - 40, 24, Justification::centredLeft);
        g.setColour (Colour (0xff8a7f6c));
        g.setFont (uiFont (10.0f, true));
        g.drawText (String::fromUTF8 ("CHÈNRECPRO · PLUGIN INTÉGRÉ"), 20, 34, getWidth() - 40, 14, Justification::centredLeft);
        if (knobs.isEmpty())
        {
            g.setColour (Col::text3);
            g.drawText (String::fromUTF8 ("Aucun réglage pour ce plugin"), getLocalBounds(), Justification::centred);
        }
    }

    void resized() override
    {
        viewport.setBounds (getLocalBounds().withTrimmedTop (54));
        int i = 0;
        for (auto* k : knobs)
        {
            int x = 10 + (i % cols) * 110, y = 6 + (i / cols) * 130;
            k->slider.setBounds (x + 15, y, 80, 92);
            k->label.setBounds (x, y + 92, 110, 16);
            ++i;
        }
    }

private:
    struct Knob
    {
        explicit Knob (te::AutomatableParameter& p) : param (p)
        {
            slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (Slider::TextBoxBelow, false, 80, 18);
            auto r = param.getValueRange();
            slider.setRange (r.getStart(), r.getEnd(), 0.0);
            slider.setValue (param.getCurrentValue(), dontSendNotification);
            if (auto d = param.getDefaultValue()) slider.setDoubleClickReturnValue (true, *d);
            slider.textFromValueFunction = [this] (double v) { return (param.valueToString ((float) v) + " " + param.getLabel()).trim(); };
            slider.valueFromTextFunction = [this] (const String& t) { return (double) param.stringToValue (t); };
            slider.updateText();
            slider.onValueChange = [this] { param.setParameter ((float) slider.getValue(), sendNotification); };
            slider.setColour (Slider::textBoxTextColourId, Col::accent);
            slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
            label.setText (param.getParameterName().toUpperCase(), dontSendNotification);
            label.setJustificationType (Justification::centred);
            label.setFont (uiFont (10.5f, true));
            label.setColour (Label::textColourId, Colour (0xffb9ad97));
        }
        te::AutomatableParameter& param;
        Slider slider;
        Label label;
    };

    te::Plugin& plugin;
    OwnedArray<Knob> knobs;
    Viewport viewport;
    Component content;
    int cols = 6;

    void timerCallback() override
    {
        for (auto* k : knobs)
            if (! k->slider.isMouseButtonDown())
                k->slider.setValue (k->param.getCurrentValue(), dontSendNotification);
    }
};

//==============================================================================
class PluginHeader : public Component, private Timer
{
public:
    explicit PluginHeader (te::Plugin& p) : plugin (p)
    {
        for (auto* b : { &prevBtn, &nextBtn, &compareBtn, &bypassBtn, &autoBtn })
            addAndMakeVisible (*b);

        addAndMakeVisible (presetBox);
        presetBox.setTextWhenNothingSelected (String::fromUTF8 ("<réglage d'usine>"));

        compareBtn.setClickingTogglesState (true);
        bypassBtn.setClickingTogglesState (true);
        bypassBtn.setToggleState (! plugin.isEnabled(), dontSendNotification);
        bypassBtn.onClick = [this] { plugin.setEnabled (! bypassBtn.getToggleState()); };

        if (auto* pi = getInstance())
        {
            for (int i = 0; i < pi->getNumPrograms(); ++i)
                presetBox.addItem (pi->getProgramName (i).isNotEmpty() ? pi->getProgramName (i) : "Programme " + String (i + 1), i + 1);
            presetBox.setSelectedId (pi->getCurrentProgram() + 1, dontSendNotification);
            presetBox.onChange = [this] { if (auto* p2 = getInstance()) p2->setCurrentProgram (presetBox.getSelectedId() - 1); };
            prevBtn.onClick = [this] { step (-1); };
            nextBtn.onClick = [this] { step (1); };

            // « Comparer » : A = état à l'ouverture, B = réglage en cours
            pi->getStateInformation (stateA);
            compareBtn.onClick = [this]
            {
                if (auto* p2 = getInstance())
                {
                    if (compareBtn.getToggleState()) { p2->getStateInformation (stateB); p2->setStateInformation (stateA.getData(), (int) stateA.getSize()); }
                    else                              { p2->setStateInformation (stateB.getData(), (int) stateB.getSize()); }
                }
            };
        }
        else
        {
            presetBox.setEnabled (false);
            compareBtn.setEnabled (false);
        }

        autoBtn.setTooltip (String::fromUTF8 ("Automation des paramètres (prochaine version)"));
        autoBtn.setEnabled (false);
        startTimerHz (4);
    }

    void paint (Graphics& g) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff2a2b30), 0, 0, Colour (0xff232428), 0, (float) getHeight(), false));
        g.fillAll();
        g.setColour (Col::line);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        auto label = [&] (int x, int y, const String& s)
        {
            g.setColour (Col::text3);
            g.setFont (uiFont (10.0f, true));
            g.drawText (s, x, y, 80, 22, Justification::centredLeft);
        };

        label (8, 6, "PISTE");
        label (8, 34, "PLUG-IN");

        auto field = [&] (Rectangle<int> r, const String& s, Colour dot)
        {
            g.setColour (Colour (0xff131416));
            g.fillRoundedRectangle (r.toFloat(), 3.0f);
            g.setColour (Colour (0xff08090a));
            g.drawRoundedRectangle (r.toFloat(), 3.0f, 1.0f);
            int x = r.getX() + 8;
            if (! dot.isTransparent())
            {
                g.setColour (dot);
                g.fillRoundedRectangle ((float) x, r.getCentreY() - 4.0f, 8.0f, 8.0f, 2.0f);
                x += 14;
            }
            g.setColour (Col::text);
            g.setFont (uiFont (12.5f));
            g.drawText (s, x, r.getY(), r.getRight() - x - 6, r.getHeight(), Justification::centredLeft, true);
        };

        auto* track = plugin.getOwnerTrack();
        field ({ 62, 6, 150, 22 }, track ? track->getName() : String (String::fromUTF8 ("—")), track ? track->getColour() : Colours::transparentBlack);

        int slot = track ? getInsertPlugins (*track).indexOf (&plugin) : -1;
        field ({ 216, 6, 34, 22 }, slot >= 0 ? insertLetter (slot) : "-", Colours::transparentBlack);
        field ({ 62, 34, 188, 22 }, plugin.getName(), Colours::transparentBlack);

        g.setColour (Col::text3);
        g.setFont (uiFont (10.0f, true));
        g.drawText ("LATENCE " + String (roundToInt (plugin.getLatencySeconds() * plugin.edit.engine.getDeviceManager().getSampleRate())) + String::fromUTF8 (" éch."),
                    getWidth() - 130, 34, 122, 22, Justification::centredRight);
        g.drawText (String::fromUTF8 ("PRÉRÉGLAGE"), 262, 6, 80, 22, Justification::centredLeft);
    }

    void resized() override
    {
        int x = 340;
        prevBtn.setBounds (x, 6, 24, 22);
        nextBtn.setBounds (getWidth() - 32, 6, 24, 22);
        presetBox.setBounds (x + 28, 6, nextBtn.getX() - x - 32, 22);
        bypassBtn.setBounds (262, 34, 80, 22);
        autoBtn.setBounds (346, 34, 64, 22);
        compareBtn.setBounds (414, 34, 96, 22);
    }

private:
    te::Plugin& plugin;
    TextButton prevBtn { String::fromUTF8 ("◂") }, nextBtn { String::fromUTF8 ("▸") },
               compareBtn { "COMPARER" }, bypassBtn { "BYPASS" }, autoBtn { "AUTO" };
    ComboBox presetBox;
    MemoryBlock stateA, stateB;

    AudioPluginInstance* getInstance() const
    {
        if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&plugin))
            return ext->getAudioPluginInstance();
        return nullptr;
    }

    void step (int d)
    {
        if (auto* pi = getInstance(); pi != nullptr && pi->getNumPrograms() > 0)
        {
            auto n = (pi->getCurrentProgram() + d + pi->getNumPrograms()) % pi->getNumPrograms();
            pi->setCurrentProgram (n);
            presetBox.setSelectedId (n + 1, dontSendNotification);
        }
    }

    void timerCallback() override
    {
        bypassBtn.setToggleState (! plugin.isEnabled(), dontSendNotification);
        repaint();
    }
};

//==============================================================================
class PluginContainer : public Component
{
public:
    PluginContainer (te::Plugin& p, std::unique_ptr<te::Plugin::EditorComponent> ed)
        : header (p), editor (std::move (ed))
    {
        addAndMakeVisible (header);
        if (editor) addAndMakeVisible (*editor);
        updateSize();
    }

    void updateSize()
    {
        auto w = jmax (560, editor ? editor->getWidth() : 0);
        auto h = headerHeight + (editor ? editor->getHeight() : 120);
        setSize (w, h);
    }

    void childBoundsChanged (Component* c) override { if (c == editor.get()) updateSize(); }

    void resized() override
    {
        auto r = getLocalBounds();
        header.setBounds (r.removeFromTop (headerHeight));
        if (editor)
            editor->setBounds (r.withSizeKeepingCentre (editor->getWidth(), editor->getHeight()).withY (headerHeight));
    }

    void paint (Graphics& g) override { g.fillAll (Colour (0xff1b1c20)); }

    static constexpr int headerHeight = 64;
    PluginHeader header;
    std::unique_ptr<te::Plugin::EditorComponent> editor;
};

//==============================================================================
class ChenPluginWindow : public DocumentWindow
{
public:
    explicit ChenPluginWindow (te::Plugin& p)
        : DocumentWindow (makeTitle (p), Colour (0xff1b1c20), DocumentWindow::closeButton | DocumentWindow::minimiseButton),
          plugin (p)
    {
        setUsingNativeTitleBar (true);
        auto ed = plugin.createEditor();
        if (ed == nullptr)
        {
            if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&plugin))
                ed = std::make_unique<ExternalEditorContent> (*ext);
            else
                ed = std::make_unique<BuiltinEditor> (plugin);
        }

        setContentOwned (new PluginContainer (plugin, std::move (ed)), true);
        setResizable (false, false);
        setAlwaysOnTop (true);

        if (plugin.windowState->lastWindowBounds.has_value())
            setTopLeftPosition (plugin.windowState->lastWindowBounds->getPosition());
        else
            centreWithSize (getWidth(), getHeight());

        setVisible (true);
    }

    ~ChenPluginWindow() override
    {
        plugin.edit.flushPluginStateIfNeeded (plugin);
        clearContentComponent();
    }

    void closeButtonPressed() override { plugin.windowState->closeWindowExplicitly(); }

    void moved() override
    {
        plugin.windowState->lastWindowBounds = getBounds();
        plugin.edit.pluginChanged (plugin);
    }

    static String makeTitle (te::Plugin& p)
    {
        String t;
        if (auto* tr = p.getOwnerTrack())
        {
            auto idx = getInsertPlugins (*tr).indexOf (&p);
            t << tr->getName() << String::fromUTF8 (" · Insert ") << (idx >= 0 ? insertLetter (idx) : String ("-")) << String::fromUTF8 (" · ");
        }
        return t + p.getName();
    }

private:
    te::Plugin& plugin;
};

//==============================================================================
class ChenUIBehaviour : public te::UIBehaviour
{
public:
    std::unique_ptr<Component> createPluginWindow (te::PluginWindowState& pws) override
    {
        if (auto* ws = dynamic_cast<te::Plugin::WindowState*> (&pws))
        {
            if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&ws->plugin))
                if (ext->getAudioPluginInstance() == nullptr)
                    return {};

            return std::make_unique<ChenPluginWindow> (ws->plugin);
        }
        return {};
    }
};

} // namespace chen
