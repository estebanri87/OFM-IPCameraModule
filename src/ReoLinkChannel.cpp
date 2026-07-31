#include "ReoLinkChannel.h"
#include "knxprod.h"

ReoLinkChannel::ReoLinkChannel(uint8_t channelIndex)
    : BaseCameraChannel(channelIndex)
{
}

void ReoLinkChannel::setup()
{
    // Base setup (polling interval, hold time)
    BaseCameraChannel::setup();

    // IP, user, password from ETS parameters
    char ip[REOLINK_MAX_URL_LEN]   = {};
    char user[REOLINK_MAX_USER_LEN] = {};
    char pass[REOLINK_MAX_PASS_LEN] = {};

    // ParamIPC_CHIpAddress / Username / Password are char arrays in knxprod.h
    strncpy(ip,   (const char*)ParamIPC_CHIpAddress, sizeof(ip)   - 1);
    strncpy(user, (const char*)ParamIPC_CHUsername,  sizeof(user) - 1);
    strncpy(pass, (const char*)ParamIPC_CHPassword,  sizeof(pass) - 1);

    uint8_t nvrCh = 0;
    _deviceType   = ParamIPC_CHDeviceType;
    _connType     = ParamIPC_CHConnectionType;
    _battery      = (_connType == IPC_CONN_WLAN_BATTERY);
    _hasChime     = (ParamIPC_CHHasChime != 0);

    if (_deviceType == IPC_DEVICE_NVR)
        nvrCh = ParamIPC_CHNvrChannelIndex;

    uint16_t port = (uint16_t)ParamIPC_CHHttpPort;
    if (port == 0) port = 80;

    _camera.setCredentials(ip, port, user, pass, nvrCh);

    logDebugP("ReoLink channel %d: ip=%s:%u, device=%d, conn=%d, battery=%d, chime=%d",
              _channelIndex, ip, port, _deviceType, _connType, (int)_battery, (int)_hasChime);
}

bool ReoLinkChannel::login()
{
    bool ok = _camera.login();
    if (ok)
        _lastTokenRefresh = millis();
    return ok;
}

bool ReoLinkChannel::queryAbility(uint8_t& featureBits, uint8_t& aiBits)
{
    // Der Assistent läuft unabhängig vom Polling-Zustand, daher hier ein eigener Login
    if (!_camera.isLoggedIn() && !_camera.login())
        return false;

    return _camera.getAbility(featureBits, aiBits);
}

bool ReoLinkChannel::pollEvents()
{
    refreshTokenIfNeeded();

    // Motion state
    bool motion = false;
    if (!_camera.getMdState(motion))
        return false;

    if (motion)
    {
        setKoBool(IPC_KoMotion, true);
        _holdTimerMotion = millis();
    }

    // AI state
    ReoLinkAiState ai;
    if (!_camera.getAiState(ai))
        return false;

    applyAiState(ai);
    updateAnyAlarm(ai, motion);

    // Battery info (if applicable)
    if (_battery)
    {
        int8_t level = -1, status = -1;
        bool sleeping = false;
        if (_camera.getBatteryInfo(level, status, sleeping))
        {
            if (level >= 0)
            {
                GroupObject& ko = knx.getGroupObject(IPC_KoCalcNumber(IPC_KoBatteryLevel));
                ko.value((uint8_t)level, DPT_Scaling);
            }
            if (status >= 0)
                setKoUint8(IPC_KoBatteryStatus, (uint8_t)status);
            setKoBool(IPC_KoCameraSleeping, sleeping);
        }
    }

    // WiFi signal (if applicable)
    if (_connType == IPC_CONN_WLAN || _connType == IPC_CONN_WLAN_BATTERY)
    {
        int8_t signal = -1;
        if (_camera.getWifiSignal(signal) && signal >= 0)
        {
            // TODO: Wert ist 0..100, KO 35 ist in der ETS noch als DPT 9.021 (mA) angelegt
            GroupObject& ko = knx.getGroupObject(IPC_KoCalcNumber(IPC_KoWifiSignal));
            ko.value((int16_t)signal, DPT_Value_Electric_Current);
        }
    }

    return true;
}

void ReoLinkChannel::applyAiState(const ReoLinkAiState& ai)
{
    if (ai.person)  { setKoBool(IPC_KoPersonDetected,  true); _holdTimerPerson  = millis(); }
    if (ai.vehicle) { setKoBool(IPC_KoVehicleDetected, true); _holdTimerVehicle = millis(); }
    if (ai.animal)  { setKoBool(IPC_KoAnimalDetected,  true); _holdTimerAnimal  = millis(); }
    if (ai.pet)     { setKoBool(IPC_KoPetDetected,     true); _holdTimerPet     = millis(); }
    if (ai.package) { setKoBool(IPC_KoPackageDetected, true); _holdTimerPackage = millis(); }
    if (ai.face)    { setKoBool(IPC_KoFaceDetected,    true); _holdTimerFace    = millis(); }
    if (ai.baby)    { setKoBool(IPC_KoBabyAlarm,       true); _holdTimerBaby    = millis(); }
    if (ai.ioAlarm) { setKoBool(IPC_KoIOAlarm,         true); _holdTimerIO      = millis(); }
}

void ReoLinkChannel::updateAnyAlarm(const ReoLinkAiState& ai, bool motion)
{
    bool any = motion || ai.person || ai.vehicle || ai.animal || ai.pet ||
               ai.package || ai.face || ai.baby || ai.visitor || ai.ioAlarm;
    if (any)
    {
        // Snapshot nur bei der steigenden Flanke, nicht in jedem Poll-Zyklus
        bool wasIdle = (_holdTimerAnyAlarm == 0);
        setKoBool(IPC_KoAnyAlarm, true);
        _holdTimerAnyAlarm = millis();
        if (wasIdle)
            triggerSnapshot();
    }
}

