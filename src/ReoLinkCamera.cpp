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
            // Reolink: code=0 = success (set), code=1 = success with value (get)
            int rspCode = responseDoc[0]["code"] | -1;
            if (rspCode == 0 || rspCode == 1)
            {
                success = true;
            }
            else
            {
                // Token may have expired
                if (rspCode == -6 || rspCode == -7)
                    invalidateToken();
                logError("ReoLink", "%s: rspCode=%d", cmdName, rspCode);
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
    JsonDocument body;
    body[0]["cmd"]    = "GetPush";
    body[0]["action"] = 0;
    body[0]["param"]["channel"] = _nvrChannel;

    JsonDocument resp;
    if (!postCommand("GetPush", body, resp)) return false;

    enabledOut = (resp[0]["value"]["Push"]["schedule"]["enable"] | 0) != 0;
    return true;
}

bool ReoLinkCamera::setPush(bool enable)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetPush";
    body[0]["action"] = 0;
    body[0]["param"]["Push"]["schedule"]["enable"] = enable ? 1 : 0;
    body[0]["param"]["channel"] = _nvrChannel;
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
    JsonDocument body;
    body[0]["cmd"]    = "SetPrivacy";
    body[0]["action"] = 0;
    body[0]["param"]["channel"]  = _nvrChannel;
    body[0]["param"]["enable"]   = on ? 1 : 0;
    return postSetCommand("SetPrivacy", body);
}

bool ReoLinkCamera::setRecording(bool on)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetRec";
    body[0]["action"] = 0;
    body[0]["param"]["Rec"]["schedule"]["enable"] = on ? 1 : 0;
    body[0]["param"]["channel"] = _nvrChannel;
    return postSetCommand("SetRec", body);
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
    // mode: 0=Auto, 1=Day, 2=Night
    const char* modeStr = (mode == 1) ? "day" : (mode == 2) ? "night" : "auto";
    JsonDocument body;
    body[0]["cmd"]    = "SetImage";
    body[0]["action"] = 0;
    body[0]["param"]["Image"]["dayNight"] = modeStr;
    body[0]["param"]["channel"] = _nvrChannel;
    return postSetCommand("SetImage", body);
}

bool ReoLinkCamera::setMotionDetect(bool on)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetMd";
    body[0]["action"] = 0;
    body[0]["param"]["Md"]["schedule"]["enable"] = on ? 1 : 0;
    body[0]["param"]["channel"] = _nvrChannel;
    return postSetCommand("SetMd", body);
}

bool ReoLinkCamera::setAutoTracking(bool on)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetAiCfg";
    body[0]["action"] = 0;
    body[0]["param"]["AiCfg"]["track"]["enable"] = on ? 1 : 0;
    body[0]["param"]["channel"] = _nvrChannel;
    return postSetCommand("SetAiCfg", body);
}

bool ReoLinkCamera::setDoNotDisturb(bool on)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetDingDongCfg";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"]       = _nvrChannel;
    body[0]["param"]["DingDong"]["silence"]       = on ? 1 : 0;
    return postSetCommand("SetDingDongCfg", body);
}

bool ReoLinkCamera::setBellLedMode(uint8_t mode)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetDingDongCfg";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"]   = _nvrChannel;
    body[0]["param"]["DingDong"]["ledState"]  = mode;
    return postSetCommand("SetDingDongCfg", body);
}

bool ReoLinkCamera::setAutoReply(uint8_t index)
{
    JsonDocument body;
    body[0]["cmd"]    = "SetDingDongCfg";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"]      = _nvrChannel;
    body[0]["param"]["DingDong"]["replyFileId"]  = index;
    return postSetCommand("SetDingDongCfg", body);
}

bool ReoLinkCamera::setChimeMute(bool muted)
{
    JsonDocument body;
    body[0]["cmd"]    = "DingDongOpt";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"] = _nvrChannel;
    body[0]["param"]["DingDong"]["mute"]    = muted ? 1 : 0;
    return postSetCommand("DingDongOpt", body);
}

bool ReoLinkCamera::setChimeVolume(uint8_t volume)
{
    JsonDocument body;
    body[0]["cmd"]    = "DingDongOpt";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"] = _nvrChannel;
    body[0]["param"]["DingDong"]["volume"]  = volume;
    return postSetCommand("DingDongOpt", body);
}

bool ReoLinkCamera::setChimeRingtone(uint8_t ringtone)
{
    JsonDocument body;
    body[0]["cmd"]    = "DingDongOpt";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"]  = _nvrChannel;
    body[0]["param"]["DingDong"]["ringId"]   = ringtone;
    return postSetCommand("DingDongOpt", body);
}

bool ReoLinkCamera::triggerChime()
{
    JsonDocument body;
    body[0]["cmd"]    = "DingDongOpt";
    body[0]["action"] = 0;
    body[0]["param"]["DingDong"]["channel"] = _nvrChannel;
    body[0]["param"]["DingDong"]["play"]    = 1;
    return postSetCommand("DingDongOpt", body);
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

bool ReoLinkCamera::getWifiSignal(int8_t& rssiOut)
{
    JsonDocument body;
    body[0]["cmd"]    = "GetWifi";
    body[0]["action"] = 0;

    JsonDocument resp;
    if (!postCommand("GetWifi", body, resp)) return false;

    rssiOut = (int8_t)(resp[0]["value"]["Wifi"]["rssi"] | -100);
    return true;
}
