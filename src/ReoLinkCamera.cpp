#include "ReoLinkCamera.h"
#include <stdio.h>
#include <string.h>

void ReoLinkCamera::setCredentials(const char* ip, uint16_t port, const char* user, const char* pass, uint8_t nvrChannel)
{
    strncpy(_ip,   ip,   sizeof(_ip)   - 1);
    strncpy(_user, user, sizeof(_user) - 1);
    strncpy(_pass, pass, sizeof(_pass) - 1);
    _port       = port;
    _nvrChannel = nvrChannel;
    invalidateToken();
}

bool ReoLinkCamera::isLoggedIn() const
{
    if (_token[0] == '\0') return false;
    // Token is valid for ~60 min; we refresh after IPC_TOKEN_REFRESH_MS
    return (millis() - _tokenTimestamp) < IPC_TOKEN_REFRESH_MS;
}

void ReoLinkCamera::invalidateToken()
{
    _token[0]       = '\0';
    _tokenTimestamp = 0;
}

void ReoLinkCamera::buildBaseUrl(char* buf, size_t bufLen) const
{
    snprintf(buf, bufLen, "http://%s:%u/api.cgi", _ip, _port);
}

bool ReoLinkCamera::login()
{
    JsonDocument body;
    body[0]["cmd"]    = "Login";
    body[0]["action"] = 0;
    body[0]["param"]["User"]["userName"] = _user;
    body[0]["param"]["User"]["password"] = _pass;

    JsonDocument resp;
    // Neuere Firmware: URL muss ?cmd=Login als Query-Parameter enthalten
    char baseUrl[128];
    buildBaseUrl(baseUrl, sizeof(baseUrl));
    char url[160];
    snprintf(url, sizeof(url), "%s?cmd=Login", baseUrl);

    std::string bodyStr;
    serializeJson(body, bodyStr);

    HTTPClient* http = new HTTPClient();
    if (!http)
    {
        logError("ReoLink", "login: HTTPClient alloc failed");
        return false;
    }

    http->begin(url);
    http->addHeader("Content-Type", "application/json");
    http->setConnectTimeout(REOLINK_HTTP_TIMEOUT_MS);
    http->setTimeout(REOLINK_HTTP_TIMEOUT_MS);

    int code = http->POST(bodyStr.c_str());
    bool success = false;

    if (code == 200)
    {
        String payload = http->getString();
        DeserializationError err = deserializeJson(resp, payload);
        if (!err)
        {
            const char* token = resp[0]["value"]["Token"]["name"] | "";
            if (token && token[0] != '\0')
            {
                strncpy(_token, token, sizeof(_token) - 1);
                _tokenTimestamp = millis();
                success = true;
                logDebug("ReoLink", "login OK, token=%s", _token);
            }
            else
            {
                logError("ReoLink", "login: no token in response");
            }
        }
        else
        {
            logError("ReoLink", "login: JSON parse error: %s", err.c_str());
        }
    }
    else
    {
        logError("ReoLink", "login: HTTP %d", code);
    }

    http->end();
    delete http;
    return success;
}

