#include "IPCameraModule.h"
#include "BaseCameraChannel.h"
#include "ReoLinkChannel.h"
#include "DahuaChannel.h"
#include "HikvisionChannel.h"
#include "knxprod.h"

IPCameraModule openknxIPCameraModule;

IPCameraModule::IPCameraModule()
    : IPCChannelOwnerModule(IPC_ChannelCount)
{
}

const std::string IPCameraModule::name()
{
    return "IPCamera";
}

const std::string IPCameraModule::version()
{
#ifdef MODULE_IPCameraModule_Version
    return MODULE_IPCameraModule_Version;
#else
    return "";
#endif
}

void IPCameraModule::showInformations()
{
}

OpenKNX::Channel* IPCameraModule::createChannel(uint8_t _channelIndex /* this parameter is used in macros, do not rename */)
{
    // Hersteller-Parameter: 0=Reolink, 1=Hikvision, 2=Dahua
    switch (ParamIPC_CHManufacturer)
    {
        case 0:
            return new ReoLinkChannel(_channelIndex);
        case 1:
            return new HikvisionChannel(_channelIndex);
        case 2:
            return new DahuaChannel(_channelIndex);
        default:
            logInfoP("IPC channel %d: unsupported manufacturer %d, disabled", _channelIndex, (int)ParamIPC_CHManufacturer);
            return nullptr;
    }
}

void IPCameraModule::loop(bool configured)
{
    IPCChannelOwnerModule::loop(configured);

    if (!configured || _abilityState != AbilityPending)
        return;

    // Ab hier genau einmal ausfuehren, egal wie es ausgeht
    _abilityState   = AbilityReady;
    _abilityFeature = 0;
    _abilityAi      = 0;

    auto* ch = static_cast<BaseCameraChannel*>(getChannel(_abilityChannel));
    if (!ch)
    {
        _abilityError = 1;
        logInfoP("Assistent: Kanal %d ist nicht aktiv", _abilityChannel + 1);
        return;
    }

    if (ch->queryAbility(_abilityFeature, _abilityAi))
    {
        _abilityError = 0;
        logInfoP("Assistent: Kanal %d gelesen, features=0x%02X ai=0x%02X",
                 _abilityChannel + 1, _abilityFeature, _abilityAi);
    }
    else
    {
        _abilityError = 2;
        logInfoP("Assistent: Kanal %d, Kamera nicht erreichbar", _abilityChannel + 1);
    }
}

// Antwortaufbau wie bei OFM-SolarmanPV: [0] = angenommen, [1] = fertig
bool IPCameraModule::processFunctionProperty(uint8_t objectIndex, uint8_t propertyId, uint8_t length,
                                             uint8_t* data, uint8_t* resultData, uint8_t& resultLength)
{
    if (objectIndex != IPC_FUNCTION_OBJECT || propertyId != IPC_FUNCTION_PROPERTY || length < 2)
        return false;

    // Das Skript zaehlt Kanaele ab 1, intern wird ab 0 gezaehlt
    const uint8_t channel = data[1] > 0 ? data[1] - 1 : 0;

    switch (data[0])
    {
        case IPC_CMD_QUERY:
            if (channel >= getNumberOfChannels())
            {
                resultData[0] = 1;
                resultLength = 1;
                return true;
            }
            _abilityChannel = channel;
            _abilityState   = AbilityPending;
            resultData[0] = 0;
            resultData[1] = 0;
            resultLength = 2;
            return true;

        case IPC_CMD_STATUS:
            resultData[0] = 0;
            if (_abilityState != AbilityReady)
            {
                resultData[1] = 0;   // laeuft noch
                resultLength = 2;
                return true;
            }
            resultData[1] = 1;
            resultData[2] = _abilityError;
            resultData[3] = _abilityFeature;
            resultData[4] = _abilityAi;
            resultLength = 5;
            _abilityState = AbilityIdle;
            return true;

        default:
            return false;
    }
}

void IPCameraModule::showHelp()
{
    openknx.console.printHelpLine("ipc<CC> status", "Show status of camera channel CC. e.g. ipc01");
    openknx.console.printHelpLine("ipc<CC> poll",   "Force poll of camera channel CC. e.g. ipc01");
    openknx.console.printHelpLine("ipc<CC> login",  "Force re-login of camera channel CC.");
}

bool IPCameraModule::processCommand(const std::string cmd, bool diagnoseKo)
{
    if (cmd.rfind("ipc", 0) != 0)
        return false;

    auto rest = cmd.substr(3);
    if (rest.empty())
        return false;

    auto spacePos = rest.find(' ');
    std::string channelStr = (spacePos != std::string::npos) ? rest.substr(0, spacePos) : rest;
    std::string subCmd     = (spacePos != std::string::npos) ? rest.substr(spacePos + 1) : "";

    int chNum = atoi(channelStr.c_str());
    if (chNum < 1 || chNum > (int)IPC_ChannelCount)
    {
        logInfo("IPC", "invalid channel number");
        return true;
    }

    auto* ch = static_cast<BaseCameraChannel*>(getChannel(chNum - 1));
    if (!ch)
    {
        logInfo("IPC", "channel not active");
        return true;
    }

    if (subCmd == "login")
    {
        ch->login();
        logInfo("IPC", "login triggered");
    }
    else if (subCmd == "poll")
    {
        ch->pollEvents();
        logInfo("IPC", "poll triggered");
    }
    else
    {
        openknx.console.printHelpLine("ipc<CC> status", "");
        openknx.console.printHelpLine("ipc<CC> poll",   "");
        openknx.console.printHelpLine("ipc<CC> login",  "");
    }

    return true;
}
