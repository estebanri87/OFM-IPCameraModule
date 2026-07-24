#include "BaseCameraChannel.h"
#include "knxprod.h"
#include "NetworkModule.h"

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

    // Verbindungsmodus (JSON / ONVIF / Combined)
    _connectionMode = ParamIPC_CHConnectionMode;

    // Kamera-Zugangsdaten cachen (für ONVIF)
    strncpy(_camIp,   (const char*)ParamIPC_CHIpAddress, sizeof(_camIp)   - 1);
    strncpy(_camUser, (const char*)ParamIPC_CHUsername,  sizeof(_camUser) - 1);
    strncpy(_camPass, (const char*)ParamIPC_CHPassword,  sizeof(_camPass) - 1);
    strncpy(_snapshotUrl, (const char*)ParamIPC_CHSnapshotURL, sizeof(_snapshotUrl) - 1);

#ifdef ARDUINO_ARCH_ESP32
    if (_connectionMode >= IPC_MODE_ONVIF_ONLY)
    {
        _onvifPort = (uint16_t)ParamIPC_CHOnvifPort;
        if (_onvifPort == 0) _onvifPort = 80;
        const char* path = (const char*)ParamIPC_CHOnvifPath;
        strncpy(_onvifPath, (path && path[0]) ? path : "/onvif/event_service",
                sizeof(_onvifPath) - 1);
    }
#endif

    _startupDelay = millis();
    logDebugP("IPC channel %d setup, mode=%d poll=%lums, hold=%lums",
              _channelIndex, _connectionMode, _pollIntervalMs, _holdTimeMs);
}

void BaseCameraChannel::loop()
{
    // Netzwerk muss verfügbar sein
    if (!openknxNetwork.established())
        return;

    // IP-Adresse muss konfiguriert sein
    if (_camIp[0] == '\0')
        return;

    // Startup-Verzögerung (15s Basis + 5s * Kanal-Index, damit nicht alle gleichzeitig starten)
    uint32_t startupDelay = IPC_STARTUP_DELAY_MS + (uint32_t)_channelIndex * IPC_STARTUP_STAGGER_MS;
    if (_firstPoll && (millis() - _startupDelay < startupDelay))
        return;

#ifdef ARDUINO_ARCH_ESP32
    // Start ONVIF task after startup delay (once)
    if (_firstPoll && _connectionMode >= IPC_MODE_ONVIF_ONLY && _onvifTaskHandle == nullptr)
        startOnvifTask();

    // Process ONVIF events from queue (non-blocking)
    if (_eventQueue != nullptr)
    {
        OnvifEvent ev;
        while (xQueueReceive(_eventQueue, &ev, 0) == pdTRUE)
        {
            onOnvifEvent(ev.topic, ev.state);
        }
    }

    // ONVIF-only mode: JSON polling disabled
    if (_connectionMode == IPC_MODE_ONVIF_ONLY)
    {
        processHoldTimers();
        _firstPoll = false;  // mark startup done
        return;
    }
#endif

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
        case IPC_KoMotionSensitivity:
            setMotionSensitivity((uint8_t)KoIPC_CHMotionSensitivity.value(DPT_Scaling));
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
            setBellLedMode((bool)KoIPC_CHBellLedMode.value(DPT_Switch));
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
    GroupObject& ko = knx.getGroupObject(IPC_KoCalcNumber(koIndex));
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

void BaseCameraChannel::triggerSnapshot()
{
    setKoBool(IPC_KoSnapshotTrigger, true);
    // Single-shot: immediately clear (no hold timer — receiver should latch)
    setKoBool(IPC_KoSnapshotTrigger, false);

    if (_snapshotUrl[0] == '\0')
        return;

    // Fire-and-forget: die Antwort (typisch ein JPEG) wird verworfen, nur der
    // Aufruf zählt — z.B. als Webhook oder um einen Upload der Kamera anzustoßen.
    HTTPClient http;
    if (!http.begin(_snapshotUrl))
    {
        logErrorP("IPC ch%d: Snapshot-URL ungültig", _channelIndex);
        return;
    }
    http.setConnectTimeout(IPC_SNAPSHOT_TIMEOUT_MS);
    http.setTimeout(IPC_SNAPSHOT_TIMEOUT_MS);

    int code = http.GET();
    if (code <= 0)
        logErrorP("IPC ch%d: Snapshot fehlgeschlagen (%d)", _channelIndex, code);
    else
        logDebugP("IPC ch%d: Snapshot ausgelöst, HTTP %d", _channelIndex, code);

    http.end();
}

#ifdef ARDUINO_ARCH_ESP32
void BaseCameraChannel::startOnvifTask()
{
    _eventQueue = xQueueCreate(16, sizeof(OnvifEvent));
    if (!_eventQueue)
    {
        logErrorP("IPC ch%d: failed to create ONVIF event queue", _channelIndex);
        return;
    }
    char taskName[20];
    snprintf(taskName, sizeof(taskName), "onvif_%d", _channelIndex);
    BaseType_t res = xTaskCreate(
        onvifTaskFunc,
        taskName,
        8192 + 2048,  // stack size (needs room for 1500-byte SOAP buffers)
        this,
        1,      // priority
        &_onvifTaskHandle
    );
    if (res != pdPASS)
    {
        logErrorP("IPC ch%d: failed to create ONVIF task", _channelIndex);
        _onvifTaskHandle = nullptr;
    }
}

void BaseCameraChannel::onvifTaskBody()
{
    // Wait a bit before first connect to let the network come up
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (true)
    {
        if (!_onvifClient.isSubscribed())
        {
            logInfoP("ONVIF ch%d: subscribing...", _channelIndex);
            bool ok = _onvifClient.subscribe(_camIp, _onvifPort, _onvifPath,
                                             _camUser, _camPass);
            if (!ok)
            {
                logInfoP("ONVIF ch%d: subscribe failed, retry in 15s", _channelIndex);
                vTaskDelay(pdMS_TO_TICKS(15000));
                continue;
            }
            logInfoP("ONVIF ch%d: subscribed OK", _channelIndex);
            setOnline(true);
        }

        // Renew subscription before it expires
        int32_t remaining = _onvifClient.renewTimerMs();
        if (remaining >= 0 && remaining < (int32_t)ONVIF_RENEW_THRESHOLD_MS)
        {
            logInfoP("ONVIF ch%d: renewing subscription", _channelIndex);
            _onvifClient.renew();
        }

        // Blocking pull (up to 5 minutes)
        OnvifEventList evts;
        bool ok = _onvifClient.pullMessages(evts);
        if (!ok)
        {
            logInfoP("ONVIF ch%d: pullMessages failed, re-subscribing", _channelIndex);
            setOnline(false);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        for (uint8_t i = 0; i < evts.count; i++)
        {
            if (_eventQueue)
                xQueueSend(_eventQueue, &evts.events[i], pdMS_TO_TICKS(100));
        }
    }
}
#endif