bool ReoLinkCamera::postCommand(const char* cmdName, JsonDocument& bodyDoc, JsonDocument& responseDoc)
{
    if (!isLoggedIn())
    {
        logError("ReoLink", "%s: not logged in", cmdName);
        return false;
    }

    char url[160];
    char baseUrl[128];
    buildBaseUrl(baseUrl, sizeof(baseUrl));
    snprintf(url, sizeof(url), "%s?token=%s", baseUrl, _token);

    std::string bodyStr;
    serializeJson(bodyDoc, bodyStr);

    HTTPClient* http = new HTTPClient();
    if (!http)
    {
        logError("ReoLink", "%s: HTTPClient alloc failed", cmdName);
        return false;
    }

    http->begin(url);
    http->addHeader("Content-Type", "application/json");
    http->setConnectTimeout(REOLINK_HTTP_TIMEOUT_MS);
    http->setTimeout(REOLINK_HTTP_TIMEOUT_MS);

    int code = http->POST(bodyStr.c_str());
    bool success = false;

    if (code == 200)
    {
        String payload = http->getString();
        DeserializationError err = deserializeJson(responseDoc, payload);
        if (!err)
        {
            // Reolink: code=0 = Erfolg, code=1 = Fehler (Detail in error.rspCode)
            int code = responseDoc[0]["code"] | -1;
            if (code == 0)
            {
                success = true;
            }
            else
            {
                int errCode = responseDoc[0]["error"]["rspCode"] | 0;
                // -6 = "please login first", -7 = Token ungültig
                if (errCode == -6 || errCode == -7)
                    invalidateToken();
                logError("ReoLink", "%s: code=%d rspCode=%d (%s)", cmdName, code, errCode,
                         (const char*)(responseDoc[0]["error"]["detail"] | "?"));
            }
        }
        else
        {
            logError("ReoLink", "%s: JSON parse error: %s", cmdName, err.c_str());
        }
    }
    else
    {
        logError("ReoLink", "%s: HTTP %d", cmdName, code);
    }

    http->end();
    delete http;
    return success;
}

bool ReoLinkCamera::postSetCommand(const char* cmdName, JsonDocument& bodyDoc)
{
    JsonDocument resp;
    return postCommand(cmdName, bodyDoc, resp);
}

bool ReoLinkCamera::getMdState(bool& motionOut)
{
    JsonDocument body;
    body[0]["cmd"]    = "GetMdState";
    body[0]["action"] = 0;
    body[0]["param"]["channel"] = _nvrChannel;

    JsonDocument resp;
    if (!postCommand("GetMdState", body, resp)) return false;

    motionOut = (resp[0]["value"]["state"] | 0) != 0;
    return true;
}

bool ReoLinkCamera::getAiState(ReoLinkAiState& aiOut)
{
    JsonDocument body;
    body[0]["cmd"]    = "GetAiState";
    body[0]["action"] = 0;
    body[0]["param"]["channel"] = _nvrChannel;

    JsonDocument resp;
    if (!postCommand("GetAiState", body, resp)) return false;

    auto val = resp[0]["value"];
    aiOut.person  = (val["people"]["alarm_state"]   | 0) != 0;
    aiOut.vehicle = (val["vehicle"]["alarm_state"]  | 0) != 0;
    aiOut.animal  = (val["animal"]["alarm_state"]   | 0) != 0;
    aiOut.pet     = (val["dog_cat"]["alarm_state"]  | 0) != 0;
    aiOut.package = (val["package"]["alarm_state"]  | 0) != 0;
    aiOut.face    = (val["face"]["alarm_state"]     | 0) != 0;
    aiOut.baby    = (val["cry"]["alarm_state"]      | 0) != 0;
    aiOut.visitor = (val["visitor"]["alarm_state"]  | 0) != 0;
    return true;
}

bool ReoLinkCamera::getPush(bool& enabledOut)
{
    // ACHTUNG: "param":{"channel":..} lässt das API-Backend der Kamera abstürzen
    // (HTTP 502, Neustart des CGI-Prozesses, alle Tokens werden verworfen).
    // Am Gerät verifiziert mit RLC-810A, FW v3.1.0.956.
    JsonDocument body;
    body[0]["cmd"]    = "GetPush";
    body[0]["action"] = 0;

    JsonDocument resp;
    if (!postCommand("GetPush", body, resp)) return false;

    enabledOut = (resp[0]["value"]["Push"]["schedule"]["enable"] | 0) != 0;
    return true;
}

bool ReoLinkCamera::setPush(bool enable)
{
    JsonDocument body;
    // channel gehört in Push.schedule — als Geschwister von "Push" stürzt die Kamera ab
    body[0]["cmd"]    = "SetPush";
    body[0]["action"] = 0;
    body[0]["param"]["Push"]["schedule"]["enable"]  = enable ? 1 : 0;
    body[0]["param"]["Push"]["schedule"]["channel"] = _nvrChannel;
    return postSetCommand("SetPush", body);
}

