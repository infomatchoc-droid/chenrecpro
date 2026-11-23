#pragma once

// =============================================================================
//  Pistes dossier (façon Cubase) : des pistes regroupées sous un en-tête de
//  dossier qu'on replie, qu'on mute ou qu'on solote d'un coup.
//  Les membres gardent leur sortie : le dossier sert à ranger, pas à mixer
//  (pour un sous-groupe audio : « Nouvelle piste Bus » dans la sortie).
//  Stockage : propriété « chenFolder » sur chaque piste membre,
//  et CHENFOLDERS > FOLDER (id, name, colour, collapsed) dans le projet.
// =============================================================================

namespace chen::Folders
{

static const Identifier memberProp ("chenFolder");
static const Identifier listType ("CHENFOLDERS"), folderType ("FOLDER");

inline String folderOf (const te::Track& t) { return t.state.getProperty (memberProp).toString(); }

inline ValueTree listTree (te::Edit& e)
{
    auto t = e.state.getChildWithName (listType);
    if (! t.isValid()) { t = ValueTree (listType); e.state.appendChild (t, nullptr); }
    return t;
}

inline ValueTree folderTree (te::Edit& e, const String& id)
{
    return e.state.getChildWithName (listType).getChildWithProperty ("id", id);
}

inline Array<te::AudioTrack*> members (te::Edit& e, const String& id)
{
    Array<te::AudioTrack*> r;
    if (id.isEmpty()) return r;
    for (auto* t : te::getAudioTracks (e))
        if (folderOf (*t) == id) r.add (t);
    return r;
}

inline bool isFirstMember (te::Edit& e, te::AudioTrack& t)
{
    auto id = folderOf (t);
    if (id.isEmpty()) return false;
    auto m = members (e, id);
    return ! m.isEmpty() && m.getFirst() == &t;
}

inline bool isCollapsed (te::Edit& e, const String& id) { return (bool) folderTree (e, id).getProperty ("collapsed", false); }
inline String name (te::Edit& e, const String& id)       { return folderTree (e, id).getProperty ("name", "Dossier").toString(); }

inline Colour colour (te::Edit& e, const String& id)
{
    static const uint32 c[] = { 0xffd8a24a, 0xff6aa8ea, 0xff62c27f, 0xffe0726c, 0xffa687e6, 0xff4fc2b5 };
    int idx = (int) folderTree (e, id).getProperty ("colour", 0);
    return Colour (c[(size_t) idx % std::size (c)]);
}

inline void setCollapsed (te::Edit& e, const String& id, bool c) { folderTree (e, id).setProperty ("collapsed", c, nullptr); }
inline void rename (te::Edit& e, const String& id, const String& n) { folderTree (e, id).setProperty ("name", n, &e.getUndoManager()); }

/** Piste cachée parce que son dossier est replié. */
inline bool isHidden (te::Edit& e, const te::AudioTrack& t)
{
    auto id = folderOf (t);
    return id.isNotEmpty() && isCollapsed (e, id);
}

/** Retire les dossiers vides. */
inline void tidy (te::Edit& e)
{
    auto list = e.state.getChildWithName (listType);
    for (int i = list.getNumChildren(); --i >= 0;)
        if (members (e, list.getChild (i).getProperty ("id").toString()).isEmpty())
            list.removeChild (i, nullptr);
}

/** Range les pistes d'un dossier les unes à la suite des autres (sous la première). */
inline void makeContiguous (te::Edit& e, const String& id)
{
    auto m = members (e, id);
    for (int i = 1; i < m.size(); ++i)
    {
        auto all = te::getAudioTracks (e);
        if (all.indexOf (m[i]) != all.indexOf (m[i - 1]) + 1)
            e.moveTrack (m[i], te::TrackInsertPoint (m[i]->getParentTrack(), m[i - 1]));
    }
}

/** Crée un dossier avec ces pistes (dans l'ordre du projet). Renvoie son identifiant. */
inline String create (te::Edit& e, Array<te::AudioTrack*> tracks, const String& folderName = {})
{
    if (tracks.isEmpty()) return {};
    auto all = te::getAudioTracks (e);
    std::sort (tracks.begin(), tracks.end(), [&all] (auto* a, auto* b) { return all.indexOf (a) < all.indexOf (b); });

    auto list = listTree (e);
    int n = 1;
    while (list.getChildWithProperty ("id", "F" + String (n)).isValid()) ++n;
    const String id = "F" + String (n);

    ValueTree f (folderType);
    f.setProperty ("id", id, nullptr);
    f.setProperty ("name", folderName.isNotEmpty() ? folderName : String::fromUTF8 ("Dossier ") + String (n), nullptr);
    f.setProperty ("colour", n - 1, nullptr);
    list.appendChild (f, nullptr);

    for (auto* t : tracks) t->state.setProperty (memberProp, id, nullptr);
    makeContiguous (e, id);
    tidy (e);
    return id;
}

inline void removeFromFolder (te::Edit& e, te::AudioTrack& t)
{
    t.state.removeProperty (memberProp, nullptr);
    tidy (e);
}

inline void dissolve (te::Edit& e, const String& id)
{
    for (auto* t : members (e, id)) t->state.removeProperty (memberProp, nullptr);
    tidy (e);
}

inline bool allMuted (te::Edit& e, const String& id)
{
    auto m = members (e, id);
    for (auto* t : m) if (! t->isMuted (false)) return false;
    return ! m.isEmpty();
}

inline bool allSoloed (te::Edit& e, const String& id)
{
    auto m = members (e, id);
    for (auto* t : m) if (! t->isSolo (false)) return false;
    return ! m.isEmpty();
}

inline void setMute (te::Edit& e, const String& id, bool m) { for (auto* t : members (e, id)) t->setMute (m); }
inline void setSolo (te::Edit& e, const String& id, bool s) { for (auto* t : members (e, id)) t->setSolo (s); }

} // namespace chen::Folders
