#pragma once
#include "BaseCameraChannel.h"
#include "ReoLinkCamera.h"

class ReoLinkChannel : public BaseCameraChannel
{
  public:
    explicit ReoLinkChannel(uint8_t channelIndex);

    void setup() override;

    bool login() override;
    bool pollEvents() override;

    // ONVIF event callback (ESP32 only)
    void onOnvifEvent(const char* topic, bool state) override;

    void setSiren(bool on) override;
    void setFloodlight(bool on) override;
    void setPrivacy(bool on) override;
    void setPush(bool on) override;
    void setRecording(bool on) override;
    void setPtzPreset(uint8_t preset) override;
    void setIrLeds(bool on) override;
    void setDayNightMode(uint8_t mode) override;
    void setMotionDetectActive(bool on) override;
    void setAutoTracking(bool on) override;
    void setManualRecord(bool on) override;
    void setDoNotDisturb(bool on) override;
    void setBellLedMode(uint8_t mode) override;
    void setAutoReply(uint8_t index) override;
    void setChimeMute(bool muted) override;
    void setChimeVolume(uint8_t volume) override;
    void setChimeRingtone(uint8_t ringtone) override;
    void triggerChime() override;

  private:
    ReoLinkCamera _camera;

    uint8_t _deviceType = IPC_DEVICE_CAMERA;   // camera / doorbell / nvr
    uint8_t _connType   = IPC_CONN_POE;         // PoE / WLAN
    bool    _battery    = false;
    bool    _hasChime   = false;

    // Token-Refresh-Timer
    uint32_t _lastTokenRefresh = 0;

    // Hilfsmethode: KO-Wert als uint8
    void setKoUint8(uint8_t koIndex, uint8_t value);

    // Hilfsmethode: alle AI-Erkennungs-KOs aus ReoLinkAiState setzen
    void applyAiState(const ReoLinkAiState& ai);

    // Hilfsmethode: kombiniertes AnyAlarm KO berechnen und setzen
    void updateAnyAlarm(const ReoLinkAiState& ai, bool motion);

    // Token erneuern wenn nötig
    void refreshTokenIfNeeded();
};
