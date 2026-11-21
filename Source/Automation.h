#pragma once

// =============================================================================
//  Automation : quel paramètre la piste montre dans sa courbe (volume, pan,
//  envois, ou n'importe quel paramètre d'un plugin en insert), et le menu
//  pour le choisir — façon Pro Tools / Cubase.
// =============================================================================

namespace chen::Automation
{

static const Identifier shownProp ("chenAutoParam");

/** Demande à la fenêtre Édition d'afficher l'automation (bouton AUTO des plugins). */
inline std::function<void (te::Track&)>& onShowLane()
{
    static std::function<void (te::Track&)> f;
    return f;
}

inline String keyFor (te::AudioTrack& t, te::AutomatableParameter& p)
{
    if (auto* v = t.getVolumePlugin())
    {
        if (&p == v->volParam.get()) return "vol";
        if (&p == v->panParam.get()) return "pan";
    }
    if (auto* pl = p.getPlugin())
        return "p:" + pl->itemID.toString() + ":" + p.paramID;
    return "vol";
}

inline te::AutomatableParameter* resolve (te::AudioTrack& t, const String& key)
{
    auto* v = t.getVolumePlugin();
    if (key == "pan") return v ? v->panParam.get() : nullptr;
    if (key.startsWith ("p:"))
    {
        auto rest = key.substring (2);
        auto id = rest.upToFirstOccurrenceOf (":", false, false);
        auto paramID = rest.fromFirstOccurrenceOf (":", false, false);
        for (auto* p : t.pluginList)
            if (p->itemID.toString() == id)
                for (auto* ap : p->getAutomatableParameters())
                    if (ap->paramID == paramID) return ap;
    }
    return v ? v->volParam.get() : nullptr;
}

inline te::AutomatableParameter* shown (te::AudioTrack& t)
{
    return resolve (t, t.state.getProperty (shownProp).toString());
}

inline void show (te::AudioTrack& t, te::AutomatableParameter& p)
{
    t.state.setProperty (shownProp, keyFor (t, p), nullptr);
    if (onShowLane()) onShowLane() (t);
}

inline bool isVolume (te::AudioTrack& t, te::AutomatableParameter& p)
{
    auto* v = t.getVolumePlugin();
    return v != nullptr && &p == v->volParam.get();
}

/** Nom court affiché sur la courbe : « VOLUME », « PAN », « Comp · Seuil »… */
inline String displayName (te::AudioTrack& t, te::AutomatableParameter& p)
{
    if (auto* v = t.getVolumePlugin())
    {
        if (&p == v->volParam.get()) return "VOLUME";
        if (&p == v->panParam.get()) return "PAN";
    }
    if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p.getPlugin()))
        return String::fromUTF8 ("ENVOI ") + s->getBusName();
    String plug = p.getPlugin() ? p.getPlugin()->getName() : String();
    return plug + String::fromUTF8 (" · ") + p.getParameterName();
}

inline String valueText (te::AudioTrack& t, te::AutomatableParameter& p)
{
    if (isVolume (t, p)) return dbString (te::volumeFaderPositionToDB (p.getCurrentValue())) + " dB";
    return p.getCurrentValueAsStringWithLabel();
}

/** Combien de paramètres de la piste ont une automation (points). */
inline int countAutomated (te::AudioTrack& t)
{
    int n = 0;
    for (auto* p : t.getAllAutomatableParams())
        if (p->hasAutomationPoints()) ++n;
    return n;
}

