#pragma once
#include "OpenKNX.h"
#include "HTTPClient.h"
#include <ArduinoJson.h>

// KO indices relative to channel base (IPC_KoCalcIndex)
#define IPC_KoOnline             0
#define IPC_KoMotion             1
#define IPC_KoAnyAlarm           2
#define IPC_KoPersonDetected     3
#define IPC_KoVehicleDetected    4
#define IPC_KoPrivacyMode        5
#define IPC_KoRecording          6
#define IPC_KoPushActive         7
#define IPC_KoSiren              8
#define IPC_KoFloodlight         9

// Reolink camera-specific KOs
#define IPC_KoAnimalDetected     10
#define IPC_KoPetDetected        11
#define IPC_KoPackageDetected    12
#define IPC_KoBabyAlarm          13
#define IPC_KoFaceDetected       14
#define IPC_KoIOAlarm            15
#define IPC_KoPtzPreset          16
#define IPC_KoMotionDetectActive 17
#define IPC_KoIrLeds             18
#define IPC_KoDayNightState      19
#define IPC_KoDayNightMode       20
#define IPC_KoAutoTracking       21
#define IPC_KoManualRecord       22

// Doorbell KOs
#define IPC_KoDoorbellTrigger    23
#define IPC_KoDoorbellHold       24
#define IPC_KoDoNotDisturb       25
#define IPC_KoBellLedMode        26
#define IPC_KoAutoReply          27

// Chime KOs
#define IPC_KoChimeMute          28
#define IPC_KoChimeVolume        29
#define IPC_KoChimeRingtone      30
#define IPC_KoChimeTrigger       31

// Battery KOs
#define IPC_KoBatteryLevel       32
#define IPC_KoBatteryStatus      33
#define IPC_KoCameraSleeping     34

// WLAN KO
#define IPC_KoWifiSignal         35

// Snapshot-Trigger KO
#define IPC_KoSnapshotTrigger    36

// Total KOs per channel
#define IPC_KoBlockSize          37

// Hold time defaults (ms)
#define IPC_HOLD_TIME_DEFAULT_MS  (30UL * 1000UL)
#define IPC_STARTUP_DELAY_MS      (15UL * 1000UL)
#define IPC_STARTUP_STAGGER_MS    (5UL  * 1000UL)  // zusätzliche Verzögerung pro Kanal-Index

// Gerätetyp-Werte (analog zu ETS-Enumeration)
#define IPC_DEVICE_CAMERA    0
#define IPC_DEVICE_DOORBELL  1
#define IPC_DEVICE_NVR       2

// Verbindungstyp
#define IPC_CONN_LAN            0  // LAN (PoE oder Netzteil)
#define IPC_CONN_WLAN           1  // WLAN mit Netzteil
#define IPC_CONN_WLAN_BATTERY   2  // WLAN mit Akku
// Rückwärtskompatibel
#define IPC_CONN_POE   IPC_CONN_LAN

// Verbindungsmodus (ONVIF)
#define IPC_MODE_JSON_POLL   0
#define IPC_MODE_ONVIF_ONLY  1
#define IPC_MODE_ONVIF_JSON  2

#ifdef ARDUINO_ARCH_ESP32
#include "OnvifClient.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#endif

class BaseCameraChannel : public OpenKNX::Channel
{
  protected:
    uint8_t _channelIndex;

    bool _online = false;
    bool _firstPoll = true;
    uint32_t _startupDelay = 0;
    uint32_t _lastPoll = 0;
    uint32_t _pollIntervalMs = 10000;

    // Verbindungsmodus (0=JSON, 1=ONVIF, 2=Combined)
    uint8_t _connectionMode = IPC_MODE_JSON_POLL;

    // Camera credential cache for ONVIF (filled in setup())
    char _camIp[80]   = {};
    char _camUser[32] = {};
    char _camPass[32] = {};

#ifdef ARDUINO_ARCH_ESP32
    OnvifClient  _onvifClient;
    QueueHandle_t _eventQueue    = nullptr;
    TaskHandle_t  _onvifTaskHandle = nullptr;
    uint16_t     _onvifPort = 80;
    char         _onvifPath[64] = {};

    // FreeRTOS task body (runs forever)
    void onvifTaskBody();

    // Static task entry point
    static void onvifTaskFunc(void* pvParam)
    {
        static_cast<BaseCameraChannel*>(pvParam)->onvifTaskBody();
        vTaskDelete(nullptr);
    }

    // Start the ONVIF long-poll task
    void startOnvifTask();
#endif

    // Hold timers per KO (motion, alarm types, doorbell)
    uint32_t _holdTimerMotion = 0;
    uint32_t _holdTimerAnyAlarm = 0;
    uint32_t _holdTimerPerson = 0;
    uint32_t _holdTimerVehicle = 0;
    uint32_t _holdTimerAnimal = 0;
    uint32_t _holdTimerPet = 0;
    uint32_t _holdTimerPackage = 0;
    uint32_t _holdTimerBaby = 0;
    uint32_t _holdTimerFace = 0;
    uint32_t _holdTimerIO = 0;
    uint32_t _holdTimerDoorbell = 0;

    uint32_t _holdTimeMs = IPC_HOLD_TIME_DEFAULT_MS;

    // Hilfsmethode: KO-Wert setzen (bool, DPT 1)
    void setKoBool(uint8_t koIndex, bool value);

    // Hold-Timer aktualisieren und KO bei Ablauf zurücksetzen
    void processHoldTimer(uint32_t& timer, uint8_t koIndex);

    // Alle Hold-Timer im loop() verarbeiten
    void processHoldTimers();

    // Setze Online-Status und sende KO 0
    void setOnline(bool online);

    // Sende SnapshotTrigger (KO 36) bei neuem Kamera-Ereignis
    void triggerSnapshot();

  public:
    explicit BaseCameraChannel(uint8_t channelIndex);
    virtual ~BaseCameraChannel() = default;

    const std::string name() override;
    void setup() override;
    void loop() override;
    void processInputKo(GroupObject& ko) override;

    // --- Abstrakte Schnittstelle (herstellerspezifisch) ---

    // Login / Verbindungsaufbau; gibt true zurück bei Erfolg
    virtual bool login() = 0;

    // Callback: ONVIF-Ereignis empfangen (ESP32 only)
    // topic = kurzer Topic-Name (z.B. "Motion", "Visitor", "MotionAlarm")
    virtual void onOnvifEvent(const char* topic, bool state) {}

    // Kamera-Ereignisse abfragen und KOs aktualisieren
    // Gibt true zurück wenn Verbindung erfolgreich
    virtual bool pollEvents() = 0;

    // Steuerung
    virtual void setSiren(bool on) = 0;
    virtual void setFloodlight(bool on) = 0;
    virtual void setPrivacy(bool on) = 0;
    virtual void setPush(bool on) = 0;
    virtual void setRecording(bool on) {}
    virtual void setPtzPreset(uint8_t preset) {}
    virtual void setIrLeds(bool on) {}
    virtual void setDayNightMode(uint8_t mode) {}
    virtual void setMotionDetectActive(bool on) {}
    virtual void setAutoTracking(bool on) {}
    virtual void setManualRecord(bool on) {}
    virtual void setDoNotDisturb(bool on) {}
    virtual void setBellLedMode(uint8_t mode) {}
    virtual void setAutoReply(uint8_t index) {}
    virtual void setChimeMute(bool muted) {}
    virtual void setChimeVolume(uint8_t volume) {}
    virtual void setChimeRingtone(uint8_t ringtone) {}
    virtual void triggerChime() {}
};
