#include "HikvisionChannel.h"
#include "knxprod.h"

HikvisionChannel::HikvisionChannel(uint8_t channelIndex)
    : BaseCameraChannel(channelIndex)
{
}

void HikvisionChannel::setup()
{
    BaseCameraChannel::setup();
    logDebugP("Hikvision channel %d: mode=%d, ip=%s", _channelIndex, _connectionMode, _camIp);
}

void HikvisionChannel::onOnvifEvent(const char* topic, bool state)
{
    // Hikvision RuleEngine ONVIF topics
    if (strstr(topic, "CellMotionDetector") || strstr(topic, "Motion"))
    {
        if (state) { setKoBool(IPC_KoMotion, true); _holdTimerMotion = millis(); }
    }
    else if (strstr(topic, "PeopleDetect") || strstr(topic, "PeopleDetection") ||
             strstr(topic, "FieldDetector") || strstr(topic, "LineDetector"))
    {
        if (state) { setKoBool(IPC_KoPersonDetected, true); _holdTimerPerson = millis(); }
    }
    else if (strstr(topic, "VehicleDetect") || strstr(topic, "VehicleDetection"))
    {
        if (state) { setKoBool(IPC_KoVehicleDetected, true); _holdTimerVehicle = millis(); }
    }
    else if (strstr(topic, "FaceDetect") || strstr(topic, "FaceDetection"))
    {
        if (state) { setKoBool(IPC_KoFaceDetected, true); _holdTimerFace = millis(); }
    }
    else if (strstr(topic, "AudioException") || strstr(topic, "DefocusDetection") ||
             strstr(topic, "TamperingDetection"))
    {
        if (state) { setKoBool(IPC_KoIOAlarm, true); _holdTimerIO = millis(); }
    }
    else if (strstr(topic, "Doorbell") || strstr(topic, "CallButton"))
    {
        if (state) { setKoBool(IPC_KoDoorbellTrigger, true); _holdTimerDoorbell = millis(); }
    }

    // AnyAlarm on any true event
    if (state)
    {
        setKoBool(IPC_KoAnyAlarm, true);
        _holdTimerAnyAlarm = millis();
        triggerSnapshot();
    }
}