/** Menu de choix du paramètre (étiquette de la courbe, bouton AUTO). */
inline void showMenu (te::AudioTrack& t, std::function<void()> changed, te::AutomatableParameter* lastTouched = nullptr)
{
    PopupMenu m;
    std::map<int, te::AutomatableParameter::Ptr> ids;
    int id = 1;
    auto* current = shown (t);

    auto addParam = [&] (PopupMenu& menu, te::AutomatableParameter& p, const String& name)
    {
        ids[id] = &p;
        menu.addItem (id++, (p.hasAutomationPoints() ? String::fromUTF8 ("● ") : String ("    ")) + name, true, &p == current);
    };

    if (lastTouched != nullptr)
    {
        m.addSectionHeader (String::fromUTF8 ("Dernier paramètre touché"));
        addParam (m, *lastTouched, displayName (t, *lastTouched));
        m.addSeparator();
    }

    if (auto* v = t.getVolumePlugin())
    {
        addParam (m, *v->volParam, "Volume");
        addParam (m, *v->panParam, "Pan");
    }

    // envois
    PopupMenu sends;
    for (auto* p : t.pluginList)
        if (auto* s = dynamic_cast<te::AuxSendPlugin*> (p))
            if (s->gain != nullptr) addParam (sends, *s->gain, s->getBusName());
    if (sends.getNumItems() > 0) m.addSubMenu ("Envois", sends);

    // inserts (lettres A–J comme dans la console)
    auto inserts = getInsertPlugins (t);
    bool any = false;
    for (int slot = 0; slot < inserts.size(); ++slot)
    {
        auto* pl = inserts[slot];
        auto params = pl->getAutomatableParameters();
        if (params.isEmpty()) continue;
        if (! any) { m.addSeparator(); any = true; }
        PopupMenu sub;
        int automated = 0;
        for (int i = 0; i < params.size(); ++i)
        {
            if (params[i]->hasAutomationPoints()) ++automated;
            if (i < 400) addParam (sub, *params[i], params[i]->getParameterName());
        }
        String title = insertLetter (slot).toUpperCase() + String::fromUTF8 (" · ") + pl->getName();
        if (automated > 0) title << String::fromUTF8 ("   ● ") << automated;
        m.addSubMenu (title, sub, true, Image(), current != nullptr && current->getPlugin() == pl);
    }
    if (! any)
        m.addItem (-1, String::fromUTF8 ("(aucun plugin en insert à automatiser)"), false);

    m.addSeparator();
    const int clearId = 100000, clearAllId = 100001;
    m.addItem (clearId, String::fromUTF8 ("Effacer l'automation de ce paramètre"), current != nullptr && current->hasAutomationPoints());
    m.addItem (clearAllId, String::fromUTF8 ("Effacer toute l'automation de la piste"), countAutomated (t) > 0);

    te::AudioTrack::Ptr tp (&t);
    m.showMenuAsync (PopupMenu::Options(), [tp, ids, changed] (int r)
    {
        if (r <= 0 || tp == nullptr) return;
        auto* um = &tp->edit.getUndoManager();
        if (r == clearId)
        {
            if (auto* p = shown (*tp)) p->getCurve().clear (um);
        }
        else if (r == clearAllId)
        {
            for (auto* p : tp->getAllAutomatableParams()) p->getCurve().clear (um);
        }
        else if (auto it = ids.find (r); it != ids.end())
        {
            show (*tp, *it->second);
        }
        if (changed) changed();
    });
}

//==============================================================================
/** Retient le dernier paramètre touché dans la fenêtre d'un plugin (pour « AUTO »). */
class TouchTracker : private te::AutomatableParameter::Listener
{
public:
    explicit TouchTracker (te::Plugin& p) : plugin (p)
    {
        for (auto* ap : plugin.getAutomatableParameters()) { ap->addListener (this); params.add (ap); }
    }
    ~TouchTracker() override
    {
        for (auto& ap : params) ap->removeListener (this);
    }

    te::AutomatableParameter* getLastTouched() const { return last.get(); }

private:
    te::Plugin& plugin;
    ReferenceCountedArray<te::AutomatableParameter> params;
    te::AutomatableParameter::Ptr last;

    void curveHasChanged (te::AutomatableParameter&) override {}
    void parameterChangeGestureBegin (te::AutomatableParameter& p) override { last = &p; }
    void parameterChanged (te::AutomatableParameter& p, float) override { last = &p; }
};

} // namespace chen::Automation
