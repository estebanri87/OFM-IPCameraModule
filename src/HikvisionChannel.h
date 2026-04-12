#pragma once
#include "BaseCameraChannel.h"

/**
 * HikvisionChannel — Hikvision IP camera via ONVIF Long Polling.
 *
 * Supported ONVIF topics (RuleEngine events):
 *   CellMotionDetector/Motion          → IPC_KoMotion
 *   FieldDetector/ObjectDetected       → IPC_KoPersonDetected
 *   LineDetector/ObjectCrossLine       → IPC_KoPersonDetected
 *   PeopleDetection/PeopleDetected     → IPC_KoPersonDetected
 *   VehicleDetection/ObjectDetected    → IPC_KoVehicleDetected
 *   FaceDetection/FaceDetected         → IPC_KoFaceDetected
 *   AudioException / DefocusDetection  → IPC_KoIOAlarm
 *
 * pollEvents() is a no-op for pure ONVIF mode.
 * JSON polling mode is NOT supported for Hikvision.
 */
class HikvisionChannel : public BaseCameraChannel
{
  public:
    explicit HikvisionChannel(uint8_t channelIndex);

    void setup() override;

    // Not used in ONVIF mode — returns true to keep channel marked online
    bool login()       override { return true; }
    bool pollEvents()  override { return true; }

    void onOnvifEvent(const char* topic, bool state) override;

    // Stub control methods
    void setSiren(bool)         override {}
    void setFloodlight(bool)    override {}
    void setPrivacy(bool)       override {}
    void setPush(bool)          override {}
};
