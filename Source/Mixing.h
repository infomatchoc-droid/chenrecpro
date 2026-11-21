#pragma once

// =============================================================================
//  Mixage : pistes FX (retours d'effets), envois, routage vers des bus.
// =============================================================================

namespace chen::Mixing
{

inline te::AuxReturnPlugin* getAuxReturn (te::Track& t)
{
    for (auto* p : t.pluginList.getPlugins())
        if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (p)) return r;
    return nullptr;
}

inline bool isFxTrack (te::Track& t) { return getAuxReturn (t) != nullptr; }

inline Array<te::AuxSendPlugin*> getSends (te::Track& t)
{
    Array<te::AuxSendPlugin*> res;
    for (auto* p : t.pluginList.getPlugins())
        if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p)) res.add (s);
    return res;
}

/** Retours d'effets existants : numéro de bus -> piste FX. */
inline std::map<int, te::AudioTrack*> getFxReturns (te::Edit& edit)
{
    std::map<int, te::AudioTrack*> res;
    for (auto* t : te::getAudioTracks (edit))
        if (auto* r = getAuxReturn (*t)) res[r->busNumber.get()] = t;
    return res;
}

inline String busName (te::Edit& edit, int bus)
{
    auto fx = getFxReturns (edit);
    if (auto it = fx.find (bus); it != fx.end()) return it->second->getName();
    return "Bus " + String (bus + 1);
}

enum class FxKind { reverb, delay, empty };

/** Crée une piste FX (retour d'effet) sur un bus libre. */
inline te::AudioTrack* createFxTrack (te::Edit& edit, FxKind kind)
{
    auto used = getFxReturns (edit);
    int bus = 0;
    while (used.count (bus) && bus < 31) ++bus;

    auto t = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr);
    if (t == nullptr) return nullptr;

    String name = kind == FxKind::reverb ? String::fromUTF8 ("FX · Réverb")
                : kind == FxKind::delay  ? String::fromUTF8 ("FX · Délai")
                                         : "FX " + String (bus + 1);
    t->setName (name);
    t->setColour (Colour (0xff8fa0b5));

    if (auto ret = edit.getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
    {
        if (auto* r = dynamic_cast<te::AuxReturnPlugin*> (ret.get())) r->busNumber = bus;
        t->pluginList.insertPlugin (ret, 0, nullptr);
    }

    String fxType = kind == FxKind::reverb ? te::ReverbPlugin::xmlTypeName
                  : kind == FxKind::delay  ? te::DelayPlugin::xmlTypeName : "";
    if (fxType.isNotEmpty())
        if (auto fx = edit.getPluginCache().createNewPlugin (fxType, {}))
        {
            t->pluginList.insertPlugin (fx, 1, nullptr);
            if (auto* rv = dynamic_cast<te::ReverbPlugin*> (fx.get()))
            {
                rv->wetParam->setParameter (1.0f, sendNotification);   // retour 100 % mouillé
                rv->dryParam->setParameter (0.0f, sendNotification);
            }
        }

    edit.setAuxBusName (bus, name);
    return t.get();
}

/** Ajoute un envoi post-fader vers un bus. */
inline te::AuxSendPlugin* addSend (te::Track& t, int bus)
{
    auto p = t.edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
    auto* send = dynamic_cast<te::AuxSendPlugin*> (p.get());
    if (send == nullptr) return nullptr;
    send->busNumber = bus;

    // juste après le volume (post-fader), comme Pro Tools
    int index = 0;
    auto plugins = t.pluginList.getPlugins();
    for (int i = 0; i < plugins.size(); ++i)
        if (dynamic_cast<te::VolumeAndPanPlugin*> (plugins[i].get()) != nullptr) index = i + 1;
    if (auto lastSend = getSends (t); ! lastSend.isEmpty())
        index = jmax (index, t.pluginList.indexOf (lastSend.getLast()) + 1);

    t.pluginList.insertPlugin (p, index, nullptr);
    send->setGainDb (-10.0f);
    return send;
}

//==============================================================================
inline String outputName (te::AudioTrack& t)
{
    if (auto* d = t.getOutput().getDestinationTrack()) return d->getName();
    return String::fromUTF8 ("Stéréo Out");
}

/** Vrai si router `from` vers `to` créerait une boucle. */
inline bool wouldLoop (te::AudioTrack& from, te::AudioTrack& to)
{
    for (auto* d = &to; d != nullptr; d = d->getOutput().getDestinationTrack())
        if (d == &from) return true;
    return false;
}

inline void showOutputMenu (te::AudioTrack& t, Component* target)
{
    PopupMenu m;
    m.addSectionHeader ("SORTIE");
    m.addItem (1, String::fromUTF8 ("Stéréo Out"), true, t.getOutput().getDestinationTrack() == nullptr);
    m.addSeparator();

    Array<te::AudioTrack*> dests;
    for (auto* other : te::getAudioTracks (t.edit))
    {
        if (other == &t || isFxTrack (*other) || wouldLoop (t, *other)) continue;
        dests.add (other);
        m.addItem (100 + dests.size() - 1, other->getName(), true, t.getOutput().getDestinationTrack() == other);
    }
    m.addSeparator();
    m.addItem (2, "Nouvelle piste Bus");

    te::Track::Ptr keep (&t);
    m.showMenuAsync (target ? PopupMenu::Options().withTargetComponent (target) : PopupMenu::Options(),
                     [keep, dests] (int r)
    {
        auto* at = dynamic_cast<te::AudioTrack*> (keep.get());
        if (at == nullptr || r == 0) return;
        if (r == 1) at->getOutput().setOutputToDefaultDevice (false);
        if (r >= 100) at->getOutput().setOutputToTrack (dests[r - 100]);
        if (r == 2)
        {
            int n = 1;
            for (auto* o : te::getAudioTracks (at->edit)) if (o->getName().startsWith ("Bus")) ++n;
            if (auto bus = at->edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (at->edit), nullptr))
            {
                bus->setName ("Bus " + String (n));
                bus->setColour (Colour (0xffd8b765));
                at->getOutput().setOutputToTrack (bus.get());
            }
        }
    });
}

} // namespace chen::Mixing
