#include "IPCameraModule.h"
#include "ReoLinkChannel.h"
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
    // Hersteller-Parameter: 0=Reolink, 1=Hikvision (future), 2=Dahua (future)
    switch (ParamIPC_CHManufacturer)
    {
        case 0:
            return new ReoLinkChannel(_channelIndex);
        default:
            logInfoP("IPC channel %d: unsupported manufacturer %d, disabled", _channelIndex, (int)ParamIPC_CHManufacturer);
            return nullptr;
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
        openknx.console.println("IPC: invalid channel number");
        return true;
    }

    auto* ch = static_cast<BaseCameraChannel*>(getChannel(chNum - 1));
    if (!ch)
    {
        openknx.console.println("IPC: channel not active");
        return true;
    }

    if (subCmd == "login")
    {
        ch->login();
        openknx.console.println("IPC: login triggered");
    }
    else if (subCmd == "poll")
    {
        ch->pollEvents();
        openknx.console.println("IPC: poll triggered");
    }
    else
    {
        openknx.console.printHelpLine("ipc<CC> status", "");
        openknx.console.printHelpLine("ipc<CC> poll",   "");
        openknx.console.printHelpLine("ipc<CC> login",  "");
    }

    return true;
}
