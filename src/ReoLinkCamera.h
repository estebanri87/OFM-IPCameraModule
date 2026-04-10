#pragma once
#include "OpenKNX.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>

#define REOLINK_HTTP_TIMEOUT_MS 8000
#define REOLINK_MAX_URL_LEN     80
#define REOLINK_MAX_TOKEN_LEN   64
#define REOLINK_MAX_USER_LEN    32
#define REOLINK_MAX_PASS_LEN    32

struct ReoLinkAiState
{
    bool motion   = false;
    bool person   = false;
    bool vehicle  = false;
    bool animal   = false;
    bool pet      = false;
    bool package  = false;
    bool face     = false;
    bool baby     = false;
    bool visitor  = false;
    bool ioAlarm  = false;
};

struct ReoLinkDeviceState
{
    ReoLinkAiState ai;
    bool online       = false;
    bool pushEnabled  = false;
    bool recording    = false;
    bool privacy      = false;
    bool floodlight   = false;
    int8_t dayNight   = -1;   // -1=unknown, 0=day, 1=night
    int8_t batteryLevel = -1; // -1=unknown, 0-100
    int8_t batteryStatus = -1;
    bool sleeping     = false;
    int8_t wifiSignal = -100; // dBm
};

class ReoLinkCamera
{
  public:
    ReoLinkCamera() = default;

    void setCredentials(const char* ip, uint16_t port, const char* user, const char* pass, uint8_t nvrChannel = 0);

    // Login and fetch token; returns true on success
    bool login();

    // Returns true if token is valid (not expired)
    bool isLoggedIn() const;

    // Marks token as expired (forces re-login on next call)
    void invalidateToken();

    // GET motion detection state (GetMdState)
    bool getMdState(bool& motionOut);

    // GET AI detection state (GetAiState)
    bool getAiState(ReoLinkAiState& aiOut);

    // SET push notifications
    bool setPush(bool enable);

    // GET push notifications state
    bool getPush(bool& enabledOut);

    // SET siren
    bool setSiren(bool on);

    // SET floodlight (WhiteLed)
    bool setFloodlight(bool on);

    // SET privacy mode
    bool setPrivacy(bool on);

    // SET recording
    bool setRecording(bool on);

    // SET PTZ preset
    bool setPtzPreset(uint8_t preset);

    // SET IR LEDs
    bool setIrLeds(bool on);

    // SET day/night mode (0=auto, 1=day, 2=night)
    bool setDayNightMode(uint8_t mode);

    // SET motion detection active
    bool setMotionDetect(bool on);

    // SET auto tracking
    bool setAutoTracking(bool on);

    // Doorbell: DingDong options
    bool setDoNotDisturb(bool on);
    bool setBellLedMode(uint8_t mode);
    bool setAutoReply(uint8_t index);

    // Chime options (DingDongOpt)
    bool setChimeMute(bool muted);
    bool setChimeVolume(uint8_t volume);
    bool setChimeRingtone(uint8_t ringtone);
    bool triggerChime();

    // Battery and WiFi info
    bool getBatteryInfo(int8_t& levelOut, int8_t& statusOut, bool& sleepingOut);
    bool getWifiSignal(int8_t& rssiOut);

  private:
    char _ip[REOLINK_MAX_URL_LEN] = {};
    uint16_t _port = 80;
    char _user[REOLINK_MAX_USER_LEN] = {};
    char _pass[REOLINK_MAX_PASS_LEN] = {};
    uint8_t _nvrChannel = 0;

    char _token[REOLINK_MAX_TOKEN_LEN] = {};
    uint32_t _tokenTimestamp = 0;

    // Build base URL into buffer
    void buildBaseUrl(char* buf, size_t bufLen) const;

    // POST a JSON command array to /api.cgi; returns true and fills responseDoc on success
    bool postCommand(const char* cmdName, JsonDocument& bodyDoc, JsonDocument& responseDoc);

    // Simple SET command helper (no response parsing needed)
    bool postSetCommand(const char* cmdName, JsonDocument& bodyDoc);
};
