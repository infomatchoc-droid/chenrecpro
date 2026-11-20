// =============================================================================
//  ChènRecPro — le DAW du Studio des Chènevières
//  Basé sur Tracktion Engine (GPLv3) et JUCE.
// =============================================================================

#include <JuceHeader.h>

namespace te = tracktion;
using namespace std::literals;
using namespace juce;

#include "Theme.h"
#include "LookAndFeel.h"
#include "EngineHelpers.h"
#include "PluginWindow.h"
#include "Console.h"
#include "EditView.h"

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
        g.drawText (s, sb.reduced (10, 0), Justification::centredLeft);

        g.setColour (Col::accent);
        g.drawText (String::fromUTF8 ("F3 · Console     Espace · Lecture     R · Enregistrer     Ctrl+T · Nouvelle piste"),
                    sb.reduced (10, 0), Justification::centred);

        g.setColour (saved ? Col::meterMid : Col::text2);
        g.drawText ((saved ? String::fromUTF8 ("● Projet enregistré") : String::fromUTF8 ("● Modifications non enregistrées")) + String::fromUTF8 ("   ·   ") + (edit ? editFile.getFileNameWithoutExtension() : String()),
                    sb.reduced (10, 0), Justification::centredRight);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromBottom (statusH);
        if (transportBar) transportBar->setBounds (r.removeFromTop (58));
        if (arrange) arrange->setBounds (r);
    }

    /** Fichiers passés au lancement ou glissés sur l'icône : projet ou audio. */
    void openFiles (const StringArray& paths)
    {
        for (auto& p : paths)
        {
            File f (p.unquoted());
            if (! f.existsAsFile()) continue;
            if (f.hasFileExtension (projectExtension) || f.hasFileExtension ("tracktionedit")) { openOrCreate (f); continue; }
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
        if (code == KeyPress::spaceKey)                 { EngineHelpers::togglePlay (*edit); return true; }
        if (! cmd && (code == 'R' || code == 'r' || code == KeyPress::numberPadMultiply)) { EngineHelpers::toggleRecord (*edit); return true; }
        if (code == KeyPress::homeKey || code == KeyPress::returnKey) { edit->getTransport().setPosition (0s); return true; }
        if (cmd && (code == 'S' || code == 's'))        { save(); return true; }
        if (cmd && (code == 'T' || code == 't'))        { addTrack(); return true; }
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
                break;
            case 5:
                m.addItem (50, String::fromUTF8 ("Gestionnaire de plugins…"));
                break;
            case 6:
                m.addItem (60, String::fromUTF8 ("Console\tF3"), true, consoleWindow != nullptr);
                m.addItem (61, String::fromUTF8 ("Zoom pour tout voir\tF"));
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
            case 9:  JUCEApplication::getInstance()->systemRequestedQuit(); break;
            case 10: edit->getUndoManager().undo(); break;
            case 11: edit->getUndoManager().redo(); break;
            case 20: addTrack(); break;
            case 21: if (auto* t = arrange->getSelectedTrack()) edit->deleteTrack (t); break;
            case 22: askTempo(); break;
            case 23: addMarker(); break;
            case 30: showAudioSettings(); break;
            case 31: EngineHelpers::assignInputs (*edit); break;
            case 40: EngineHelpers::togglePlay (*edit); break;
            case 41: EngineHelpers::toggleRecord (*edit); break;
            case 42: edit->getTransport().setPosition (0s); break;
            case 43: edit->getTransport().looping = ! edit->getTransport().looping.get(); break;
            case 44: edit->clickTrackEnabled = ! edit->clickTrackEnabled.get(); break;
            case 50: showPluginManager (engine); break;
            case 60: toggleConsole(); break;
            case 61: arrange->zoomToFit(); break;
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
        arrange = nullptr;
        transportBar = nullptr;

        editFile = f;
        bool isNew = ! f.existsAsFile();
        edit = isNew ? te::createEmptyEdit (engine, f) : te::loadEditFromFile (engine, f);
        edit->playInStopEnabled = true;
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
        addAndMakeVisible (*transportBar);
        addAndMakeVisible (*arrange);

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

    void save()
    {
        if (edit == nullptr) return;
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

    void timerCallback() override
    {
        if (edit && edit->hasChangedSinceSaved()) saved = false;
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
    bool moreThanOneInstanceAllowed() override       { return false; }

    void initialise (const String& commandLine) override
    {
        LookAndFeel::setDefaultLookAndFeel (&lnf);
        engine = std::make_unique<te::Engine> ("ChenRecPro", std::make_unique<chen::ChenUIBehaviour>(), nullptr);
        mainWindow = std::make_unique<chen::MainWindow> (*engine);
        anotherInstanceStarted (commandLine);
    }

    void anotherInstanceStarted (const String& commandLine) override
    {
        if (mainWindow != nullptr && commandLine.isNotEmpty())
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