bool ReoLinkCamera::setSiren(bool on)
{
    JsonDocument body;
    body[0]["cmd"]    = "AudioAlarmPlay";
    body[0]["action"] = 0;
    body[0]["param"]["alarm_mode"]  = on ? "manul" : "off";
    body[0]["param"]["manual_switch"] = on ? 1 : 0;
    body[0]["param"]["channel"]     = _nvrChannel;
    return postSetCommand("AudioAlarmPlay", body);
}

bool ReoLinkCamera::setFloodlight(bool on)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetWhiteLed";
    body[0]["action"] = 0;
    body[0]["param"]["WhiteLed"]["state"] = on ? 1 : 0;
    body[0]["param"]["WhiteLed"]["channel"] = _nvrChannel;
    return postSetCommand("SetWhiteLed", body);
}

bool ReoLinkCamera::setPrivacy(bool on)
{
    // "SetPrivacy" kennt die Firmware nicht; die Privatzone ist die Mask-Funktion
    JsonDocument body;
    body[0]["cmd"]    = "SetMask";
    body[0]["action"] = 0;
    body[0]["param"]["Mask"]["channel"] = _nvrChannel;
    body[0]["param"]["Mask"]["enable"]  = on ? 1 : 0;
    return postSetCommand("SetMask", body);
}

bool ReoLinkCamera::setRecording(bool on)
{
    // "SetRec" liefert auf aktueller Firmware "not support" (-9), daher SetRecV20.
    // "scheduleEnable" wird zwar mit 200 quittiert, wirkt aber nicht — "enable" schon.
    // NVR-Kanäle sind hier nicht verifiziert (getestet nur an Einzelkameras).
    JsonDocument body;
    body[0]["cmd"]    = "SetRecV20";
    body[0]["action"] = 0;
    body[0]["param"]["Rec"]["enable"] = on ? 1 : 0;
    return postSetCommand("SetRecV20", body);
}

bool ReoLinkCamera::setPtzPreset(uint8_t preset)
{
    JsonDocument body;
    body[0]["cmd"]    = "PtzCtrl";
    body[0]["action"] = 0;
    body[0]["param"]["channel"]  = _nvrChannel;
    body[0]["param"]["op"]       = "ToPos";
    body[0]["param"]["id"]       = preset;
    body[0]["param"]["speed"]    = 32;
    return postSetCommand("PtzCtrl", body);
}

bool ReoLinkCamera::setIrLeds(bool on)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetIrLights";
    body[0]["action"] = 0;
    body[0]["param"]["IrLights"]["state"] = on ? "Auto" : "Off";
    body[0]["param"]["channel"] = _nvrChannel;
    return postSetCommand("SetIrLights", body);
}

bool ReoLinkCamera::setDayNightMode(uint8_t mode)
{
    // mode: 0=Auto, 1=Tag (Farbe), 2=Nacht (S/W)
    // Zulässige Werte laut GetIsp-Range: ["Auto","Color","Black&White"]
    const char* modeStr = (mode == 1) ? "Color" : (mode == 2) ? "Black&White" : "Auto";

    // Bewusst nur die geänderten Felder senden: das vollständige Isp-Objekt
    // zurückzuschreiben quittiert die Kamera mit rspCode -6 (Body zu groß).
    JsonDocument body;
    body[0]["cmd"]    = "SetIsp";
    body[0]["action"] = 0;
    body[0]["param"]["Isp"]["channel"]  = _nvrChannel;
    body[0]["param"]["Isp"]["dayNight"] = modeStr;
    return postSetCommand("SetIsp", body);
}

