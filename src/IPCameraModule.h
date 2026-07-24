#pragma once
// Function-Property-Kennung für den ETS-Assistenten.
// 0xA0 nutzt das PresenceModule, 0xA1 das SolarmanPV-Modul.
#define IPC_FUNCTION_OBJECT   0xA2
#define IPC_FUNCTION_PROPERTY 1
#define IPC_CMD_QUERY         1
#define IPC_CMD_STATUS        2

#include "OpenKNX.h"
#include "ChannelOwnerModule.h"

class IPCameraModule : public IPCChannelOwnerModule
{
  public:
    IPCameraModule();
    const std::string name() override;
    const std::string version() override;
    void showInformations() override;
    OpenKNX::Channel* createChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */) override;
    void showHelp() override;
    bool processCommand(const std::string cmd, bool diagnoseKo) override;

    void loop(bool configured) override;
    bool processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length,
                                 uint8_t* data, uint8_t* resultData, uint8_t& resultLength) override;

  private:
    // Zustand des ETS-Assistenten. Die Abfrage laeuft im loop(), weil Login und
    // GetAbility laenger dauern, als eine Function Property antworten darf.
    enum AbilityState : uint8_t
    {
        AbilityIdle = 0,
        AbilityPending,   // angefordert, noch nicht ausgefuehrt
        AbilityReady      // Ergebnis liegt vor
    };

    AbilityState _abilityState   = AbilityIdle;
    uint8_t      _abilityChannel = 0;
    uint8_t      _abilityError   = 0;  // 0 = OK, 1 = Kanal ungueltig, 2 = Kamera nicht erreichbar
    uint8_t      _abilityFeature = 0;
    uint8_t      _abilityAi      = 0;
};

extern IPCameraModule openknxIPCameraModule;
