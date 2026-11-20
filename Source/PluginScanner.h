#pragma once

// =============================================================================
//  Analyse des plugins « hors processus » :
//  chaque plugin est ouvert dans un ChènRecPro secondaire (--chen-scan).
//  Si un plugin plante ou bloque, seul ce processus tombe : l'analyse continue
//  et le plugin fautif est simplement mis de côté.
// =============================================================================

namespace chen
{

static constexpr const char* scanArg = "--chen-scan";

inline bool isScanChildProcess()
{
    return JUCEApplicationBase::getCommandLineParameterArray()[0] == scanArg;
}

/** Côté processus secondaire : analyse un fichier et écrit le résultat en XML. */
inline int runScanChild()
{
    auto args = JUCEApplicationBase::getCommandLineParameterArray();
    if (args.size() < 4) return 2;

    auto formatName = args[1].unquoted();
    File outFile (args[2].unquoted());
    auto identifier = args[3].unquoted();

    AudioPluginFormatManager fm;
    addDefaultFormatsToManager (fm);

    for (auto* format : fm.getFormats())
    {
        if (format->getName() != formatName) continue;

        OwnedArray<PluginDescription> found;
        format->findAllTypesForFile (found, identifier);

        XmlElement root ("CHENSCAN");
        for (auto* d : found)
            root.addChildElement (d->createXml().release());

        return root.writeTo (outFile) ? 0 : 3;
    }
    return 4;
}

/** Côté application : lance un processus par plugin, avec délai maximum. */
class OutOfProcessScanner : public KnownPluginList::CustomScanner
{
public:
    bool findPluginTypesFor (AudioPluginFormat& format, OwnedArray<PluginDescription>& result,
                             const String& fileOrIdentifier) override
    {
        auto out = File::getSpecialLocation (File::tempDirectory)
                       .getNonexistentChildFile ("chenscan_" + String::toHexString (Random::getSystemRandom().nextInt()), ".xml");

        ChildProcess child;
        StringArray cmd { File::getSpecialLocation (File::currentExecutableFile).getFullPathName(),
                          scanArg, format.getName(), out.getFullPathName(), fileOrIdentifier };

        if (! child.start (cmd, 0))
            return false;

        // les plugins protégés (iLok, UAD…) peuvent être lents à s'ouvrir
        const auto deadline = Time::getMillisecondCounter() + 90000;
        while (child.isRunning())
        {
            if (shouldExit() || Time::getMillisecondCounter() > deadline)
            {
                child.kill();
                out.deleteFile();
                return ! shouldExit() ? false : true;
            }
            Thread::sleep (20);
        }

        auto ok = child.getExitCode() == 0 && out.existsAsFile();
        if (ok)
        {
            if (auto xml = parseXML (out))
                for (auto* e : xml->getChildIterator())
                {
                    PluginDescription d;
                    if (d.loadFromXml (*e))
                        result.add (new PluginDescription (d));
                }
        }
        out.deleteFile();
        return ok;
    }
};

} // namespace chen