bool ReoLinkCamera::setMotionSensitivity(uint8_t percent)
{
    // "SetMd" und "SetAlarm" melden auf aktueller Firmware "not support" (-9);
    // ein Ein/Aus für die Bewegungserkennung gibt es nicht. Stellbar ist nur
    // die Empfindlichkeit über sensDef (1 = maximal, 50 = minimal).
    if (percent > 100) percent = 100;
    uint8_t sensDef = (uint8_t)(50 - ((uint16_t)percent * 49 + 50) / 100);
    if (sensDef < 1)  sensDef = 1;
    if (sensDef > 50) sensDef = 50;

    JsonDocument body;
    body[0]["cmd"]    = "SetMdAlarm";
    body[0]["action"] = 0;
    body[0]["param"]["MdAlarm"]["channel"]              = _nvrChannel;
    body[0]["param"]["MdAlarm"]["useNewSens"]           = 1;
    body[0]["param"]["MdAlarm"]["newSens"]["sensDef"]   = sensDef;
    return postSetCommand("SetMdAlarm", body);
}

bool ReoLinkCamera::setAutoTracking(bool on)
{
    // param wird NICHT in ein "AiCfg"-Objekt gewickelt (vgl. reolink_aio set_auto_tracking)
    JsonDocument body;
    body[0]["cmd"]    = "SetAiCfg";
    body[0]["action"] = 0;
    body[0]["param"]["channel"] = _nvrChannel;
    body[0]["param"]["aiTrack"] = on ? 1 : 0;
    return postSetCommand("SetAiCfg", body);
}

bool ReoLinkCamera::setDoNotDisturb(bool on)
{
    // "Nicht stören" (Silent-Time) läuft bei Reolink ausschließlich über das
    // Baichuan-Binärprotokoll (Port 9000), nicht über die CGI-API.
    // SetDingDongCfg quittiert hier zwar mit rspCode 200, bewirkt aber nichts.
    logError("ReoLink", "setDoNotDisturb: über die HTTP-API nicht unterstützt");
    return false;
}

bool ReoLinkCamera::setBellLedMode(bool on)
{
    // GetPowerLed liefert PowerLed.eDoorbellLightState ("On"/"Off"); die API kennt
    // nur diese beiden Zustände, daher ist das KO ein Schalter und kein Modus-Byte.
    JsonDocument body;
    body[0]["cmd"]    = "SetPowerLed";
    body[0]["action"] = 0;
    body[0]["param"]["PowerLed"]["channel"]              = _nvrChannel;
    body[0]["param"]["PowerLed"]["eDoorbellLightState"]  = on ? "On" : "Off";
    body[0]["param"]["PowerLed"]["state"]                = "On";
    return postSetCommand("SetPowerLed", body);
}

bool ReoLinkCamera::setAutoReply(uint8_t index)
{
    // fileId entspricht der id aus GetAudioFileList, -1 = aus
    JsonDocument body;
    body[0]["cmd"]    = "SetAutoReply";
    body[0]["action"] = 0;
    body[0]["param"]["AutoReply"]["enable"]  = 1;
    body[0]["param"]["AutoReply"]["fileId"]  = (int)index;
    body[0]["param"]["AutoReply"]["timeout"] = 15;
    return postSetCommand("SetAutoReply", body);
}