void ReoLinkChannel::setKoUint8(uint8_t koIndex, uint8_t value)
{
    GroupObject& ko = knx.getGroupObject(IPC_KoCalcNumber(koIndex));
    ko.value(value, DPT_SceneNumber);
}

void ReoLinkChannel::refreshTokenIfNeeded()
{
    if (!_camera.isLoggedIn())
    {
        logDebugP("ReoLink ch%d: token expired, re-login", _channelIndex);
        _camera.login();
    }
}

void ReoLinkChannel::setSiren(bool on)         { _camera.setSiren(on); }
void ReoLinkChannel::setFloodlight(bool on)    { _camera.setFloodlight(on); }
void ReoLinkChannel::setPrivacy(bool on)       { _camera.setPrivacy(on); }
void ReoLinkChannel::setPush(bool on)          { _camera.setPush(on); }
void ReoLinkChannel::setRecording(bool on)     { _camera.setRecording(on); }
void ReoLinkChannel::setPtzPreset(uint8_t p)   { _camera.setPtzPreset(p); }
void ReoLinkChannel::setIrLeds(bool on)        { _camera.setIrLeds(on); }
void ReoLinkChannel::setDayNightMode(uint8_t m){ _camera.setDayNightMode(m); }
void ReoLinkChannel::setMotionSensitivity(uint8_t p) { _camera.setMotionSensitivity(p); }
void ReoLinkChannel::setAutoTracking(bool on)  { _camera.setAutoTracking(on); }
void ReoLinkChannel::setManualRecord(bool on)  { _camera.setRecording(on); }
void ReoLinkChannel::setDoNotDisturb(bool on)  { _camera.setDoNotDisturb(on); }
void ReoLinkChannel::setBellLedMode(bool on)   { _camera.setBellLedMode(on); }
void ReoLinkChannel::setAutoReply(uint8_t i)   { _camera.setAutoReply(i); }
void ReoLinkChannel::setChimeMute(bool m)      { _camera.setChimeMute(m); }
void ReoLinkChannel::setChimeVolume(uint8_t v) { _camera.setChimeVolume(v); }
void ReoLinkChannel::setChimeRingtone(uint8_t r){ _camera.setChimeRingtone(r); }
void ReoLinkChannel::triggerChime()            { _camera.triggerChime(); }

void ReoLinkChannel::sendInitialState()
{
    // Aktuellen Wert jedes Status-KOs einmal aktiv senden, damit im Bus nicht
    // erst bei der ersten Änderung ein Wert erscheint.
    static const uint8_t bools[] = {
        IPC_KoOnline, IPC_KoMotion, IPC_KoAnyAlarm, IPC_KoPersonDetected,
        IPC_KoVehicleDetected, IPC_KoAnimalDetected, IPC_KoPetDetected,
        IPC_KoPackageDetected, IPC_KoBabyAlarm, IPC_KoFaceDetected,
        IPC_KoIOAlarm, IPC_KoDoorbellTrigger, IPC_KoDoorbellHold,
        IPC_KoCameraSleeping, IPC_KoPushActive};

    for (uint8_t i = 0; i < sizeof(bools); i++)
    {
        GroupObject& ko = knx.getGroupObject(IPC_KoCalcNumber(bools[i]));
        sendKoBool(bools[i], (bool)ko.value(DPT_Switch));
    }
}

void ReoLinkChannel::onOnvifEvent(const char* topic, bool state)
{
    // Map Reolink ONVIF topics to KOs
    if (strstr(topic, "Visitor"))
    {
        if (state) { setKoBool(IPC_KoDoorbellTrigger, true); _holdTimerDoorbell = millis(); }
    }
    else if (strstr(topic, "Motion") || strstr(topic, "MotionAlarm"))
    {
        if (state) { setKoBool(IPC_KoMotion, true); _holdTimerMotion = millis(); }
    }
    else if (strstr(topic, "PeopleDetect") || strstr(topic, "FaceDetect"))
    {
        if (state) { setKoBool(IPC_KoPersonDetected, true); _holdTimerPerson = millis(); }
    }
    else if (strstr(topic, "VehicleDetect"))
    {
        if (state) { setKoBool(IPC_KoVehicleDetected, true); _holdTimerVehicle = millis(); }
    }
    else if (strstr(topic, "DogCatDetect"))
    {
        if (state) { setKoBool(IPC_KoAnimalDetected, true); _holdTimerAnimal = millis(); }
    }
    else if (strstr(topic, "Package"))
    {
        if (state) { setKoBool(IPC_KoPackageDetected, true); _holdTimerPackage = millis(); }
    }
    else if (strstr(topic, "Baby"))
    {
        if (state) { setKoBool(IPC_KoBabyAlarm, true); _holdTimerBaby = millis(); }
    }
    else if (strstr(topic, "IoAlarm") || strstr(topic, "IoDetect"))
    {
        if (state) { setKoBool(IPC_KoIOAlarm, true); _holdTimerIO = millis(); }
    }

    // AnyAlarm: fire on any true event
    if (state)
    {
        bool wasIdle = (_holdTimerAnyAlarm == 0);
        setKoBool(IPC_KoAnyAlarm, true);
        _holdTimerAnyAlarm = millis();
        if (wasIdle)
            triggerSnapshot();
    }
}
