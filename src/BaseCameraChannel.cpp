#include "BaseCameraChannel.h"
#include "knxprod.h"

BaseCameraChannel::BaseCameraChannel(uint8_t channelIndex)
    : _channelIndex(channelIndex)
{
}

const std::string BaseCameraChannel::name()
{
    return "IPCamera";
}

void BaseCameraChannel::setup()
{
    // Polling-Intervall aus ETS-Parameter (0=5s, 1=10s, 2=30s, 3=60s)
    switch (ParamIPC_CHPollingInterval)
    {
        case 0: _pollIntervalMs = 5000;  break;
        case 1: _pollIntervalMs = 10000; break;
        case 2: _pollIntervalMs = 30000; break;
        case 3: _pollIntervalMs = 60000; break;
        default: _pollIntervalMs = 10000; break;
    }

    // Hold-Zeit aus ETS-Parameter (in Sekunden)
    _holdTimeMs = (uint32_t)ParamIPC_CHHoldTime * 1000UL;
    if (_holdTimeMs == 0)
        _holdTimeMs = IPC_HOLD_TIME_DEFAULT_MS;

    _startupDelay = millis();
    logDebugP("IPC channel %d setup, poll=%lums, hold=%lums", _channelIndex, _pollIntervalMs, _holdTimeMs);
}

void BaseCameraChannel::loop()
{
    // Startup-Verzögerung
    if (_firstPoll && (millis() - _startupDelay < IPC_STARTUP_DELAY_MS))
        return;

    // Polling-Intervall
    if (!_firstPoll && (millis() - _lastPoll < _pollIntervalMs))
    {
        processHoldTimers();
        return;
    }

    _lastPoll = millis();
    _firstPoll = false;

    // Login falls nicht online
    if (!_online)
    {
        if (!login())
        {
            setOnline(false);
            processHoldTimers();
            return;
        }
    }

    if (!pollEvents())
        setOnline(false);
    else
        setOnline(true);

    processHoldTimers();
}

void BaseCameraChannel::processInputKo(GroupObject& ko)
{
    auto index = IPC_KoCalcIndex(ko.asap());
    switch (index)
    {
        case IPC_KoPrivacyMode:
            setPrivacy((bool)KoIPC_CHPrivacyMode.value(DPT_Switch));
            break;
        case IPC_KoRecording:
            setRecording((bool)KoIPC_CHRecording.value(DPT_Switch));
            break;
        case IPC_KoPushActive:
            setPush((bool)KoIPC_CHPushActive.value(DPT_Switch));
            break;
        case IPC_KoSiren:
            setSiren((bool)KoIPC_CHSiren.value(DPT_Switch));
            break;
        case IPC_KoFloodlight:
            setFloodlight((bool)KoIPC_CHFloodlight.value(DPT_Switch));
            break;
        case IPC_KoPtzPreset:
            setPtzPreset((uint8_t)KoIPC_CHPtzPreset.value(DPT_SceneNumber));
            break;
        case IPC_KoMotionDetectActive:
            setMotionDetectActive((bool)KoIPC_CHMotionDetectActive.value(DPT_Switch));
            break;
        case IPC_KoIrLeds:
            setIrLeds((bool)KoIPC_CHIrLeds.value(DPT_Switch));
            break;
        case IPC_KoDayNightMode:
            setDayNightMode((uint8_t)KoIPC_CHDayNightMode.value(DPT_SceneNumber));
            break;
        case IPC_KoAutoTracking:
            setAutoTracking((bool)KoIPC_CHAutoTracking.value(DPT_Switch));
            break;
        case IPC_KoManualRecord:
            setManualRecord((bool)KoIPC_CHManualRecord.value(DPT_Switch));
            break;
        case IPC_KoDoNotDisturb:
            setDoNotDisturb((bool)KoIPC_CHDoNotDisturb.value(DPT_Switch));
            break;
        case IPC_KoBellLedMode:
            setBellLedMode((uint8_t)KoIPC_CHBellLedMode.value(DPT_SceneNumber));
            break;
        case IPC_KoAutoReply:
            setAutoReply((uint8_t)KoIPC_CHAutoReply.value(DPT_SceneNumber));
            break;
        case IPC_KoChimeMute:
            setChimeMute((bool)KoIPC_CHChimeMute.value(DPT_Switch));
            break;
        case IPC_KoChimeVolume:
            setChimeVolume((uint8_t)KoIPC_CHChimeVolume.value(DPT_SceneNumber));
            break;
        case IPC_KoChimeRingtone:
            setChimeRingtone((uint8_t)KoIPC_CHChimeRingtone.value(DPT_SceneNumber));
            break;
        case IPC_KoChimeTrigger:
            triggerChime();
            break;
        default:
            break;
    }
}

void BaseCameraChannel::setKoBool(uint8_t koIndex, bool value)
{
    GroupObject& ko = openknx.getGroupObject(IPC_KoCalcNumber(_channelIndex, koIndex));
    bool current = (bool)ko.value(DPT_Switch);
    if (current != value)
        ko.value(value, DPT_Switch);
}

void BaseCameraChannel::processHoldTimer(uint32_t& timer, uint8_t koIndex)
{
    if (timer == 0) return;
    if (millis() - timer >= _holdTimeMs)
    {
        timer = 0;
        setKoBool(koIndex, false);
    }
}

void BaseCameraChannel::processHoldTimers()
{
    processHoldTimer(_holdTimerMotion,   IPC_KoMotion);
    processHoldTimer(_holdTimerAnyAlarm, IPC_KoAnyAlarm);
    processHoldTimer(_holdTimerPerson,   IPC_KoPersonDetected);
    processHoldTimer(_holdTimerVehicle,  IPC_KoVehicleDetected);
    processHoldTimer(_holdTimerAnimal,   IPC_KoAnimalDetected);
    processHoldTimer(_holdTimerPet,      IPC_KoPetDetected);
    processHoldTimer(_holdTimerPackage,  IPC_KoPackageDetected);
    processHoldTimer(_holdTimerBaby,     IPC_KoBabyAlarm);
    processHoldTimer(_holdTimerFace,     IPC_KoFaceDetected);
    processHoldTimer(_holdTimerIO,       IPC_KoIOAlarm);
    processHoldTimer(_holdTimerDoorbell, IPC_KoDoorbellHold);
}

void BaseCameraChannel::setOnline(bool online)
{
    if (_online != online)
    {
        _online = online;
        setKoBool(IPC_KoOnline, online);
        logDebugP("IPC channel %d: %s", _channelIndex, online ? "online" : "offline");
    }
}
