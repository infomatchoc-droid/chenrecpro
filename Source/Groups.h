#pragma once

// =============================================================================
//  Groupes de pistes, façon Pro Tools : volume (relatif), mute et solo liés.
// =============================================================================

namespace chen::Groups
{
static const Identifier groupID ("chenGroup");

inline int groupOf (te::Track& t) { return (int) t.state.getProperty (groupID, 0); }

inline Array<te::AudioTrack*> members (te::Edit& e, int g)
{
    Array<te::AudioTrack*> res;
    if (g <= 0) return res;
    for (auto* t : te::getAudioTracks (e))
        if (groupOf (*t) == g) res.add (t);
    return res;
}

inline String letter (int g)  { return g > 0 ? String::charToString ((juce_wchar) ('a' + (g - 1) % 26)) : String(); }

inline Colour colour (int g)
{
    static const uint32 c[] = { 0xffe0726c, 0xff6aa8ea, 0xff62c27f, 0xffd8b765, 0xffa687e6, 0xff4fc2b5 };
    return Colour (c[(size_t) (g - 1) % std::size (c)]);
}

/** Crée un nouveau groupe avec les pistes données. */
inline int create (te::Edit& e, const Array<te::AudioTrack*>& tracks)
{
    int next = 1;
    for (auto* t : te::getAudioTracks (e)) next = jmax (next, groupOf (*t) + 1);
    for (auto* t : tracks) t->state.setProperty (groupID, next, nullptr);
    return next;
}

inline void removeFromGroup (te::Track& t) { t.state.removeProperty (groupID, nullptr); }

/** Volume : le déplacement est appliqué à tout le groupe, en relatif (Alt : piste seule). */
inline void setVolume (te::AudioTrack& t, float newDb, bool alone = false)
{
    auto* v = t.getVolumePlugin();
    if (v == nullptr) return;
    auto delta = newDb - v->getVolumeDb();
    v->setVolumeDb (newDb);
    if (alone) return;
    for (auto* o : members (t.edit, groupOf (t)))
        if (o != &t)
            if (auto* ov = o->getVolumePlugin())
                ov->setVolumeDb (jlimit (-100.0f, 6.0f, ov->getVolumeDb() + delta));
}

inline void setMute (te::Track& t, bool m)
{
    t.setMute (m);
    if (auto* at = dynamic_cast<te::AudioTrack*> (&t))
        for (auto* o : members (t.edit, groupOf (*at))) o->setMute (m);
}

inline void setSolo (te::Track& t, bool s)
{
    t.setSolo (s);
    if (auto* at = dynamic_cast<te::AudioTrack*> (&t))
        for (auto* o : members (t.edit, groupOf (*at))) o->setSolo (s);
}

} // namespace chen::Groups