bool ReoLinkCamera::getChimeInfo()
{
    // 1) Gekoppelten Chime ermitteln
    JsonDocument listBody;
    listBody[0]["cmd"]    = "GetDingDongList";
    listBody[0]["action"] = 0;

    JsonDocument listResp;
    if (!postCommand("GetDingDongList", listBody, listResp)) return false;

    JsonArray paired = listResp[0]["value"]["DingDongList"]["pairedlist"].as<JsonArray>();
    _chimeId = 0;
    for (JsonObject dev : paired)
    {
        int32_t devId = dev["deviceId"] | 0;
        if (devId > 0)
        {
            _chimeId = devId;
            strncpy(_chimeName, dev["deviceName"] | "Reolink Chime", sizeof(_chimeName) - 1);
            break;
        }
    }
    if (_chimeId == 0)
    {
        logInfo("ReoLink", "getChimeInfo: kein Chime gekoppelt");
        return false;
    }

    // 2) Lautstärke und LED-Zustand lesen (option 2)
    JsonDocument optBody;
    optBody[0]["cmd"]    = "DingDongOpt";
    optBody[0]["action"] = 0;
    optBody[0]["param"]["DingDong"]["channel"] = _nvrChannel;
    optBody[0]["param"]["DingDong"]["option"]  = REOLINK_DINGDONG_GET;
    optBody[0]["param"]["DingDong"]["id"]      = _chimeId;

    JsonDocument optResp;
    if (!postCommand("DingDongOpt", optBody, optResp)) return false;

    auto val = optResp[0]["value"]["DingDong"];
    _chimeVolume = (uint8_t)(val["volLevel"] | 4);
    _chimeLed    = (val["ledState"] | 1) != 0;
    const char* name = val["name"] | "";
    if (name[0] != '\0')
        strncpy(_chimeName, name, sizeof(_chimeName) - 1);

    logDebug("ReoLink", "Chime id=%ld vol=%d led=%d", (long)_chimeId, _chimeVolume, (int)_chimeLed);
    return true;
}

bool ReoLinkCamera::sendChimeOption(uint8_t volume, bool led)
{
    if (_chimeId == 0 && !getChimeInfo()) return false;

    JsonDocument body;
    body[0]["cmd"]    = "DingDongOpt";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"]  = _nvrChannel;
    body[0]["param"]["DingDong"]["option"]   = REOLINK_DINGDONG_SET;
    body[0]["param"]["DingDong"]["id"]       = _chimeId;
    body[0]["param"]["DingDong"]["name"]     = _chimeName;
    body[0]["param"]["DingDong"]["volLevel"] = volume;
    body[0]["param"]["DingDong"]["ledState"] = led ? 1 : 0;
    return postSetCommand("DingDongOpt", body);
}

bool ReoLinkCamera::setChimeMute(bool muted)
{
    if (_chimeId == 0 && !getChimeInfo()) return false;

    // Stummschalten = Lautstärke 0; beim Aufheben die letzte Lautstärke zurückholen
    uint8_t volume = muted ? 0 : (_chimeVolume > 0 ? _chimeVolume : 4);
    return sendChimeOption(volume, _chimeLed);
}

bool ReoLinkCamera::setChimeVolume(uint8_t volume)
{
    if (volume > 4) volume = 4;
    if (!sendChimeOption(volume, _chimeLed)) return false;
    _chimeVolume = volume;
    return true;
}

bool ReoLinkCamera::setChimeRingtone(uint8_t ringtone)
{
    // Der Klingelton je Ereignistyp ist nur über Baichuan setzbar. Hier wird der
    // Ton gemerkt und bei triggerChime() verwendet.
    if (ringtone > 9) ringtone = 9;
    _chimeTone = ringtone;
    return true;
}

bool ReoLinkCamera::triggerChime()
{
    if (_chimeId == 0 && !getChimeInfo()) return false;

    JsonDocument body;
    body[0]["cmd"]    = "DingDongOpt";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"] = _nvrChannel;
    body[0]["param"]["DingDong"]["option"]  = REOLINK_DINGDONG_PLAY;
    body[0]["param"]["DingDong"]["id"]      = _chimeId;
    body[0]["param"]["DingDong"]["musicId"] = _chimeTone;
    return postSetCommand("DingDongOpt", body);
}

