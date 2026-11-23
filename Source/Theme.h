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

/** Pistache, le beagle du Studio des Chènevières (photo de sa tête). */
inline Image getPistacheImage()
{
    return ImageCache::getFromMemory (BinaryData::pistache_png, BinaryData::pistache_pngSize);
}

/** Dessine la tête de Pistache dans un cadre arrondi, façon logo. */
inline void drawPistache (Graphics& g, Rectangle<float> area, float corner = 6.0f, bool border = true)
{
    auto img = getPistacheImage();
    if (! img.isValid()) return;
    Graphics::ScopedSaveState s (g);
    Path clip; clip.addRoundedRectangle (area, corner);
    g.reduceClipRegion (clip);
    g.drawImage (img, area, RectanglePlacement::centred | RectanglePlacement::fillDestination);
    if (border)
    {
        g.setColour (Col::accent.withAlpha (0.8f));
        g.strokePath (clip, PathStrokeType (1.2f));
    }
}

/** Dessine un bouton-pastille façon console (M, S, ●, I…). */
/** Flocon dessiné (le glyphe ❄ manque dans certaines polices Windows). */
inline void drawSnowflake (Graphics& g, Point<float> c, float radius, Colour col)
{
    g.setColour (col);
    for (int i = 0; i < 3; ++i)
    {
        auto a = MathConstants<float>::pi * (float) i / 3.0f + MathConstants<float>::halfPi;
        auto d = Point<float> (std::cos (a), std::sin (a)) * radius;
        g.drawLine ({ c - d, c + d }, 1.4f);
        for (float sgn : { -1.0f, 1.0f })
        {
            auto tip = c + d * sgn;
            auto mid = c + d * (sgn * 0.55f);
            auto n = Point<float> (-d.y, d.x) * 0.28f;
            g.drawLine ({ mid, tip - d * (sgn * 0.12f) + n }, 1.0f);
            g.drawLine ({ mid, tip - d * (sgn * 0.12f) - n }, 1.0f);
        }
    }
}

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
