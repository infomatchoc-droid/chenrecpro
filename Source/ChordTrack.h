#pragma once

// =============================================================================
//  Piste d'accords (façon Chord Track de Cubase) : une grille d'accords posée
//  sur la timeline, qu'on écoute, qu'on transforme en MIDI, et que les notes
//  des pistes MIDI peuvent suivre.
//  Stockée dans le projet : CHENCHORDS > CHORD (beat, root, quality, bass).
// =============================================================================

namespace chen::Chords
{

struct Chord
{
    double beat = 0.0;   // position (temps)
    int root = 0;        // 0 = Do / C
    String quality;      // "", "m", "7", "maj7", "m7"…
    int bass = -1;       // basse différente (accord renversé : C/E), -1 = aucune
};

struct Quality { const char* id; const char* label; std::vector<int> intervals; };

inline const std::vector<Quality>& qualities()
{
    static const std::vector<Quality> q {
        { "",      "majeur",            { 0, 4, 7 } },
        { "m",     "mineur",            { 0, 3, 7 } },
        { "7",     "7 (septième)",      { 0, 4, 7, 10 } },
        { "maj7",  "maj7",              { 0, 4, 7, 11 } },
        { "m7",    "m7",                { 0, 3, 7, 10 } },
        { "6",     "6",                 { 0, 4, 7, 9 } },
        { "m6",    "m6",                { 0, 3, 7, 9 } },
        { "9",     "9",                 { 0, 4, 7, 10, 14 } },
        { "add9",  "add9",              { 0, 4, 7, 14 } },
        { "m9",    "m9",                { 0, 3, 7, 10, 14 } },
        { "sus2",  "sus2",              { 0, 2, 7 } },
        { "sus4",  "sus4",              { 0, 5, 7 } },
        { "7sus4", "7sus4",             { 0, 5, 7, 10 } },
        { "dim",   "dim (diminué)",     { 0, 3, 6 } },
        { "dim7",  "dim7",              { 0, 3, 6, 9 } },
        { "m7b5",  "m7♭5 (demi-dim.)",  { 0, 3, 6, 10 } },
        { "aug",   "aug (augmenté)",    { 0, 4, 8 } },
    };
    return q;
}

inline std::vector<int> intervalsOf (const String& quality)
{
    for (auto& q : qualities())
        if (quality == q.id) return q.intervals;
    return { 0, 4, 7 };
}

inline String noteName (int pc, bool latin)
{
    static const char* en[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    static const char* fr[] = { "Do", "Do#", "Ré", "Mib", "Mi", "Fa", "Fa#", "Sol", "Lab", "La", "Sib", "Si" };
    pc = ((pc % 12) + 12) % 12;
    return String::fromUTF8 (latin ? fr[pc] : en[pc]);
}

inline String name (const Chord& c, bool latin = false)
{
    auto q = c.quality;
    if (latin && q == "m") q = "m";
    auto s = noteName (c.root, latin) + String::fromUTF8 (q.replace ("b5", "♭5").toRawUTF8());
    if (c.bass >= 0 && c.bass != c.root) s << "/" << noteName (c.bass, latin);
    return s;
}

/** Hauteurs pour l'écoute et « Accords → MIDI » : basse à l'octave 2–3, accord serré autour du Do 4. */
inline std::vector<int> voicing (const Chord& c, bool withBass = true)
{
    std::vector<int> notes;
    int base = 60 + c.root;
    if (base > 66) base -= 12;                    // accord autour du Do central
    for (int iv : intervalsOf (c.quality)) notes.push_back (base + iv);
    if (withBass)
    {
        int b = 36 + (c.bass >= 0 ? c.bass : c.root);
        if (b > 43) b -= 12;
        notes.insert (notes.begin(), b);
    }
    return notes;
}

/** Classes de hauteur (0–11) de l'accord, basse comprise. */
inline std::vector<int> pitchClasses (const Chord& c)
{
    std::vector<int> pcs;
    for (int iv : intervalsOf (c.quality)) pcs.push_back ((c.root + iv) % 12);
    if (c.bass >= 0) pcs.push_back (c.bass % 12);
    return pcs;
}

//==============================================================================
static const Identifier listType ("CHENCHORDS"), chordType ("CHORD");

inline ValueTree listTree (te::Edit& e)
{
    auto t = e.state.getChildWithName (listType);
    if (! t.isValid())
    {
        t = ValueTree (listType);
        e.state.appendChild (t, nullptr);
    }
    return t;
}

inline std::vector<Chord> get (te::Edit& e)
{
    std::vector<Chord> r;
    auto t = e.state.getChildWithName (listType);
    for (auto c : t)
        if (c.hasType (chordType))
            r.push_back ({ (double) c.getProperty ("beat"), (int) c.getProperty ("root"),
                           c.getProperty ("quality").toString(), (int) c.getProperty ("bass", -1) });
    std::sort (r.begin(), r.end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
    return r;
}

inline void set (te::Edit& e, const std::vector<Chord>& chords)
{
    auto t = listTree (e);
    auto* um = &e.getUndoManager();
    for (int i = t.getNumChildren(); --i >= 0;)
        t.removeChild (i, um);
    for (auto& c : chords)
    {
        ValueTree v (chordType);
        v.setProperty ("beat", c.beat, nullptr);
        v.setProperty ("root", c.root, nullptr);
        v.setProperty ("quality", c.quality, nullptr);
        if (c.bass >= 0) v.setProperty ("bass", c.bass, nullptr);
        t.appendChild (v, um);
    }
}

/** Accord en vigueur à un temps donné (le dernier posé avant). */
inline std::optional<Chord> at (const std::vector<Chord>& chords, double beat)
{
    std::optional<Chord> r;
    for (auto& c : chords)
        if (c.beat <= beat + 1.0e-6) r = c;
    return r;
}

/** Fin d'un accord : début du suivant, ou une mesure plus loin pour le dernier. */
inline double endOf (te::Edit& e, const std::vector<Chord>& chords, size_t i)
{
    if (i + 1 < chords.size()) return chords[i + 1].beat;
    auto& ts = e.tempoSequence.getTimeSigAt (te::BeatPosition::fromBeats (chords[i].beat));
    return chords[i].beat + ts.numerator.get() * 4.0 / jmax (1, ts.denominator.get());
}

//==============================================================================
/** Accords → MIDI : un clip MIDI avec les accords (basse + accord serré) sur la piste. */
inline te::MidiClip* toMidi (te::Edit& e, te::AudioTrack& t)
{
    auto chords = get (e);
    if (chords.empty()) return nullptr;
    auto& ts = e.tempoSequence;
    const double startBeat = chords.front().beat;
    const double endBeat = endOf (e, chords, chords.size() - 1);
    te::TimeRange range (ts.toTime (te::BeatPosition::fromBeats (startBeat)), ts.toTime (te::BeatPosition::fromBeats (endBeat)));
    auto mc = t.insertMIDIClip ("Accords", range, nullptr);
    if (mc == nullptr) return nullptr;
    auto& seq = mc->getSequence();
    auto* um = &e.getUndoManager();
    for (size_t i = 0; i < chords.size(); ++i)
    {
        const double b = chords[i].beat - startBeat;
        const double len = endOf (e, chords, i) - chords[i].beat;
        for (int n : voicing (chords[i]))
            seq.addNote (n, te::BeatPosition::fromBeats (b), te::BeatDuration::fromBeats (jmax (0.25, len - 0.02)), 90, 0, um);
    }
    return mc.get();
}

/** Les notes d'un clip MIDI suivent les accords : chaque note va sur la note de l'accord
    la plus proche (même octave), comme « Les événements suivent la piste d'accords ». */
inline int conformClip (te::Edit& e, te::MidiClip& mc)
{
    auto chords = get (e);
    if (chords.empty()) return 0;
    auto& ts = e.tempoSequence;
    auto* um = &e.getUndoManager();
    int changed = 0;
    for (auto* n : mc.getSequence().getNotes())
    {
        auto t = mc.getTimeOfContentBeat (n->getStartBeat());
        auto beat = ts.toBeats (t).inBeats();
        auto ch = at (chords, beat + 0.01);
        if (! ch) continue;
        auto pcs = pitchClasses (*ch);
        int best = n->getNoteNumber(), bestDist = 99;
        for (int d = 0; d <= 6 && bestDist == 99; ++d)
            for (int sgn : { -1, 1 })
            {
                int cand = n->getNoteNumber() + d * sgn;
                if (std::find (pcs.begin(), pcs.end(), ((cand % 12) + 12) % 12) != pcs.end()) { best = cand; bestDist = d; break; }
            }
        if (best != n->getNoteNumber()) { n->setNoteNumber (jlimit (0, 127, best), um); ++changed; }
    }
    return changed;
}

/** Détecte les accords d'un clip MIDI (un par mesure, ou par demi-mesure si ça change)
    et remplit la piste d'accords. */
inline int detectFromClip (te::Edit& e, te::MidiClip& mc)
{
    auto& ts = e.tempoSequence;
    auto pos = mc.getPosition();
    const double b0 = ts.toBeats (pos.getStart()).inBeats();
    const double b1 = ts.toBeats (pos.getEnd()).inBeats();
    auto& sig = ts.getTimeSigAt (pos.getStart());
    const double bar = sig.numerator.get() * 4.0 / jmax (1, sig.denominator.get());
    const double step = bar / 2.0;

    std::vector<Chord> found;
    for (double s = std::floor (b0 / step) * step; s < b1 - 0.01; s += step)
    {
        float weight[12] = {};
        int lowest = 128;
        for (auto* n : mc.getSequence().getNotes())
        {
            auto ns = ts.toBeats (mc.getTimeOfContentBeat (n->getStartBeat())).inBeats();
            auto ne = ts.toBeats (mc.getTimeOfContentBeat (n->getEndBeat())).inBeats();
            auto ov = jmin (ne, s + step) - jmax (ns, s);
            if (ov <= 0.0) continue;
            weight[n->getNoteNumber() % 12] += (float) ov;
            lowest = jmin (lowest, n->getNoteNumber());
        }
        float best = 0.0f; Chord bc; bool ok = false;
        for (int root = 0; root < 12; ++root)
            for (auto& q : qualities())
            {
                float score = 0.0f; int count = 0;
                for (int iv : q.intervals) { score += weight[(root + iv) % 12]; ++count; }
                float outside = 0.0f;
                for (int pc = 0; pc < 12; ++pc) outside += weight[pc];
                outside -= score;
                score = score - 0.6f * outside - 0.15f * (float) count + (weight[root] > 0.0f ? 0.3f : 0.0f);
                if (score > best + 1.0e-4f) { best = score; bc = { s, root, q.id, -1 }; ok = true; }
            }
        if (! ok) continue;
        if (lowest < 128 && lowest % 12 != bc.root) bc.bass = lowest % 12;
        if (! found.empty() && found.back().root == bc.root && found.back().quality == bc.quality && found.back().bass == bc.bass) continue;
        found.push_back (bc);
    }
    if (found.empty()) return 0;

    auto all = get (e);
    all.erase (std::remove_if (all.begin(), all.end(), [&] (auto& c) { return c.beat >= b0 - 0.01 && c.beat < b1 - 0.01; }), all.end());
    all.insert (all.end(), found.begin(), found.end());
    std::sort (all.begin(), all.end(), [] (auto& a, auto& b) { return a.beat < b.beat; });
    set (e, all);
    return (int) found.size();
}

//==============================================================================
/** Menu de choix d'un accord (fondamentale › couleur), avec renversements. */
inline void chooseChord (std::optional<Chord> current, std::function<void (Chord)> chosen,
                         std::function<void()> removed = {}, std::function<void()> audition = {})
{
    PopupMenu m;
    const auto& qs = qualities();
    for (int root = 0; root < 12; ++root)
    {
        PopupMenu sub;
        for (int qi = 0; qi < (int) qs.size(); ++qi)
        {
            Chord c { 0, root, qs[(size_t) qi].id, -1 };
            sub.addItem (1 + root * 100 + qi, name (c) + String::fromUTF8 ("   ·  ") + String::fromUTF8 (qs[(size_t) qi].label),
                         true, current && current->root == root && current->quality == qs[(size_t) qi].id && current->bass < 0);
        }
        m.addSubMenu (noteName (root, false) + String::fromUTF8 ("   (") + noteName (root, true) + ")", sub,
                      true, Image(), current && current->root == root);
    }
    if (current)
    {
        m.addSeparator();
        PopupMenu inv;
        auto pcs = intervalsOf (current->quality);
        for (int iv : pcs)
        {
            if (iv == 0 || iv > 11) continue;
            Chord c = *current; c.bass = (current->root + iv) % 12;
            inv.addItem (2000 + c.bass, name (c), true, current->bass == c.bass);
        }
        Chord c0 = *current; c0.bass = -1;
        inv.addItem (2100, name (c0) + String::fromUTF8 ("  (état fondamental)"), true, current->bass < 0);
        m.addSubMenu (String::fromUTF8 ("Renversement (basse)"), inv);
        if (audition) m.addItem (3000, String::fromUTF8 ("Écouter"));
        if (removed) m.addItem (3001, String::fromUTF8 ("Supprimer l'accord"));
    }

    m.showMenuAsync (PopupMenu::Options(), [current, chosen, removed, audition] (int r)
    {
        if (r <= 0) return;
        if (r == 3000) { if (audition) audition(); return; }
        if (r == 3001) { if (removed) removed(); return; }
        if (r >= 2000 && r <= 2100 && current)
        {
            Chord c = *current; c.bass = r == 2100 ? -1 : r - 2000;
            chosen (c);
            return;
        }
        const int root = (r - 1) / 100, qi = (r - 1) % 100;
        if (qi < (int) qualities().size()) chosen ({ 0, root, qualities()[(size_t) qi].id, -1 });
    });
}

} // namespace chen::Chords