bool ReoLinkCamera::getAbility(uint8_t& featureBits, uint8_t& aiBits)
{
    featureBits = 0;
    aiBits      = 0;

    JsonDocument body;
    body[0]["cmd"]    = "GetAbility";
    body[0]["action"] = 0;
    body[0]["param"]["User"]["userName"] = _user;

    JsonDocument resp;
    if (!postCommand("GetAbility", body, resp)) return false;

    JsonObject chn = resp[0]["value"]["Ability"]["abilityChn"][_nvrChannel];
    if (chn.isNull())
    {
        logError("ReoLink", "getAbility: abilityChn[%d] fehlt", _nvrChannel);
        return false;
    }

    // permit > 0 bedeutet "vorhanden"; 0 heißt, das Gerät kann es nicht
    auto permit = [&](const char* key) -> bool {
        return (chn[key]["permit"] | 0) > 0;
    };

    if (permit("floodLight") || permit("supportFLswitch")) featureBits |= (1 << 0);
    if (permit("alarmAudio"))                              featureBits |= (1 << 1);
    if (permit("ptzPreset"))                               featureBits |= (1 << 2);
    if (permit("ledControl"))                              featureBits |= (1 << 3);
    if (permit("mask"))                                    featureBits |= (1 << 4);
    if (permit("recCfg"))                                  featureBits |= (1 << 5);
    if (permit("aiTrack"))                                 featureBits |= (1 << 6);
    if (permit("alarmIoIn"))                               featureBits |= (1 << 7);

    // Für die KI-Typen ist GetAiState maßgeblich: nur was dort "support":1 meldet,
    // taucht später im Polling überhaupt auf.
    JsonDocument aiBody;
    aiBody[0]["cmd"]    = "GetAiState";
    aiBody[0]["action"] = 0;
    aiBody[0]["param"]["channel"] = _nvrChannel;

    JsonDocument aiResp;
    if (!postCommand("GetAiState", aiBody, aiResp)) return false;

    auto val = aiResp[0]["value"];
    if ((val["people"]["support"]  | 0) != 0) aiBits |= (1 << 0);
    if ((val["vehicle"]["support"] | 0) != 0) aiBits |= (1 << 1);
    if ((val["dog_cat"]["support"] | 0) != 0) aiBits |= (1 << 2);
    if ((val["package"]["support"] | 0) != 0) aiBits |= (1 << 3);
    if ((val["face"]["support"]    | 0) != 0) aiBits |= (1 << 4);

    // Bit 5: klassische Bewegungserkennung (GetMdState), kommt aus GetAbility
    if ((chn["alarmMd"]["permit"] | 0) > 0 || (chn["supportMd"]["permit"] | 0) > 0)
        aiBits |= (1 << 5);

    // Bit 6/7: Push liegt auf Geräteebene, Tag/Nacht im Kanal
    if ((resp[0]["value"]["Ability"]["push"]["permit"] | 0) > 0) aiBits |= (1 << 6);
    if ((chn["ispDayNight"]["permit"] | 0) > 0)                  aiBits |= (1 << 7);

    logInfo("ReoLink", "getAbility: features=0x%02X ai=0x%02X", featureBits, aiBits);
    return true;
}

bool ReoLinkCamera::getBatteryInfo(int8_t& levelOut, int8_t& statusOut, bool& sleepingOut)
{
    JsonDocument body;
    body[0]["cmd"]    = "GetBatteryInfo";
    body[0]["action"] = 0;
    body[0]["param"]["channel"] = _nvrChannel;

    JsonDocument resp;
    if (!postCommand("GetBatteryInfo", body, resp)) return false;

    auto val = resp[0]["value"]["Battery"];
    levelOut   = (int8_t)(val["batteryPercent"] | -1);
    statusOut  = (int8_t)(val["chargeStatus"]   | -1);
    sleepingOut = (val["lowPower"] | 0) != 0;
    return true;
}

bool ReoLinkCamera::getWifiSignal(int8_t& signalOut)
{
    // GetWifi liefert die WLAN-Konfiguration (SSID/Passwort) und auf LAN-Modellen
    // "ability error". Die Signalstärke kommt aus GetWifiSignal als 0..100, nicht in dBm.
    JsonDocument body;
    body[0]["cmd"]    = "GetWifiSignal";
    body[0]["action"] = 0;

    JsonDocument resp;
    if (!postCommand("GetWifiSignal", body, resp)) return false;

    signalOut = (int8_t)(resp[0]["value"]["wifiSignal"] | -1);
    return true;
}
