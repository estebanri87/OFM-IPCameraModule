#include "DahuaChannel.h"
#include "knxprod.h"

DahuaChannel::DahuaChannel(uint8_t channelIndex)
    : BaseCameraChannel(channelIndex)
{
}

void DahuaChannel::setup()
{
    BaseCameraChannel::setup();
    logDebugP("Dahua channel %d: mode=%d, ip=%s", _channelIndex, _connectionMode, _camIp);
}

void DahuaChannel::onOnvifEvent(const char* topic, bool state)
{
    // Dahua VideoAnalytics ONVIF topics
    if (strstr(topic, "MotionDetection") || strstr(topic, "VideoMotion") || strstr(topic, "Motion"))
    {
        if (state) { setKoBool(IPC_KoMotion, true); _holdTimerMotion = millis(); }
    }
    else if (strstr(topic, "CrossLineDetection") || strstr(topic, "CrossRegionDetection"))
    {
        // Map line crossing to PersonDetected as closest approximation
        if (state) { setKoBool(IPC_KoPersonDetected, true); _holdTimerPerson = millis(); }
    }
    else if (strstr(topic, "PeopleDetect") || strstr(topic, "ObjectDetected"))
    {
        if (state) { setKoBool(IPC_KoPersonDetected, true); _holdTimerPerson = millis(); }
    }
    else if (strstr(topic, "VehicleDetect") || strstr(topic, "TrafficJam"))
    {
        if (state) { setKoBool(IPC_KoVehicleDetected, true); _holdTimerVehicle = millis(); }
    }
    else if (strstr(topic, "FaceDetect"))
    {
        if (state) { setKoBool(IPC_KoFaceDetected, true); _holdTimerFace = millis(); }
    }
    else if (strstr(topic, "TamperingDetection") || strstr(topic, "Tampering"))
    {
        if (state) { setKoBool(IPC_KoIOAlarm, true); _holdTimerIO = millis(); }
    }

    // AnyAlarm on any true event
    if (state)
    {
        setKoBool(IPC_KoAnyAlarm, true);
        _holdTimerAnyAlarm = millis();
        triggerSnapshot();
    }
}
