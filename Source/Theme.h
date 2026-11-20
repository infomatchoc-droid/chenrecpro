#pragma once

// =============================================================================
//  ChènRecPro — Studio des Chènevières
//  Couleurs, polices et petits outils de dessin partagés par toute l'interface.
// =============================================================================

namespace chen
{
using namespace juce;

namespace Col
{
    const Colour bg       { 0xff141518 };
    const Colour panel1   { 0xff1c1d21 };
    const Colour panel2   { 0xff232429 };
    const Colour panel3   { 0xff2a2c32 };
    const Colour line     { 0xff0c0d0f };
    const Colour line2    { 0xff2e3036 };
    const Colour text     { 0xffc9ccd2 };
    const Colour text2    { 0xff8b8f98 };
    const Colour text3    { 0xff5d616a };
    const Colour accent   { 0xfff0a53a };
    const Colour lcd      { 0xffffb648 };
    const Colour rec      { 0xffe5484d };
    const Colour play     { 0xff3fb950 };
    const Colour select   { 0xff4c8dff };
    const Colour meterLo  { 0xff2fa84f };
    const Colour meterMid { 0xff7fd36a };
    const Colour meterHi  { 0xffe6c34a };
    const Colour meterClip{ 0xffe5484d };

    inline Colour track (int index)
    {
        static const uint32 palette[] = { 0xffe0726c, 0xffe3a08f, 0xffa687e6, 0xff6aa8ea,
                                          0xff4fc2b5, 0xff62c27f, 0xffd0a46a, 0xffd8b765,
                                          0xff8fa0b5, 0xffb79bff };
        return Colour (palette[(size_t) std::abs (index) % std::size (palette)]);
    }
}

inline Font uiFont (float h, bool bold = false)
{
    return Font (FontOptions (h, bold ? Font::bold : Font::plain));
}

inline Font monoFont (float h)
{
    return Font (FontOptions (Font::getDefaultMonospacedFontName(), h, Font::plain));
}

/** Pistache, le beagle du Studio des Chènevières. */
inline std::unique_ptr<Drawable> createPistache()
{
    static const char* svg = R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
      <path d="M14 20c-5 7-5 20 1 26 3 3 7 0 7-4V23c0-4-5-6-8-3z" fill="#8a5630"/>
      <path d="M50 20c5 7 5 20-1 26-3 3-7 0-7-4V23c0-4 5-6 8-3z" fill="#8a5630"/>
      <path d="M19 26c0-10 6-15 13-15s13 5 13 15v9c0 10-6 17-13 17s-13-7-13-17z" fill="#d08f50"/>
      <path d="M29 11h6v21c0 2-1 3-3 3s-3-1-3-3z" fill="#f2ece2"/>
      <path d="M23 41c2 8 6 11 9 11s7-3 9-11c-4-2-14-2-18 0z" fill="#f2ece2"/>
      <circle cx="25.5" cy="30" r="2.6" fill="#120d0a"/><circle cx="38.5" cy="30" r="2.6" fill="#120d0a"/>
      <ellipse cx="32" cy="42" rx="4" ry="2.8" fill="#120d0a"/></svg>)";

    return Drawable::createFromSVGString (String (svg));
}

/** Dessine un bouton-pastille façon console (M, S, ●, I…). */
inline void drawPill (Graphics& g, Rectangle<float> r, const String& txt, bool on, Colour onColour, Colour onText = Colours::black)
{
    g.setColour (on ? onColour : Col::panel3.brighter (0.05f));
    g.fillRoundedRectangle (r, 2.0f);
    g.setColour (Col::line);
    g.drawRoundedRectangle (r, 2.0f, 1.0f);
    g.setColour (on ? onText : Col::text2);
    g.setFont (uiFont (r.getHeight() * 0.62f, true));
    g.drawText (txt, r, Justification::centred);
}

/** Vu-mètre segmenté vertical. level = 0..1 */
inline void drawMeter (Graphics& g, Rectangle<float> r, float level, float peak)
{
    g.setColour (Colour (0xff0b0c0d));
    g.fillRoundedRectangle (r, 1.0f);

    auto filled = r.withTop (r.getBottom() - r.getHeight() * jlimit (0.0f, 1.0f, level));
    ColourGradient grad (Col::meterLo, r.getX(), r.getBottom(), Col::meterClip, r.getX(), r.getY(), false);
    grad.addColour (0.62, Col::meterMid);
    grad.addColour (0.84, Col::meterHi);
    g.setGradientFill (grad);
    g.fillRect (filled);

    g.setColour (Colour (0xff0b0c0d));
    for (float y = r.getBottom() - 3.0f; y > r.getY(); y -= 3.0f)
        g.fillRect (r.getX(), y, r.getWidth(), 1.0f);

    if (peak > 0.001f)
    {
        g.setColour (Col::meterHi);
        g.fillRect (r.getX(), r.getBottom() - r.getHeight() * jlimit (0.0f, 1.0f, peak), r.getWidth(), 1.0f);
    }
}

/** Convertit des dB en position de mètre (0..1), échelle type console. */
inline float dbToMeter (float db)
{
    if (db <= -60.0f) return 0.0f;
    if (db >= 6.0f)   return 1.0f;
    // courbe douce : plus de résolution en haut
    return std::pow ((db + 60.0f) / 66.0f, 2.2f);
}

/** Position de fader (0..1) <-> dB, échelle Pro Tools approximative. */
inline float faderPosToDb (float p)
{
    if (p <= 0.0f) return -100.0f;
    return jmap (std::pow (p, 0.5f), 0.0f, 1.0f, -60.0f, 0.0f) + (p > 0.78f ? (p - 0.78f) / 0.22f * 6.0f : 0.0f);
}

inline float dbToFaderPos (float db)
{
    // inversion numérique simple (précision largement suffisante pour l'affichage)
    float lo = 0.0f, hi = 1.0f;
    for (int i = 0; i < 30; ++i)
    {
        auto mid = (lo + hi) * 0.5f;
        if (faderPosToDb (mid) < db) lo = mid; else hi = mid;
    }
    return (lo + hi) * 0.5f;
}

inline String dbString (float db)
{
    if (db <= -99.0f) return String::fromUTF8 ("-\xe2\x88\x9e");
    return (db > 0.05f ? "+" : "") + String (db, 1);
}

} // namespace chen
