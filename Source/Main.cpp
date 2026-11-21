// =============================================================================
//  ChènRecPro — le DAW du Studio des Chènevières
//  Basé sur Tracktion Engine (GPLv3) et JUCE.
// =============================================================================

#include <JuceHeader.h>

// VariVox (correction de justesse) : moteur et éditeur de notes du plugin
#include "Take.h"
#include "NoteView.h"
#include "Audition.h"

namespace te = tracktion;
using namespace std::literals;
using namespace juce;

#include "Theme.h"
#include "LookAndFeel.h"
#include "EngineHelpers.h"
#include "PluginWindow.h"
#include "PluginScanner.h"
#include "Mixing.h"
#include "Groups.h"
#include "Console.h"
#include "Playlists.h"
#include "Comping.h"
#include "EditView.h"
#include "Inspector.h"
#include "PianoRoll.h"
#include "SessionView.h"
#include "VariVoxPanel.h"

namespace chen
{

static const char* projectExtension = ".chenrec";

//==============================================================================
class MainComponent : public Component,
                      public MenuBarModel,
                      private Timer
{
public:
    explicit MainComponent (te::Engine& e) : engine (e)
    {
        setWantsKeyboardFocus (true);
        preferAsioOnFirstLaunch();
        openOrCreate (getDefaultProjectFile());
        startTimerHz (2);
        setSize (1600, 960);
    }

    ~MainComponent() override
    {
        consoleWindow = nullptr;
        if (edit != nullptr) save();
        session = nullptr;
        variVox = nullptr;
        pianoRoll = nullptr;
        inspector = nullptr;
        arrange = nullptr;
        transportBar = nullptr;
        edit = nullptr;
    }

    //==========================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (Col::bg);

        auto sb = getLocalBounds().removeFromBottom (statusH).toFloat();
        g.setColour (Colour (0xff0f1012));
        g.fillRect (sb);
        g.setColour (Colours::black);
        g.fillRect (sb.withHeight (1));

        auto& dm = engine.getDeviceManager();
        auto* dev = dm.deviceManager.getCurrentAudioDevice();
        String s;
        s << String (dm.getSampleRate() / 1000.0, 1) << String::fromUTF8 (" kHz   ·   ")
          << (dev ? dev->getTypeName() + String::fromUTF8 (" · ") + dev->getName() : String::fromUTF8 ("Aucune interface audio"))
          << String::fromUTF8 ("   ·   Buffer ") << (dev ? String (dev->getCurrentBufferSizeSamples()) : String (String::fromUTF8 ("—")))
          << String::fromUTF8 ("   ·   Latence ") << (dev ? String (1000.0 * (dev->getInputLatencyInSamples() + dev->getOutputLatencyInSamples()) / jmax (1.0, dm.getSampleRate()), 1) + " ms" : String (String::fromUTF8 ("—")));
        g.setColour (Col::text3);
        g.setFont (uiFont (11.5f));
        auto font = uiFont (11.5f);
        g.setFont (font);
        auto area = sb.reduced (10, 0);
        g.drawText (s, area, Justification::centredLeft);
        auto leftW = GlyphArrangement::getStringWidth (font, s) + 24.0f;

        auto right = (saved ? String::fromUTF8 ("● Projet enregistré") : String::fromUTF8 ("● Modifications non enregistrées"))
                     + String::fromUTF8 ("   ·   ") + (edit ? editFile.getFileNameWithoutExtension() : String());
        auto rightW = GlyphArrangement::getStringWidth (font, right) + 24.0f;
        g.setColour (saved ? Col::meterMid : Col::text2);
        g.drawText (right, area, Justification::centredRight);

        // raccourcis : uniquement dans la place libre entre les deux textes
        auto middle = area.withTrimmedLeft (leftW).withTrimmedRight (rightW);
        const char* shortcuts[] = { "F3 · Console", "Espace · Lecture", "R · Enregistrer", "Tab · Session", "B · Séparer",
                                    "Ctrl+D · Dupliquer", "Maj+R · MIDI rétro", "A · Automation", "W · Écriture" };
        String line;
        for (auto* sc : shortcuts)
        {
            auto candidate = line.isEmpty() ? String::fromUTF8 (sc) : line + "     " + String::fromUTF8 (sc);
            if (GlyphArrangement::getStringWidth (font, candidate) > middle.getWidth()) break;
            line = candidate;
        }
        g.setColour (Col::accent);
        g.drawText (line, middle, Justification::centred);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromBottom (statusH);
        if (transportBar) transportBar->setBounds (r.removeFromTop (58));
        if (pianoRoll && pianoRoll->isVisible()) pianoRoll->setBounds (r.removeFromBottom (jmin (380, r.getHeight() / 2)));
        if (variVox && variVox->isVisible()) variVox->setBounds (r.removeFromBottom (jmin (460, r.getHeight() * 11 / 20)));
        if (inspector) { inspector->setVisible (showInspector); if (showInspector) inspector->setBounds (r.removeFromLeft (Inspector::width)); }
        if (arrange) { arrange->setVisible (! showSession); arrange->setBounds (r); }
        if (session) { session->setVisible (showSession); session->setBounds (r); }
    }

