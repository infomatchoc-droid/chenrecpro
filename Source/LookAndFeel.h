#pragma once

namespace chen
{

/** Habillage global de ChènRecPro : sombre, sobre, accent ambre. */
class ChenLookAndFeel : public LookAndFeel_V4
{
public:
    ChenLookAndFeel()
    {
        auto scheme = LookAndFeel_V4::getDarkColourScheme();
        scheme.setUIColour (ColourScheme::windowBackground, Col::bg);
        scheme.setUIColour (ColourScheme::widgetBackground, Col::panel2);
        scheme.setUIColour (ColourScheme::menuBackground, Col::panel2);
        scheme.setUIColour (ColourScheme::outline, Col::line);
        scheme.setUIColour (ColourScheme::defaultText, Col::text);
        scheme.setUIColour (ColourScheme::defaultFill, Col::panel3);
        scheme.setUIColour (ColourScheme::highlightedText, Colours::white);
        scheme.setUIColour (ColourScheme::highlightedFill, Colour (0xff3a5fa8));
        scheme.setUIColour (ColourScheme::menuText, Col::text);
        setColourScheme (scheme);

        setColour (PopupMenu::backgroundColourId, Colour (0xff26272c));
        setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff3a5fa8));
        setColour (PopupMenu::headerTextColourId, Col::text3);
        setColour (TextButton::buttonColourId, Col::panel3);
        setColour (TextButton::buttonOnColourId, Colour (0xff3a2f1c));
        setColour (TextButton::textColourOnId, Col::accent);
        setColour (TextButton::textColourOffId, Col::text);
        setColour (ComboBox::backgroundColourId, Colour (0xff131416));
        setColour (ComboBox::outlineColourId, Colour (0xff08090a));
        setColour (ResizableWindow::backgroundColourId, Col::bg);
        setColour (DocumentWindow::textColourId, Col::text);
        setColour (ListBox::backgroundColourId, Col::panel1);
        setColour (TextEditor::backgroundColourId, Colour (0xff131416));
        setColour (TextEditor::outlineColourId, Colour (0xff08090a));
        setColour (Slider::thumbColourId, Col::accent);
        setColour (Slider::trackColourId, Col::panel3);
        setColour (Slider::rotarySliderFillColourId, Col::accent);
        setColour (Slider::rotarySliderOutlineColourId, Colour (0xff131416));
        setColour (ScrollBar::thumbColourId, Col::panel3.brighter (0.2f));
        setColour (TooltipWindow::backgroundColourId, Colour (0xff26272c));
        setColour (TooltipWindow::textColourId, Col::text);
        setColour (TooltipWindow::outlineColourId, Col::line);
    }

    Font getPopupMenuFont() override                     { return uiFont (13.5f); }
    Font getMenuBarFont (MenuBarComponent&, int, const String&) override { return uiFont (13.0f); }
    Font getTextButtonFont (TextButton&, int h) override { return uiFont (jmin (13.0f, h * 0.55f), true); }

    void drawMenuBarBackground (Graphics& g, int w, int h, bool, MenuBarComponent&) override
    {
        g.fillAll (Colour (0xff0f1012));
        g.setColour (Col::line);
        g.fillRect (0, h - 1, w, 1);
    }

    void drawMenuBarItem (Graphics& g, int w, int h, int, const String& text, bool isMouseOver,
                          bool isMenuOpen, bool, MenuBarComponent&) override
    {
        if (isMenuOpen || isMouseOver)
        {
            g.setColour (Col::panel3);
            g.fillRect (0, 2, w, h - 4);
        }
        g.setColour (isMenuOpen ? Colours::white : Colour (0xffa7abb3));
        g.setFont (uiFont (13.0f));
        g.drawFittedText (text, 0, 0, w, h, Justification::centred, 1);
    }

    void drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto on = b.getToggleState();
        auto base = on ? findColour (TextButton::buttonOnColourId) : findColour (TextButton::buttonColourId);
        if (over) base = base.brighter (0.08f);
        if (down) base = base.darker (0.15f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? Col::accent.withAlpha (0.45f) : Col::line);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }

    void drawDocumentWindowTitleBar (DocumentWindow& w, Graphics& g, int width, int height,
                                     int titleSpaceX, int titleSpaceW, const Image*, bool) override
    {
        g.setGradientFill (ColourGradient (Colour (0xff34363c), 0, 0, Colour (0xff2a2c31), 0, (float) height, false));
        g.fillAll();
        g.setColour (Col::line);
        g.fillRect (0, height - 1, width, 1);
        g.setColour (Col::text);
        g.setFont (uiFont (13.0f));
        g.drawText (w.getName(), titleSpaceX, 0, titleSpaceW, height, Justification::centredLeft, true);
    }
};

} // namespace chen
