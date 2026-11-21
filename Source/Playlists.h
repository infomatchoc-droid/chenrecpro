#pragma once

// =============================================================================
//  Playlists par piste, façon Pro Tools.
//  Les clips de la playlist active vivent normalement sur la piste ;
//  les autres playlists sont rangées dans un nœud CHENPLAYLISTS de la piste
//  (et donc enregistrées avec le projet).
// =============================================================================

namespace chen::Playlists
{
static const Identifier rootID    ("CHENPLAYLISTS");
static const Identifier listID    ("PLAYLIST");
static const Identifier currentID ("current");
static const Identifier nameID    ("name");

inline ValueTree getRoot (te::ClipTrack& t)
{
    auto r = t.state.getChildWithName (rootID);
    if (! r.isValid())
    {
        r = ValueTree (rootID);
        r.setProperty (currentID, t.getName() + ".01", nullptr);
        t.state.appendChild (r, nullptr);
    }
    return r;
}

inline String getCurrent (te::ClipTrack& t)
{
    auto r = t.state.getChildWithName (rootID);
    return r.isValid() ? r[currentID].toString() : t.getName() + ".01";
}

inline StringArray getNames (te::ClipTrack& t)
{
    StringArray names { getCurrent (t) };
    auto r = t.state.getChildWithName (rootID);
    for (auto c : r)
        if (c.hasType (listID)) names.addIfNotAlreadyThere (c[nameID].toString());
    names.sortNatural();
    return names;
}

/** Copie l'état des clips actuels dans une playlist rangée (remplace si elle existe). */
inline void stash (te::ClipTrack& t, const String& name)
{
    auto r = getRoot (t);
    if (auto old = r.getChildWithProperty (nameID, name); old.isValid())
        r.removeChild (old, nullptr);

    ValueTree pl (listID);
    pl.setProperty (nameID, name, nullptr);
    for (auto* c : t.getClips())
        pl.appendChild (c->state.createCopy(), nullptr);
    r.appendChild (pl, nullptr);
}

inline void removeAllClips (te::ClipTrack& t)
{
    juce::Array<te::Clip*> clips (t.getClips());
    for (auto* c : clips)
        c->removeFromParent();
}

inline void restore (te::ClipTrack& t, const String& name)
{
    auto r = getRoot (t);
    auto pl = r.getChildWithProperty (nameID, name);
    if (! pl.isValid()) return;

    for (auto c : pl)
    {
        auto copy = c.createCopy();
        t.edit.createNewItemID().writeID (copy, nullptr);
        t.insertClipWithState (copy);
    }
    r.removeChild (pl, nullptr);
}

inline String nextName (te::ClipTrack& t)
{
    auto names = getNames (t);
    for (int i = 1; i < 1000; ++i)
    {
        auto n = t.getName() + "." + String (i).paddedLeft ('0', 2);
        if (! names.contains (n)) return n;
    }
    return t.getName() + ".new";
}

/** Nouvelle playlist vide (l'actuelle est rangée). */
inline void createNew (te::ClipTrack& t)
{
    auto cur = getCurrent (t);
    stash (t, cur);
    removeAllClips (t);
    getRoot (t).setProperty (currentID, nextName (t), nullptr);
}

/** Duplique la playlist active et bascule sur la copie. */
inline void duplicate (te::ClipTrack& t)
{
    stash (t, getCurrent (t));
    getRoot (t).setProperty (currentID, nextName (t), nullptr);
}

inline void switchTo (te::ClipTrack& t, const String& name)
{
    auto cur = getCurrent (t);
    if (name == cur) return;
    stash (t, cur);
    removeAllClips (t);
    restore (t, name);
    getRoot (t).setProperty (currentID, name, nullptr);
}

inline void rename (te::ClipTrack& t, const String& newName)
{
    if (newName.isEmpty() || getNames (t).contains (newName)) return;
    getRoot (t).setProperty (currentID, newName, nullptr);
}

/** Supprime la playlist active et bascule sur une autre (s'il y en a). */
inline void deleteCurrent (te::ClipTrack& t)
{
    auto names = getNames (t);
    if (names.size() < 2) return;
    auto cur = getCurrent (t);
    names.removeString (cur);
    removeAllClips (t);
    restore (t, names[0]);
    getRoot (t).setProperty (currentID, names[0], nullptr);
}

/** Menu playlist (▾ à côté du nom de piste). */
inline void showMenu (te::ClipTrack& t, Component* target)
{
    PopupMenu m;
    m.addSectionHeader ("PLAYLISTS");
    auto names = getNames (t);
    auto cur = getCurrent (t);
    for (int i = 0; i < names.size(); ++i)
        m.addItem (100 + i, names[i], true, names[i] == cur);
    m.addSeparator();
    m.addItem (1, "Nouvelle playlist");
    m.addItem (2, "Dupliquer la playlist");
    m.addItem (3, String::fromUTF8 ("Renommer la playlist…"));
    m.addItem (4, "Supprimer la playlist", names.size() > 1);

    te::Track::Ptr keep (&t);
    m.showMenuAsync (target ? PopupMenu::Options().withTargetComponent (target) : PopupMenu::Options(),
                     [keep, names] (int r)
    {
        auto* ct = dynamic_cast<te::ClipTrack*> (keep.get());
        if (ct == nullptr || r == 0) return;
        if (r == 1) createNew (*ct);
        if (r == 2) duplicate (*ct);
        if (r == 4) deleteCurrent (*ct);
        if (r >= 100) switchTo (*ct, names[r - 100]);
        if (r == 3)
        {
            auto* aw = new AlertWindow ("Renommer la playlist", {}, MessageBoxIconType::NoIcon);
            aw->addTextEditor ("name", getCurrent (*ct));
            aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
            aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
            aw->enterModalState (true, ModalCallbackFunction::create ([keep, aw] (int res)
            {
                if (auto* ct2 = dynamic_cast<te::ClipTrack*> (keep.get()); ct2 != nullptr && res == 1)
                    rename (*ct2, aw->getTextEditorContents ("name"));
            }), true);
        }
    });
}

} // namespace chen::Playlists