    /** Fichiers passés au lancement ou glissés sur l'icône : projet ou audio. */
    void openFiles (const StringArray& paths)
    {
        for (auto& p : paths)
        {
            File f (p.unquoted());
            if (! f.existsAsFile()) continue;
            if (f.hasFileExtension (projectExtension) || f.hasFileExtension ("tracktionedit")) { openOrCreate (f); continue; }
            if (f.hasFileExtension ("mid;midi"))
            {
                if (auto t = edit->insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr))
                {
                    t->setName (f.getFileNameWithoutExtension());
                    t->setColour (Colour (0xffa687e6));
                    arrange->importMidiFile (*t, f, 0.0);
                }
                continue;
            }
            if (auto t = edit->insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr))
            {
                auto n = te::getAudioTracks (*edit).size() - 1;
                t->setName (f.getFileNameWithoutExtension());
                t->setColour (Col::track (n));
                arrange->importFile (*t, f, 0.0);
            }
        }
    }

    bool keyPressed (const KeyPress& k) override
    {
        auto cmd = k.getModifiers().isCommandDown();
        auto code = k.getKeyCode();

        if (code == KeyPress::F3Key)                    { toggleConsole(); return true; }
        if (code == KeyPress::tabKey)                   { showSession = ! showSession; resized(); return true; }
        if (code == KeyPress::spaceKey)                 { EngineHelpers::togglePlay (*edit); return true; }
        if (! cmd && k.getModifiers().isShiftDown() && (code == 'R' || code == 'r' || code == KeyPress::numberPadMultiply))
        {
            auto res = edit->getTransport().applyRetrospectiveRecord (false);   // MIDI rétrospectif
            ignoreUnused (res);
            return true;
        }
        if (! cmd && (code == 'R' || code == 'r' || code == KeyPress::numberPadMultiply)) { Comping::toggleRecord (*edit); return true; }
        if (cmd && k.getModifiers().isShiftDown() && (code == 'T' || code == 't')) { addInstrumentTrack(); return true; }
        if (code == KeyPress::homeKey || code == KeyPress::returnKey) { edit->getTransport().setPosition (0s); return true; }
        if (cmd && (code == 'S' || code == 's'))        { save(); return true; }
        if (cmd && (code == 'T' || code == 't'))        { addTrack(); return true; }
        if (cmd && (code == 'G' || code == 'g'))
        {
            if (k.getModifiers().isShiftDown()) ungroupSelection();
            else                                groupSelection();
            return true;
        }
        if (cmd && (code == 'I' || code == 'i'))        { importAudio(); return true; }
        if (cmd && (code == 'Z' || code == 'z'))
        {
            if (k.getModifiers().isShiftDown()) edit->getUndoManager().redo();
            else                                edit->getUndoManager().undo();
            return true;
        }
        if (! cmd && (code == 'L' || code == 'l'))     { edit->getTransport().looping = ! edit->getTransport().looping.get(); return true; }
        if (! cmd && (code == 'C' || code == 'c'))     { edit->clickTrackEnabled = ! edit->clickTrackEnabled.get(); return true; }
        if (! cmd && (code == 'F' || code == 'f'))     { arrange->zoomToFit(); return true; }
        if (! cmd && (code == 'I' || code == 'i'))     { showInspector = ! showInspector; resized(); return true; }
        if (! cmd && (code == 'A' || code == 'a'))     { arrange->toggleAutomationView(); return true; }
        if (! cmd && (code == 'W' || code == 'w'))
        {
            auto& arm = edit->getAutomationRecordManager();
            arm.setWritingAutomation (! arm.isWritingAutomation());
            return true;
        }
        return false;
    }

    //==========================================================================
    StringArray getMenuBarNames() override
    {
        return { "Fichier", String::fromUTF8 ("Édition"), "Projet", "Audio", "Transport", "Studio", String::fromUTF8 ("Fenêtre"), "Aide" };
    }

    PopupMenu getMenuForIndex (int index, const String&) override
    {
        PopupMenu m;
        switch (index)
        {
            case 0:
                m.addItem (1, "Nouveau projet", true, false); m.addItem (2, String::fromUTF8 ("Ouvrir…"));
                m.addItem (3, "Enregistrer\tCtrl+S"); m.addItem (4, String::fromUTF8 ("Enregistrer sous…"));
                m.addSeparator();
                m.addItem (5, String::fromUTF8 ("Importer un fichier audio…\tCtrl+I"));
                m.addItem (6, String::fromUTF8 ("Exporter le mix (WAV)…"));
                m.addSeparator();
                m.addItem (9, "Quitter");
                break;
            case 1:
                m.addItem (10, "Annuler\tCtrl+Z", edit && edit->getUndoManager().canUndo());
                m.addItem (11, String::fromUTF8 ("Rétablir\tCtrl+Maj+Z"), edit && edit->getUndoManager().canRedo());
                break;
            case 2:
                m.addItem (20, "Nouvelle piste audio\tCtrl+T");
                m.addItem (24, String::fromUTF8 ("Nouvelle piste instrument…\tCtrl+Maj+T"));
                m.addItem (25, String::fromUTF8 ("Nouvelle piste FX : Réverb"));
                m.addItem (26, String::fromUTF8 ("Nouvelle piste FX : Délai"));
                m.addItem (21, String::fromUTF8 ("Supprimer la piste sélectionnée"));
                m.addSeparator();
                m.addItem (22, String::fromUTF8 ("Tempo du projet…"));
                m.addItem (23, "Ajouter un marqueur au curseur");
                break;
            case 3:
                m.addItem (30, String::fromUTF8 ("Réglages audio…"));
                m.addItem (31, String::fromUTF8 ("Réassigner les entrées aux pistes"));
                break;
            case 4:
                m.addItem (40, "Lecture / Stop\tEspace");
                m.addItem (41, "Enregistrer\tR");
                m.addItem (42, String::fromUTF8 ("Retour au début\tEntrée"));
                m.addItem (43, "Boucle\tL", true, edit && edit->getTransport().looping.get());
                m.addItem (44, String::fromUTF8 ("Métronome\tC"), true, edit && edit->clickTrackEnabled.get());
                m.addItem (45, String::fromUTF8 ("MIDI rétrospectif\tMaj+R"));
                m.addItem (46, String::fromUTF8 ("Nouvelle playlist à chaque prise"), true, edit && Comping::newPlaylistPerTake (*edit));
                break;
            case 5:
                m.addItem (50, String::fromUTF8 ("Gestionnaire de plugins…"));
                break;
            case 6:
                m.addItem (64, String::fromUTF8 ("Vue Session (Ableton)\tTab"), true, showSession);
                m.addItem (60, String::fromUTF8 ("Console\tF3"), true, consoleWindow != nullptr);
                m.addItem (61, String::fromUTF8 ("Zoom pour tout voir\tF"));
                m.addItem (62, "Inspecteur\tI", true, showInspector);
                m.addItem (63, "Automation du volume\tA", true, arrange && arrange->automationView);
                break;
            case 7:
                m.addItem (70, String::fromUTF8 ("À propos de ChènRecPro"));
                break;
            default: break;
        }
        return m;
    }

    void menuItemSelected (int id, int) override
    {
        switch (id)
        {
            case 1:  newProject(); break;
            case 2:  openProject(); break;
            case 3:  save(); break;
            case 4:  saveAs(); break;
            case 5:  importAudio(); break;
            case 6:  exportMix(); break;
            case 7:  exportStems(); break;
            case 27: groupSelection(); break;
            case 28: ungroupSelection(); break;
            case 9:  JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 10: edit->getUndoManager().undo(); break;
            case 11: edit->getUndoManager().redo(); break;
            case 20: addTrack(); break;
            case 24: addInstrumentTrack(); break;
            case 46: Comping::setNewPlaylistPerTake (*edit, ! Comping::newPlaylistPerTake (*edit)); break;
            case 45: { auto res = edit->getTransport().applyRetrospectiveRecord (false); ignoreUnused (res); break; }
            case 21: if (auto* t = arrange->getSelectedTrack()) edit->deleteTrack (t); break;
            case 22: askTempo(); break;
            case 23: addMarker(); break;
            case 30: showAudioSettings(); break;
            case 31: EngineHelpers::assignInputs (*edit); break;
            case 40: EngineHelpers::togglePlay (*edit); break;
            case 41: Comping::toggleRecord (*edit); break;
            case 42: edit->getTransport().setPosition (0s); break;
            case 43: edit->getTransport().looping = ! edit->getTransport().looping.get(); break;
            case 44: edit->clickTrackEnabled = ! edit->clickTrackEnabled.get(); break;
            case 50: showPluginManager (engine); break;
            case 60: toggleConsole(); break;
            case 64: showSession = ! showSession; resized(); break;
            case 61: arrange->zoomToFit(); break;
            case 62: showInspector = ! showInspector; resized(); break;
            case 63: arrange->toggleAutomationView(); break;
            case 25: Mixing::createFxTrack (*edit, Mixing::FxKind::reverb); break;
            case 26: Mixing::createFxTrack (*edit, Mixing::FxKind::delay); break;
            case 70: showAbout(); break;
            default: break;
        }
    }

