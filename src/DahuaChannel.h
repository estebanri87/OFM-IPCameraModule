#pragma once
#include "BaseCameraChannel.h"

/**
 * DahuaChannel — Dahua IP camera via ONVIF Long Polling.
 *
 * Supported ONVIF topics (VideoAnalytics events):
 *   VideoMotion / MotionDetection  → IPC_KoMotion
 *   CrossLineDetection             → IPC_KoPersonDetected  (best approx.)
 *   ObjectDetected / PeopleDetect  → IPC_KoPersonDetected
 *   TamperingDetection             → IPC_KoAnyAlarm
 *
 * pollEvents() is a no-op for pure ONVIF mode.
 * JSON polling mode is NOT supported for Dahua (use mode 1 or 2 with mode=1).
 */
class DahuaChannel : public BaseCameraChannel
{
  public:
    explicit DahuaChannel(uint8_t channelIndex);

    void setup() override;

    // Not used in ONVIF mode — returns true to keep channel marked online
    bool login()       override { return true; }
    bool pollEvents()  override { return true; }

    void onOnvifEvent(const char* topic, bool state) override;

    // Stub control methods — Dahua control via ONVIF not implemented
    void setSiren(bool)         override {}
    void setFloodlight(bool)    override {}
    void setPrivacy(bool)       override {}
    void setPush(bool)          override {}
};
