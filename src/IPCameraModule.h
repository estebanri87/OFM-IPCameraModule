#pragma once
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
};

extern IPCameraModule openknxIPCameraModule;