private:
    te::Engine& engine;
    std::unique_ptr<te::Edit> edit;
    File editFile;
    std::unique_ptr<TransportBar> transportBar;
    std::unique_ptr<ArrangeView> arrange;
    std::unique_ptr<Inspector> inspector;
    std::unique_ptr<PianoRoll> pianoRoll;
    std::unique_ptr<VariVoxPanel> variVox;
    std::unique_ptr<SessionView> session;
    bool showSession = false;
    Component menuAnchor;
    bool showInspector = true;
    std::unique_ptr<ConsoleWindow> consoleWindow;
    bool saved = true;
    static constexpr int statusH = 24;

    static File getProjectsFolder()
    {
        auto d = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("ChenRecPro");
        d.createDirectory();
        return d;
    }

    File getDefaultProjectFile() const
    {
        auto last = engine.getPropertyStorage().getPropertiesFile().getValue ("chenLastProject");
        if (File::isAbsolutePath (last) && File (last).existsAsFile())
            return File (last);
        return getProjectsFolder().getChildFile (String::fromUTF8 ("Projet sans titre") + projectExtension);
    }

    void openOrCreate (const File& f)
    {
        consoleWindow = nullptr;
        if (edit != nullptr) save();
        session = nullptr;
        variVox = nullptr;
        pianoRoll = nullptr;
        inspector = nullptr;
        arrange = nullptr;
        transportBar = nullptr;

        editFile = f;
        bool isNew = ! f.existsAsFile();
        edit = isNew ? te::createEmptyEdit (engine, f) : te::loadEditFromFile (engine, f);
        edit->playInStopEnabled = true;
        edit->getAutomationRecordManager().setWritingAutomation (false);   // lecture par défaut (W pour écrire)
        edit->getAutomationRecordManager().setReadingAutomation (true);
        edit->getTransport().ensureContextAllocated();

        if (isNew)
        {
            edit->tempoSequence.getTempo (0)->setBpm (120.0);
            int i = 0;
            for (auto* t : te::getAudioTracks (*edit))
            {
                t->setName ("Audio " + String (i + 1));
                t->setColour (Col::track (i));
                ++i;
            }
            while (te::getAudioTracks (*edit).size() < 4) addTrack();
            EngineHelpers::assignInputs (*edit);
        }

        transportBar = std::make_unique<TransportBar> (*edit);
        arrange = std::make_unique<ArrangeView> (*edit, *transportBar);
        inspector = std::make_unique<Inspector> (*edit, *arrange);
        arrange->onSelectionChanged = [this] { if (inspector) inspector->refresh(); };
        pianoRoll = std::make_unique<PianoRoll> (*edit);
        addChildComponent (*pianoRoll);
        pianoRoll->onClose = [this] { pianoRoll->setVisible (false); pianoRoll->setClip (nullptr); resized(); arrange->grabKeyboardFocus(); };
        arrange->onOpenMidiEditor = [this] (te::MidiClip* c) { openMidiEditor (c); };
        variVox = std::make_unique<VariVoxPanel> (*edit);
        addChildComponent (*variVox);
        variVox->onClose = [this] { variVox->setVisible (false); variVox->setClip (nullptr); resized(); arrange->grabKeyboardFocus(); };
        arrange->onOpenVariVox = [this] (te::WaveAudioClip* c) { openVariVox (c); };
        session = std::make_unique<SessionView> (*edit);
        addChildComponent (*session);
        session->onOpenMidiEditor = [this] (te::MidiClip* c) { openMidiEditor (c); };
        session->onSelectTrack = [this] (te::AudioTrack* t) { arrange->selectTrack (t); };
        EngineHelpers::enableMidiInputs (*edit);
        addAndMakeVisible (*transportBar);
        addAndMakeVisible (*arrange);
        addAndMakeVisible (*inspector);

        engine.getPropertyStorage().getPropertiesFile().setValue ("chenLastProject", f.getFullPathName());
        if (isNew) save();
        resized();
        updateTitle();
    }

    /** Sous Windows, bascule une première fois sur ASIO (cartes pro : SSL, RME…). */
    void preferAsioOnFirstLaunch()
    {
       #if JUCE_WINDOWS
        auto& props = engine.getPropertyStorage().getPropertiesFile();
        if (props.getBoolValue ("chenAsioChosen", false)) return;

        auto& adm = engine.getDeviceManager().deviceManager;
        for (auto* type : adm.getAvailableDeviceTypes())
        {
            if (type->getTypeName() != "ASIO") continue;
            type->scanForDevices();
            if (type->getDeviceNames().isEmpty()) return;
            adm.setCurrentAudioDeviceType ("ASIO", true);
            props.setValue ("chenAsioChosen", true);
            return;
        }
       #endif
    }

    void addTrack()
    {
        auto n = te::getAudioTracks (*edit).size();
        if (auto t = edit->insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr))
        {
            t->setName ("Audio " + String (n + 1));
            t->setColour (Col::track (n));
        }
        saved = false;
    }

    void openVariVox (te::WaveAudioClip* c)
    {
        if (pianoRoll->isVisible()) pianoRoll->onClose();
        variVox->setClip (c);
        variVox->setVisible (c != nullptr);
        resized();
        variVox->grabFocusOnEditor();
    }

    void openMidiEditor (te::MidiClip* c)
    {
        if (variVox && variVox->isVisible()) variVox->onClose();
        pianoRoll->setClip (c);
        pianoRoll->setVisible (c != nullptr);
        resized();
        pianoRoll->grabKeyboardFocus();
    }

    void addInstrumentTrack()
    {
        auto n = 0;
        for (auto* t : te::getAudioTracks (*edit)) if (EngineHelpers::isInstrumentTrack (*t)) ++n;
        auto t = edit->insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr);
        if (t == nullptr) return;
        t->setName ("Instrument " + String (n + 1));
        t->setColour (Colour (0xffa687e6));
        arrange->selectTrack (t.get());

        menuAnchor.setBounds (Inspector::width + 260, 120, 10, 10);
        addChildComponent (menuAnchor);
        te::Track::Ptr keep (t.get());
        InsertMenu::show (engine, menuAnchor, false, [this, keep] (InsertMenu::Choice c)
        {
            auto* at = dynamic_cast<te::AudioTrack*> (keep.get());
            if (at == nullptr || c.remove || c.manager) return;
            te::Plugin::Ptr p = c.builtinType.isNotEmpty()
                                  ? edit->getPluginCache().createNewPlugin (c.builtinType, {})
                                  : edit->getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, c.desc);
            if (p == nullptr) return;
            at->pluginList.insertPlugin (p, 0, nullptr);
            at->setName (p->getName());
            EngineHelpers::routeMidiTo (*at);
            arrange->selectTrack (at);
            p->showWindowExplicitly();
        }, true);
        saved = false;
    }

    void save()
    {
        if (edit == nullptr) return;
        if (variVox) variVox->flush();
        te::EditFileOperations (*edit).save (true, true, false);
        saved = true;
        repaint();
    }

    void saveAs()
    {
        auto fc = std::make_shared<FileChooser> (String::fromUTF8 ("Enregistrer le projet sous…"), getProjectsFolder(), String ("*") + projectExtension);
        fc->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                         [this, fc] (const FileChooser&)
                         {
                             auto f = fc->getResult();
                             if (f == File()) return;
                             f = f.withFileExtension (projectExtension);
                             te::EditFileOperations (*edit).saveAs (f, true, nullptr);
                             editFile = f;
                             engine.getPropertyStorage().getPropertiesFile().setValue ("chenLastProject", f.getFullPathName());
                             updateTitle();
                         });
    }

    void newProject()
    {
        auto fc = std::make_shared<FileChooser> ("Nouveau projet", getProjectsFolder(), String ("*") + projectExtension);
        fc->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                         [this, fc] (const FileChooser&)
                         {
                             auto f = fc->getResult();
                             if (f == File()) return;
                             f = f.withFileExtension (projectExtension);
                             f.deleteFile();
                             openOrCreate (f);
                         });
    }

    void openProject()
    {
        auto fc = std::make_shared<FileChooser> ("Ouvrir un projet", getProjectsFolder(), String ("*") + projectExtension + ";*.tracktionedit");
        fc->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                         [this, fc] (const FileChooser&)
                         {
                             if (fc->getResult().existsAsFile())
                                 openOrCreate (fc->getResult());
                         });
    }

    void importAudio()
    {
        auto fc = std::make_shared<FileChooser> ("Importer un fichier audio", File::getSpecialLocation (File::userMusicDirectory),
                                                 engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats());
        fc->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectMultipleItems | FileBrowserComponent::canSelectFiles,
                         [this, fc] (const FileChooser&)
                         {
                             auto* t = arrange->getSelectedTrack();
                             for (auto& f : fc->getResults())
                             {
                                 if (t == nullptr)
                                     t = edit->insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (*edit), nullptr).get();
                                 if (t == nullptr) break;
                                 arrange->importFile (*t, f, edit->getTransport().getPosition().inSeconds());
                                 t = nullptr;
                             }
                             saved = false;
                         });
    }

    void exportMix()
    {
        auto fc = std::make_shared<FileChooser> ("Exporter le mix", getProjectsFolder().getChildFile (editFile.getFileNameWithoutExtension() + ".wav"), "*.wav");
        fc->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::warnAboutOverwriting,
                         [this, fc] (const FileChooser&)
                         {
                             auto f = fc->getResult();
                             if (f == File()) return;
                             f = f.withFileExtension (".wav");
                             juce::BigInteger tracksToDo;
                             for (int i = 0; i < te::getAllTracks (*edit).size(); ++i) tracksToDo.setBit (i);
                             auto range = te::TimeRange (0s, edit->getLength());
                             te::Renderer::renderToFile (String::fromUTF8 ("Export ChènRecPro"), f, *edit, range, tracksToDo, true, true, {}, true);
                             f.revealToUser();
                         });
    }

    void groupSelection()
    {
        auto tracks = arrange->getSelectedTracks();
        if (tracks.size() < 2)
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Grouper",
                String::fromUTF8 ("Sélectionne au moins deux pistes : Ctrl+clic (ou Maj+clic) sur leurs en-têtes, puis Ctrl+G."));
            return;
        }
        Groups::create (*edit, tracks);
        arrange->multiSel.clear();
        repaint();
    }

    void ungroupSelection()
    {
        for (auto* t : arrange->getSelectedTracks()) Groups::removeFromGroup (*t);
        repaint();
    }

    /** Un fichier WAV par piste (inserts et fader compris), dans un dossier au choix. */
    void exportStems()
    {
        auto fc = std::make_shared<FileChooser> (String::fromUTF8 ("Dossier des stems"), getProjectsFolder());
        fc->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories,
                         [this, fc] (const FileChooser&)
        {
            auto dir = fc->getResult();
            if (! dir.isDirectory()) return;
            auto all = te::getAllTracks (*edit);
            auto range = te::TimeRange (0s, edit->getLength());
            int n = 0;
            for (auto* t : te::getAudioTracks (*edit))
            {
                if (t->getClips().isEmpty()) continue;
                juce::BigInteger bits;
                bits.setBit (all.indexOf (t));
                auto f = dir.getChildFile (File::createLegalFileName (editFile.getFileNameWithoutExtension() + " - " + t->getName()) + ".wav");
                te::Renderer::renderToFile (String::fromUTF8 ("Stem ") + t->getName(), f, *edit, range, bits, true, true, {}, true);
                ++n;
            }
            dir.revealToUser();
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Stems",
                String (n) + String::fromUTF8 (" stem(s) exporté(s) dans :\n") + dir.getFullPathName());
        });
    }

    void askTempo()
    {
        auto* aw = new AlertWindow ("Tempo du projet", "BPM", MessageBoxIconType::NoIcon);
        aw->addTextEditor ("bpm", String (edit->tempoSequence.getTempo (0)->getBpm(), 2));
        aw->addButton ("OK", 1, KeyPress (KeyPress::returnKey));
        aw->addButton ("Annuler", 0, KeyPress (KeyPress::escapeKey));
        aw->enterModalState (true, ModalCallbackFunction::create ([this, aw] (int r)
        {
            if (r == 1)
                edit->tempoSequence.getTempo (0)->setBpm (jlimit (20.0, 400.0, aw->getTextEditorContents ("bpm").getDoubleValue()));
        }), true);
    }

    void addMarker()
    {
        edit->ensureMarkerTrack();
        if (auto* mt = edit->getMarkerTrack())
        {
            auto pos = edit->getTransport().getPosition();
            auto len = edit->tempoSequence.toTime (edit->tempoSequence.toBeats (pos) + te::BeatDuration::fromBeats (16.0)) - pos;
            mt->insertNewClip (te::TrackItem::Type::marker, "Section " + String (mt->getClips().size() + 1), { pos, pos + len }, nullptr);
        }
    }

    void showAudioSettings()
    {
        DialogWindow::LaunchOptions o;
        o.dialogTitle = String::fromUTF8 ("Réglages audio — ChènRecPro");
        o.dialogBackgroundColour = Col::panel1;
        o.content.setOwned (new AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager, 0, 256, 1, 256, false, false, true, true));
        o.content->setSize (520, 640);
        o.useNativeTitleBar = true;
        o.launchAsync();
    }

    void showAbout()
    {
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::NoIcon, String::fromUTF8 ("ChènRecPro ") + JUCE_APPLICATION_VERSION_STRING,
            String::fromUTF8 ("Le DAW du Studio des Chènevières.\nDivision Digitale & Créative.\n\nBasé sur Tracktion Engine et JUCE (GPLv3)."));
    }

    void toggleConsole()
    {
        if (consoleWindow != nullptr)
        {
            consoleWindow = nullptr;
            grabKeyboardFocus();
            return;
        }
        consoleWindow = std::make_unique<ConsoleWindow> (*edit, [this] { MessageManager::callAsync ([sp = SafePointer<MainComponent> (this)] { if (sp) sp->toggleConsole(); }); });
        consoleWindow->toFront (true);
    }

    void updateTitle()
    {
        if (auto* w = findParentComponentOfClass<DocumentWindow>())
            w->setName (String::fromUTF8 ("ChènRecPro — ") + editFile.getFileNameWithoutExtension());
    }

    int autosaveTicks = 0;

    void timerCallback() override
    {
        if (edit && edit->hasChangedSinceSaved()) saved = false;

        // sauvegarde automatique toutes les 60 s (jamais pendant un enregistrement)
        if (++autosaveTicks >= 120)
        {
            autosaveTicks = 0;
            if (edit && ! saved && ! edit->getTransport().isRecording())
                save();
        }
        repaint (getLocalBounds().removeFromBottom (statusH));
    }
};

