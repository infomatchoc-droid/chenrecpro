#pragma once

namespace chen
{

namespace EngineHelpers
{
    inline void armTrack (te::AudioTrack& t, bool arm, int position = 0)
    {
        for (auto* in : t.edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, position))
                in->setRecordingEnabled (t.itemID, arm);
    }

    inline bool isTrackArmed (te::AudioTrack& t, int position = 0)
    {
        for (auto* in : t.edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, position))
                return in->isRecordingEnabled (t.itemID);
        return false;
    }

    inline bool isInputMonitoringEnabled (te::AudioTrack& t, int position = 0)
    {
        for (auto* in : t.edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, position))
                return in->isLivePlayEnabled (t);
        return false;
    }

    inline void enableInputMonitoring (te::AudioTrack& t, bool on, int position = 0)
    {
        if (isInputMonitoringEnabled (t, position) == on)
            return;

        for (auto* in : t.edit.getAllInputDevices())
            if (te::isOnTargetTrack (*in, t, position))
                if (auto mode = in->getInputDevice().getMonitorMode();
                    mode == te::InputDevice::MonitorMode::on || mode == te::InputDevice::MonitorMode::off)
                    in->getInputDevice().setMonitorMode (mode == te::InputDevice::MonitorMode::on ? te::InputDevice::MonitorMode::off
                                                                                                    : te::InputDevice::MonitorMode::on);
    }

    inline void togglePlay (te::Edit& edit)
    {
        auto& tr = edit.getTransport();
        if (tr.isPlaying()) tr.stop (false, false);
        else                tr.play (false);
    }

    inline void toggleRecord (te::Edit& edit)
    {
        auto& tr = edit.getTransport();
        if (tr.isRecording()) tr.stop (true, false);
        else                  tr.record (false);
    }

    inline bool isInstrumentTrack (te::Track& t)
    {
        for (auto* p : t.pluginList.getPlugins())
            if (p->isSynth()) return true;
        return false;
    }

    inline String instrumentName (te::Track& t)
    {
        for (auto* p : t.pluginList.getPlugins())
            if (p->isSynth()) return p->getName();
        return {};
    }

    /** Active toutes les entrées MIDI (claviers maîtres), avec écoute directe. */
    inline void enableMidiInputs (te::Edit& edit)
    {
        auto& dm = edit.engine.getDeviceManager();
        for (auto& m : dm.getMidiInDevices())
        {
            if (m == nullptr || m->getDeviceType() != te::InputDevice::physicalMidiDevice) continue;
            m->setEnabled (true);
            m->setMonitorMode (te::InputDevice::MonitorMode::on);
        }
        edit.getTransport().ensureContextAllocated();
    }

    /** Le MIDI suit la sélection (comme Cubase) : les claviers jouent la piste instrument sélectionnée. */
    inline void routeMidiTo (te::AudioTrack& t)
    {
        auto& edit = t.edit;
        for (auto* in : edit.getAllInputDevices())
        {
            if (in->getInputDevice().getDeviceType() != te::InputDevice::physicalMidiDevice) continue;
            if (te::isOnTargetTrack (*in, t, 0)) continue;
            bool wasArmed = false;
            for (auto* other : te::getAudioTracks (edit))
                if (te::isOnTargetTrack (*in, *other, 0)) wasArmed = wasArmed || in->isRecordingEnabled (other->itemID);
            [[maybe_unused]] auto r = in->setTarget (t.itemID, true, nullptr, 0);
            in->setRecordingEnabled (t.itemID, wasArmed);
        }
    }

    /** Branche chaque entrée audio (mono) sur une piste, dans l'ordre. */
    inline void assignInputs (te::Edit& edit)
    {
        auto& dm = edit.engine.getDeviceManager();

        for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
            if (auto* wip = dm.getWaveInDevice (i))
            {
                wip->setChannelConfiguration (te::ChannelConfiguration::mono());
                wip->setMonitorMode (te::InputDevice::MonitorMode::automatic);
                wip->setEnabled (true);
            }

        edit.getTransport().ensureContextAllocated();

        int n = 0;
        for (auto* in : edit.getAllInputDevices())
        {
            if (in->getInputDevice().getDeviceType() != te::InputDevice::waveDevice)
                continue;

            edit.ensureNumberOfAudioTracks (n + 1);
            if (auto* t = te::getAudioTracks (edit)[n])
            {
                bool alreadyAssigned = false;
                for (auto* other : edit.getAllInputDevices())
                    if (te::isOnTargetTrack (*other, *t, 0)) alreadyAssigned = true;

                if (! alreadyAssigned)
                {
                    [[maybe_unused]] auto r = in->setTarget (t->itemID, true, &edit.getUndoManager(), 0);
                    in->setRecordingEnabled (t->itemID, false);
                }
            }
            if (++n >= 8) break;
        }

        edit.restartPlayback();
    }
}

} // namespace chen
