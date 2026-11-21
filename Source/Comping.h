#pragma once

// =============================================================================
//  Comping par couloirs, façon Pro Tools :
//  chaque playlist rangée s'affiche en couloir sous la piste ; une sélection
//  dans un couloir remplace la même zone de la playlist principale.
// =============================================================================

namespace chen::Comping
{
static const Identifier showLanesID ("chenShowLanes");
static const Identifier perTakeID   ("chenNewPlaylistPerTake");

struct LaneClip
{
    File file;
    double start = 0, length = 0, offset = 0;
    String name;
};

struct Lane
{
    String name;
    ValueTree tree;
    std::vector<LaneClip> clips;
};

inline bool lanesShown (te::Track& t)            { return t.state.getProperty (showLanesID, false); }
inline void setLanesShown (te::Track& t, bool b) { t.state.setProperty (showLanesID, b, nullptr); }

inline bool newPlaylistPerTake (te::Edit& e)            { return e.state.getProperty (perTakeID, true); }
inline void setNewPlaylistPerTake (te::Edit& e, bool b) { e.state.setProperty (perTakeID, b, nullptr); }

inline std::vector<Lane> getLanes (te::ClipTrack& t)
{
    std::vector<Lane> lanes;
    auto root = t.state.getChildWithName (Playlists::rootID);
    for (auto pl : root)
    {
        if (! pl.hasType (Playlists::listID)) continue;
        Lane lane { pl[Playlists::nameID].toString(), pl, {} };
        for (auto c : pl)
        {
            LaneClip lc;
            lc.file   = te::SourceFileReference::findFileFromString (t.edit, c[te::IDs::source].toString());
            lc.start  = c[te::IDs::start];
            lc.length = c[te::IDs::length];
            lc.offset = c[te::IDs::offset];
            lc.name   = c[te::IDs::name].toString();
            lane.clips.push_back (lc);
        }
        lanes.push_back (std::move (lane));
    }
    std::sort (lanes.begin(), lanes.end(), [] (const Lane& a, const Lane& b) { return a.name.compareNatural (b.name) > 0; });
    return lanes;   // la prise la plus récente en premier
}

/** Remplace la zone [t1, t2] de la playlist active par celle du couloir. */
inline void promote (te::ClipTrack& t, const Lane& lane, double t1, double t2)
{
    if (t2 - t1 < 0.005) return;
    auto& edit = t.edit;
    auto tp1 = te::TimePosition::fromSeconds (t1), tp2 = te::TimePosition::fromSeconds (t2);

    // 1. vider la zone dans la playlist active
    juce::Array<te::Clip*> clips (t.getClips());
    for (auto* c : clips)
    {
        auto r = c->getPosition().time;
        if (r.getEnd() <= tp1 || r.getStart() >= tp2) continue;
        if (r.getStart() < tp1) c = t.splitClip (*c, tp1);
        if (c == nullptr) continue;
        if (c->getPosition().getEnd() > tp2)
            t.splitClip (*c, tp2);
        c->removeFromParent();
    }

    // 2. copier les morceaux du couloir
    constexpr double fade = 0.008;   // raccords : 8 ms
    for (auto c : lane.tree)
    {
        double s = c[te::IDs::start], l = c[te::IDs::length];
        if (s + l <= t1 || s >= t2) continue;

        auto copy = c.createCopy();
        edit.createNewItemID().writeID (copy, nullptr);
        if (auto* nc = t.insertClipWithState (copy))
        {
            if (s < t1)     nc->setStart (tp1, true, false);
            if (s + l > t2) nc->setEnd (tp2, true);
            if (auto* a = dynamic_cast<te::AudioClipBase*> (nc))
            {
                a->setFadeIn (te::TimeDuration::fromSeconds (fade));
                a->setFadeOut (te::TimeDuration::fromSeconds (fade));
            }
        }
    }
}

/** Avant un enregistrement : chaque piste armée qui a déjà des clips part sur une nouvelle playlist. */
inline void prepareTakes (te::Edit& edit)
{
    if (! newPlaylistPerTake (edit)) return;
    for (auto* t : te::getAudioTracks (edit))
        if (EngineHelpers::isTrackArmed (*t) && ! t->getClips().isEmpty())
        {
            Playlists::createNew (*t);
            setLanesShown (*t, true);
        }
}

/** Démarre / arrête l'enregistrement en préparant les prises. */
inline void toggleRecord (te::Edit& edit)
{
    if (! edit.getTransport().isRecording())
        prepareTakes (edit);
    EngineHelpers::toggleRecord (edit);
}

inline int countLanes (te::Track& t)
{
    if (! lanesShown (t)) return 0;
    int n = 0;
    for (auto c : t.state.getChildWithName (Playlists::rootID))
        if (c.hasType (Playlists::listID)) ++n;
    return n;
}

} // namespace chen::Comping