//==============================================================================
class MainWindow : public DocumentWindow
{
public:
    MainComponent* getMain() { return dynamic_cast<MainComponent*> (getContentComponent()); }

    MainWindow (te::Engine& e)
        : DocumentWindow (String::fromUTF8 ("ChènRecPro"), Col::bg, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        auto* mc = new MainComponent (e);
        setContentOwned (mc, true);
       #if JUCE_MAC
        MenuBarModel::setMacMainMenu (mc);
       #else
        setMenuBar (mc, 26);
       #endif
        setResizable (true, false);
        auto area = Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea;
        setBounds (area.reduced (20));
        setVisible (true);
        mc->grabKeyboardFocus();
    }

    ~MainWindow() override
    {
       #if JUCE_MAC
        MenuBarModel::setMacMainMenu (nullptr);
       #else
        setMenuBar (nullptr);
       #endif
    }

    void closeButtonPressed() override { JUCEApplication::getInstance()->systemRequestedQuit(); }
};

} // namespace chen

//==============================================================================
class ChenRecProApplication : public JUCEApplication
{
public:
    const String getApplicationName() override       { return "ChenRecPro"; }
    const String getApplicationVersion() override    { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override       { return chen::isScanChildProcess(); }

    void initialise (const String& commandLine) override
    {
        if (chen::isScanChildProcess())
        {
            setApplicationReturnValue (chen::runScanChild());
            quit();
            return;
        }

        LookAndFeel::setDefaultLookAndFeel (&lnf);
        engine = std::make_unique<te::Engine> ("ChenRecPro", std::make_unique<chen::ChenUIBehaviour>(), nullptr);
        engine->getPluginManager().knownPluginList.setCustomScanner (std::make_unique<chen::OutOfProcessScanner>());
        mainWindow = std::make_unique<chen::MainWindow> (*engine);
        anotherInstanceStarted (commandLine);
    }

    void anotherInstanceStarted (const String& commandLine) override
    {
        if (mainWindow != nullptr && commandLine.isNotEmpty() && ! commandLine.contains (chen::scanArg))
            if (auto* mc = mainWindow->getMain())
                mc->openFiles (StringArray::fromTokens (commandLine, true));
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        engine = nullptr;
        LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    chen::ChenLookAndFeel lnf;
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<chen::MainWindow> mainWindow;
};

START_JUCE_APPLICATION (ChenRecProApplication)
