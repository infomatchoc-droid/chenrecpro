#pragma once

// =============================================================================
//  Raccourcis clavier personnalisables + macros (façon Cubase « Key Commands »).
//  Chaque action de ChènRecPro est une commande nommée ; ses touches viennent
//  des réglages par défaut, d'un préréglage (Cubase, Pro Tools) ou de tes
//  propres choix, enregistrés dans les réglages de l'appli (« chenKeyMap »).
// =============================================================================

namespace chen::Keys
{

/** Comparaison de touches indépendante de la casse et des boutons de souris. */
inline bool sameKey (const KeyPress& a, const KeyPress& b)
{
    auto norm = [] (int c) { return (c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c; };
    const int mask = ModifierKeys::shiftModifier | ModifierKeys::ctrlModifier | ModifierKeys::altModifier | ModifierKeys::commandModifier;
    return norm (a.getKeyCode()) == norm (b.getKeyCode())
        && (a.getModifiers().getRawFlags() & mask) == (b.getModifiers().getRawFlags() & mask);
}

inline String describe (const KeyPress& k)
{
    String s;
    auto m = k.getModifiers();
    if (m.isCtrlDown() || m.isCommandDown()) s << "Ctrl+";
    if (m.isAltDown())   s << "Alt+";
    if (m.isShiftDown()) s << "Maj+";
    auto c = k.getKeyCode();
    if (c == KeyPress::spaceKey)                 s << "Espace";
    else if (c == KeyPress::returnKey)           s << String::fromUTF8 ("Entrée");
    else if (c == KeyPress::homeKey)             s << String::fromUTF8 ("Début");
    else if (c == KeyPress::endKey)              s << "Fin";
    else if (c == KeyPress::tabKey)              s << "Tab";
    else if (c == KeyPress::escapeKey)           s << "Echap";
    else if (c == KeyPress::upKey)               s << String::fromUTF8 ("↑");
    else if (c == KeyPress::downKey)             s << String::fromUTF8 ("↓");
    else if (c == KeyPress::leftKey)             s << String::fromUTF8 ("←");
    else if (c == KeyPress::rightKey)            s << String::fromUTF8 ("→");
    else if (c == KeyPress::numberPadMultiply)   s << String::fromUTF8 ("Pavé *");
    else if (c == KeyPress::numberPadDivide)     s << String::fromUTF8 ("Pavé /");
    else if (c == KeyPress::numberPadAdd)        s << String::fromUTF8 ("Pavé +");
    else if (c == KeyPress::numberPadSubtract)   s << String::fromUTF8 ("Pavé -");
    else if (c == KeyPress::numberPadDecimalPoint) s << String::fromUTF8 ("Pavé ,");
    else if (c == KeyPress::numberPadEquals)     s << String::fromUTF8 ("Pavé =");
    else if (c >= KeyPress::numberPad0 && c <= KeyPress::numberPad9) s << String::fromUTF8 ("Pavé ") << String (c - KeyPress::numberPad0);
    else if (c >= KeyPress::F1Key && c <= KeyPress::F12Key) s << "F" << String (c - KeyPress::F1Key + 1);
    else if (c == KeyPress::deleteKey)           s << "Suppr";
    else if (c == KeyPress::insertKey)           s << "Inser";
    else if (c == KeyPress::backspaceKey)        s << String::fromUTF8 ("Retour arrière");
    else if (c == KeyPress::pageUpKey)           s << "Page haut";
    else if (c == KeyPress::pageDownKey)         s << "Page bas";
    else s << String::charToString ((juce_wchar) ((c >= 'a' && c <= 'z') ? c - 'a' + 'A' : c));
    return s;
}

inline String toText (const KeyPress& k) { return String (k.getKeyCode()) + ":" + String (k.getModifiers().getRawFlags()); }
inline KeyPress fromText (const String& s)
{
    return KeyPress (s.upToFirstOccurrenceOf (":", false, false).getIntValue(),
                     ModifierKeys (s.fromFirstOccurrenceOf (":", false, false).getIntValue()), 0);
}

inline KeyPress key (int code, int mods = 0) { return KeyPress (code, ModifierKeys (mods), 0); }
constexpr int ctrl = ModifierKeys::commandModifier, shift = ModifierKeys::shiftModifier, alt = ModifierKeys::altModifier;

//==============================================================================
struct Command
{
    String id, name, category;
    Array<KeyPress> defaults;
    std::function<void()> action;
};

struct Macro
{
    String id, name;
    StringArray steps;   // identifiants de commandes, joués dans l'ordre
};

class Registry
{
public:
    explicit Registry (PropertiesFile& p) : props (p) {}

    void add (const String& id, const char* name, const char* category, Array<KeyPress> defaults, std::function<void()> action)
    {
        commands.push_back ({ id, String::fromUTF8 (name), String::fromUTF8 (category), defaults, std::move (action) });
    }

    const std::vector<Command>& getCommands() const { return commands; }
    std::vector<Macro>& getMacros() { return macros; }

    const Command* find (const String& id) const
    {
        for (auto& c : commands) if (c.id == id) return &c;
        return nullptr;
    }
    Macro* findMacro (const String& id)
    {
        for (auto& m : macros) if (m.id == id) return &m;
        return nullptr;
    }

    String displayName (const String& id)
    {
        if (auto* c = find (id)) return c->name;
        if (auto* m = findMacro (id)) return String::fromUTF8 ("Macro : ") + m->name;
        return id;
    }

    Array<KeyPress> keysFor (const String& id) const
    {
        if (auto it = custom.find (id); it != custom.end()) return it->second;
        if (auto* c = find (id)) return c->defaults;
        return {};
    }

    String shortcutText (const String& id) const
    {
        StringArray s;
        for (auto& k : keysFor (id)) s.add (describe (k));
        return s.joinIntoString (", ");
    }

    /** Libellé de menu : « Nom\tRaccourci ». */
    String menuLabel (const String& id, const char* name) const
    {
        auto sc = shortcutText (id);
        return String::fromUTF8 (name) + (sc.isNotEmpty() ? "\t" + sc : String());
    }

    void setKeys (const String& id, Array<KeyPress> keys) { custom[id] = keys; save(); }
    void resetKeys (const String& id) { custom.erase (id); save(); }

    /** Commande (ou macro) qui utilise déjà cette touche. */
    String ownerOf (const KeyPress& k) const
    {
        for (auto& c : commands)
            for (auto& kk : keysFor (c.id)) if (sameKey (kk, k)) return c.id;
        for (auto& m : macros)
            for (auto& kk : keysFor (m.id)) if (sameKey (kk, k)) return m.id;
        return {};
    }

    void run (const String& id, int depth = 0)
    {
        if (depth > 4) return;
        if (auto* c = find (id)) { if (c->action) c->action(); return; }
        if (auto* m = findMacro (id))
            for (auto& s : m->steps) run (s, depth + 1);
    }

    bool handle (const KeyPress& k)
    {
        auto id = ownerOf (k);
        if (id.isEmpty()) return false;
        run (id);
        return true;
    }

    //==========================================================================
    StringArray presetNames() const
    {
        return { String::fromUTF8 ("ChènRecPro (par défaut)"), "Cubase", "Pro Tools" };
    }

    /** Remplace tous les raccourcis par ceux d'un préréglage (les macros restent). */
    void applyPreset (int index)
    {
        custom.clear();
        if (index == 1)   // Cubase
        {
            custom["transport.record"]  = { key (KeyPress::numberPadMultiply), key ('R') };
            custom["transport.start"]   = { key (KeyPress::numberPadDecimalPoint), key (KeyPress::homeKey) };
            custom["transport.loop"]    = { key (KeyPress::numberPadDivide), key ('L') };
            custom["transport.play"]    = { key (KeyPress::spaceKey), key (KeyPress::numberPadEquals) };
            custom["edit.split"]        = { key ('X', alt), key ('B') };
            custom["view.mixer"]        = { key (KeyPress::F3Key) };
            custom["view.inspector"]    = { key ('I', alt) };
            custom["view.fit"]          = { key ('F', shift) };
            custom["project.newAudio"]  = { key ('T', ctrl) };
        }
        else if (index == 2)   // Pro Tools
        {
            custom["transport.record"]  = { key (KeyPress::F12Key), key (KeyPress::spaceKey, ctrl), key (KeyPress::numberPad3) };
            custom["transport.play"]    = { key (KeyPress::spaceKey), key (KeyPress::numberPad0) };
            custom["transport.start"]   = { key (KeyPress::returnKey), key (KeyPress::homeKey) };
            custom["transport.loop"]    = { key ('L', ctrl | shift) };
            custom["transport.click"]   = { key (KeyPress::numberPad7) };
            custom["edit.split"]        = { key ('E', ctrl), key ('B') };
            custom["view.mixer"]        = { key ('=', ctrl), key (KeyPress::F3Key) };
            custom["project.newAudio"]  = { key ('N', ctrl | shift) };
            custom["project.import"]    = { key ('I', ctrl | shift) };
            custom["view.fit"]          = { key ('H', ctrl | alt) };
        }
        save();
    }

    void resetAll() { custom.clear(); save(); }

    //==========================================================================
    void load()
    {
        custom.clear();
        macros.clear();
        if (auto xml = props.getXmlValue ("chenKeyMap"))
        {
            for (auto* e : xml->getChildWithTagNameIterator ("KEY"))
            {
                Array<KeyPress> ks;
                for (auto& t : StringArray::fromTokens (e->getStringAttribute ("keys"), ";", {}))
                    if (t.isNotEmpty()) ks.add (fromText (t));
                custom[e->getStringAttribute ("id")] = ks;
            }
            for (auto* e : xml->getChildWithTagNameIterator ("MACRO"))
                macros.push_back ({ e->getStringAttribute ("id"), e->getStringAttribute ("name"),
                                    StringArray::fromTokens (e->getStringAttribute ("steps"), ";", {}) });
        }
    }

    void save()
    {
        XmlElement xml ("CHENKEYS");
        for (auto& [id, ks] : custom)
        {
            StringArray t;
            for (auto& k : ks) t.add (toText (k));
            auto* e = xml.createNewChildElement ("KEY");
            e->setAttribute ("id", id);
            e->setAttribute ("keys", t.joinIntoString (";"));
        }
        for (auto& m : macros)
        {
            auto* e = xml.createNewChildElement ("MACRO");
            e->setAttribute ("id", m.id);
            e->setAttribute ("name", m.name);
            e->setAttribute ("steps", m.steps.joinIntoString (";"));
        }
        props.setValue ("chenKeyMap", &xml);
        props.saveIfNeeded();
    }

    String newMacroId() const
    {
        int n = 1;
        for (;;)
        {
            String id = "macro." + String (n);
            bool used = false;
            for (auto& m : macros) used = used || m.id == id;
            if (! used) return id;
            ++n;
        }
    }

private:
    PropertiesFile& props;
    std::vector<Command> commands;
    std::vector<Macro> macros;
    std::map<String, Array<KeyPress>> custom;
};

//==============================================================================
/** Fenêtre « Raccourcis clavier et macros ». */
class EditorComponent : public Component,
                        private ListBoxModel
{
public:
    explicit EditorComponent (Registry& r) : reg (r)
    {
        addAndMakeVisible (presetBox);
        auto names = reg.presetNames();
        for (int i = 0; i < names.size(); ++i) presetBox.addItem (names[i], i + 1);
        presetBox.setTextWhenNothingSelected (String::fromUTF8 ("Préréglage…"));
        presetBox.onChange = [this]
        {
            int idx = presetBox.getSelectedId() - 1;
            if (idx < 0) return;
            reg.applyPreset (idx);
            refresh();
        };

        addAndMakeVisible (search);
        search.setTextToShowWhenEmpty (String::fromUTF8 ("Rechercher une commande…"), Col::text3);
        search.onTextChange = [this] { refresh(); };

        addAndMakeVisible (list);
        list.setModel (this);
        list.setRowHeight (24);
        list.setColour (ListBox::backgroundColourId, Colour (0xff141518));

        auto setup = [this] (TextButton& b, const char* t)
        {
            b.setButtonText (String::fromUTF8 (t));
            addAndMakeVisible (b);
        };
        setup (assignBtn, "Ajouter une touche…");
        setup (clearBtn, "Effacer les touches");
        setup (defaultBtn, String::fromUTF8 ("Par défaut").toRawUTF8());
        setup (newMacroBtn, "Nouvelle macro…");
        setup (editMacroBtn, "Modifier la macro…");
        setup (deleteMacroBtn, "Supprimer la macro");
        setup (resetAllBtn, "Tout remettre par défaut");

        assignBtn.onClick = [this] { startCapture(); };
        clearBtn.onClick = [this] { if (auto id = selectedId(); id.isNotEmpty()) { reg.setKeys (id, {}); refresh(); } };
        defaultBtn.onClick = [this] { if (auto id = selectedId(); id.isNotEmpty()) { reg.resetKeys (id); refresh(); } };
        resetAllBtn.onClick = [this] { reg.resetAll(); presetBox.setSelectedId (0, dontSendNotification); refresh(); };
        newMacroBtn.onClick = [this] { newMacro(); };
        editMacroBtn.onClick = [this] { if (auto* m = reg.findMacro (selectedId())) editMacro (*m); };
        deleteMacroBtn.onClick = [this]
        {
            auto id = selectedId();
            auto& ms = reg.getMacros();
            ms.erase (std::remove_if (ms.begin(), ms.end(), [&] (auto& m) { return m.id == id; }), ms.end());
            reg.resetKeys (id);
            reg.save();
            refresh();
        };

        addChildComponent (capture);
        capture.onKey = [this] (const KeyPress& k) { assignKey (k); };
        capture.onCancel = [this] { capture.setVisible (false); };

        refresh();
        setSize (760, 560);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (Col::panel1);
        g.setColour (Col::text3);
        g.setFont (uiFont (11.5f));
        g.drawText (String::fromUTF8 ("Sélectionne une commande, puis « Ajouter une touche ». Une macro enchaîne plusieurs commandes sur une seule touche."),
                    getLocalBounds().removeFromBottom (26).reduced (12, 0), Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        r.removeFromBottom (20);
        auto top = r.removeFromTop (28);
        presetBox.setBounds (top.removeFromLeft (230));
        top.removeFromLeft (10);
        search.setBounds (top);
        r.removeFromTop (8);
        auto side = r.removeFromRight (190);
        r.removeFromRight (10);
        list.setBounds (r);
        for (auto* b : { &assignBtn, &clearBtn, &defaultBtn })
        {
            b->setBounds (side.removeFromTop (28)); side.removeFromTop (6);
        }
        side.removeFromTop (18);
        for (auto* b : { &newMacroBtn, &editMacroBtn, &deleteMacroBtn })
        {
            b->setBounds (side.removeFromTop (28)); side.removeFromTop (6);
        }
        resetAllBtn.setBounds (side.removeFromBottom (28));
        capture.setBounds (getLocalBounds());
    }

private:
    struct Row { String id, category, name; bool header = false; };

    struct Capture : public Component
    {
        Capture() { setWantsKeyboardFocus (true); }
        std::function<void (const KeyPress&)> onKey;
        std::function<void()> onCancel;
        String commandName;
        void paint (Graphics& g) override
        {
            g.fillAll (Colours::black.withAlpha (0.75f));
            auto b = getLocalBounds().withSizeKeepingCentre (420, 120);
            g.setColour (Col::panel2);
            g.fillRoundedRectangle (b.toFloat(), 6.0f);
            g.setColour (Col::accent);
            g.drawRoundedRectangle (b.toFloat(), 6.0f, 1.5f);
            g.setColour (Col::text);
            g.setFont (uiFont (15.0f, true));
            g.drawText (String::fromUTF8 ("Appuie sur la touche pour : ") , b.removeFromTop (50), Justification::centredBottom);
            g.setColour (Col::accent);
            g.drawText (commandName, b.removeFromTop (30), Justification::centred);
            g.setColour (Col::text3);
            g.setFont (uiFont (11.0f));
            g.drawText (String::fromUTF8 ("(clic pour annuler)"), b, Justification::centred);
        }
        bool keyPressed (const KeyPress& k) override
        {
            auto c = k.getKeyCode();
            if (c == KeyPress::escapeKey && ! k.getModifiers().isAnyModifierKeyDown()) { if (onCancel) onCancel(); return true; }
            if (onKey) onKey (k);
            return true;
        }
        void mouseDown (const MouseEvent&) override { if (onCancel) onCancel(); }
    };

    Registry& reg;
    ComboBox presetBox;
    TextEditor search;
    ListBox list;
    TextButton assignBtn, clearBtn, defaultBtn, newMacroBtn, editMacroBtn, deleteMacroBtn, resetAllBtn;
    Capture capture;
    std::vector<Row> rows;

    String selectedId() const
    {
        int r = list.getSelectedRow();
        return r >= 0 && r < (int) rows.size() && ! rows[(size_t) r].header ? rows[(size_t) r].id : String();
    }

    void refresh()
    {
        auto keep = selectedId();
        rows.clear();
        auto q = search.getText().trim().toLowerCase();
        StringArray cats;
        for (auto& c : reg.getCommands()) cats.addIfNotAlreadyThere (c.category);
        cats.add ("Macros");
        for (auto& cat : cats)
        {
            std::vector<Row> rs;
            if (cat == "Macros")
            {
                for (auto& m : reg.getMacros())
                    if (q.isEmpty() || m.name.toLowerCase().contains (q)) rs.push_back ({ m.id, cat, m.name });
            }
            else
            {
                for (auto& c : reg.getCommands())
                    if (c.category == cat && (q.isEmpty() || c.name.toLowerCase().contains (q) || reg.shortcutText (c.id).toLowerCase().contains (q)))
                        rs.push_back ({ c.id, cat, c.name });
            }
            if (rs.empty()) continue;
            rows.push_back ({ {}, cat, cat, true });
            rows.insert (rows.end(), rs.begin(), rs.end());
        }
        list.updateContent();
        list.repaint();
        for (int i = 0; i < (int) rows.size(); ++i)
            if (rows[(size_t) i].id == keep && keep.isNotEmpty()) list.selectRow (i);
        updateButtons();
    }

    void updateButtons()
    {
        auto id = selectedId();
        bool isMacro = reg.findMacro (id) != nullptr;
        assignBtn.setEnabled (id.isNotEmpty());
        clearBtn.setEnabled (id.isNotEmpty() && ! reg.keysFor (id).isEmpty());
        defaultBtn.setEnabled (id.isNotEmpty() && ! isMacro);
        editMacroBtn.setEnabled (isMacro);
        deleteMacroBtn.setEnabled (isMacro);
    }

    int getNumRows() override { return (int) rows.size(); }

    void paintListBoxItem (int row, Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) rows.size()) return;
        auto& r = rows[(size_t) row];
        if (r.header)
        {
            g.setColour (Col::panel2);
            g.fillRect (0, 0, w, h);
            g.setColour (Col::accent);
            g.setFont (uiFont (11.0f, true));
            g.drawText (r.name, 10, 0, w - 20, h, Justification::centredLeft);
            return;
        }
        if (selected) { g.setColour (Col::accent.withAlpha (0.25f)); g.fillRect (0, 0, w, h); }
        g.setColour (Col::text);
        g.setFont (uiFont (12.5f));
        g.drawText (r.name, 24, 0, w / 2, h, Justification::centredLeft, true);
        g.setColour (Col::accent.brighter (0.2f));
        g.setFont (monoFont (11.5f));
        g.drawText (reg.shortcutText (r.id), w / 2, 0, w / 2 - 10, h, Justification::centredRight, true);
        g.setColour (Colour (0xff1e2025));
        g.fillRect (0, h - 1, w, 1);
    }

    void selectedRowsChanged (int) override { updateButtons(); }
    void listBoxItemDoubleClicked (int row, const MouseEvent&) override { if (row >= 0 && ! rows[(size_t) row].header) startCapture(); }

    void startCapture()
    {
        auto id = selectedId();
        if (id.isEmpty()) return;
        capture.commandName = reg.displayName (id);
        capture.setVisible (true);
        capture.toFront (true);
        capture.grabKeyboardFocus();
    }

    void assignKey (const KeyPress& k)
    {
        capture.setVisible (false);
        auto id = selectedId();
        if (id.isEmpty()) return;
        auto owner = reg.ownerOf (k);
        auto apply = [this, id, k, owner]
        {
            if (owner.isNotEmpty() && owner != id)
            {
                auto ks = reg.keysFor (owner);
                for (int i = ks.size(); --i >= 0;) if (sameKey (ks[i], k)) ks.remove (i);
                reg.setKeys (owner, ks);
            }
            auto mine = reg.keysFor (id);
            bool has = false;
            for (auto& kk : mine) has = has || sameKey (kk, k);
            if (! has) mine.add (k);
            reg.setKeys (id, mine);
            refresh();
        };
        if (owner.isNotEmpty() && owner != id)
        {
            AlertWindow::showOkCancelBox (MessageBoxIconType::NoIcon, String::fromUTF8 ("Touche déjà utilisée"),
                describe (k) + String::fromUTF8 (" sert déjà à « ") + reg.displayName (owner) + String::fromUTF8 (" ».\nLa réattribuer à « ")
                    + reg.displayName (id) + String::fromUTF8 (" » ?"),
                String::fromUTF8 ("Réattribuer"), "Annuler", nullptr,
                ModalCallbackFunction::create ([apply] (int r) { if (r == 1) apply(); }));
        }
        else apply();
    }

    void newMacro()
    {
        auto* aw = new AlertWindow (String::fromUTF8 ("Nouvelle macro"), String::fromUTF8 ("Nom de la macro :"), MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", "Ma macro");
        aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw] (int r)
        {
            if (r != 1) return;
            auto n = aw->getTextEditorContents ("name").trim();
            if (n.isEmpty()) return;
            Macro m { reg.newMacroId(), n, {} };
            reg.getMacros().push_back (m);
            reg.save();
            search.clear();
            refresh();
            for (int i = 0; i < (int) rows.size(); ++i) if (rows[(size_t) i].id == m.id) list.selectRow (i);
            if (auto* mm = reg.findMacro (m.id)) editMacro (*mm);
        }), true);
    }

    /** Menu d'édition d'une macro : étapes actuelles, ajouter une commande, retirer la dernière. */
    void editMacro (Macro& m)
    {
        PopupMenu menu;
        menu.addSectionHeader (m.name + String::fromUTF8 (" : ") + (m.steps.isEmpty() ? String::fromUTF8 ("vide") : String (m.steps.size()) + String::fromUTF8 (" étape(s)")));
        for (int i = 0; i < m.steps.size(); ++i)
            menu.addItem (10000 + i, String (i + 1) + ". " + reg.displayName (m.steps[i]) + String::fromUTF8 ("   (clic : retirer)"));
        menu.addSeparator();
        PopupMenu addMenu;
        StringArray cats;
        for (auto& c : reg.getCommands()) cats.addIfNotAlreadyThere (c.category);
        int idx = 1;
        std::map<int, String> ids;
        for (auto& cat : cats)
        {
            PopupMenu sub;
            for (auto& c : reg.getCommands())
                if (c.category == cat) { ids[idx] = c.id; sub.addItem (idx++, c.name); }
            addMenu.addSubMenu (cat, sub);
        }
        menu.addSubMenu (String::fromUTF8 ("Ajouter une étape"), addMenu);
        auto macroId = m.id;
        menu.showMenuAsync (PopupMenu::Options().withTargetComponent (&editMacroBtn),
                            [this, ids, macroId] (int r)
        {
            auto* mm = reg.findMacro (macroId);
            if (mm == nullptr || r <= 0) return;
            if (r >= 10000) mm->steps.remove (r - 10000);
            else if (auto it = ids.find (r); it != ids.end()) mm->steps.add (it->second);
            reg.save();
            refresh();
            editMacro (*mm);   // on continue d'éditer (Échap pour finir)
        });
    }
};

inline void showEditor (Registry& r)
{
    DialogWindow::LaunchOptions o;
    o.content.setOwned (new EditorComponent (r));
    o.dialogTitle = String::fromUTF8 ("Raccourcis clavier et macros");
    o.dialogBackgroundColour = Col::panel1;
    o.escapeKeyTriggersCloseButton = false;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

} // namespace chen::Keys
