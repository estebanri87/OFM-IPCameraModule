#include "ChannelOwnerModule.h"

IPCChannelOwnerModule::IPCChannelOwnerModule(uint8_t numberOfChannels)
    : _numberOfChannels(numberOfChannels)
{
    if (_numberOfChannels > 0)
        _pChannels = new OpenKNX::Channel*[numberOfChannels]();
}

IPCChannelOwnerModule::~IPCChannelOwnerModule()
{
    if (_pChannels != nullptr)
    {
        delete[] _pChannels;
        _pChannels = nullptr;
    }
}

void IPCChannelOwnerModule::setup(bool configured)
{
    OpenKNX::Module::setup(configured);
}

OpenKNX::Channel* IPCChannelOwnerModule::createChannel(uint8_t _channelIndex)
{
    return nullptr;
}

void IPCChannelOwnerModule::setup()
{
    OpenKNX::Module::setup();
    if (_pChannels != nullptr)
    {
        logDebugP("Setting up %d channels", _numberOfChannels);
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
        {
            logDebugP("Create channel %d", _channelIndex);
            logIndentUp();
            _pChannels[_channelIndex] = createChannel(_channelIndex);
            logIndentDown();
        }
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
        {
            OpenKNX::Channel* channel = _pChannels[_channelIndex];
            if (channel != nullptr)
            {
                logDebugP("Init channel %d", _channelIndex);
                logIndentUp();
                channel->init();
                logIndentDown();

                logDebugP("Setup channel %d - setup(true)", _channelIndex);
                logIndentUp();
                channel->setup(true);
                logIndentDown();

                logInfoP("Setup channel %d - setup()", _channelIndex);
                logIndentUp();
                channel->setup();
                logIndentDown();
            }
        }
    }
}

void IPCChannelOwnerModule::loop(bool configured)
{
    OpenKNX::Module::loop(configured);
    if (_pChannels != nullptr)
    {
        uint8_t processed = 0;
        do
        {
            OpenKNX::Channel* channel = _pChannels[_currentChannel];
            if (channel != nullptr)
                channel->loop(configured);
        }
        while (openknx.freeLoopIterate(_numberOfChannels, _currentChannel, processed));
    }
}

void IPCChannelOwnerModule::loop()
{
    OpenKNX::Module::loop();
    if (_pChannels != nullptr)
    {
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
        {
            OpenKNX::Channel* channel = _pChannels[_channelIndex];
            if (channel != nullptr)
                channel->loop();
        }
    }
}

OpenKNX::Channel* IPCChannelOwnerModule::getChannel(uint8_t channelIndex)
{
    return _pChannels != nullptr ? _pChannels[channelIndex] : nullptr;
}

uint8_t IPCChannelOwnerModule::getNumberOfChannels()
{
    return _numberOfChannels;
}

uint8_t IPCChannelOwnerModule::getNumberOfUsedChannels()
{
    uint8_t activeChannels = 0;
    if (_pChannels != nullptr)
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
            if (_pChannels[_channelIndex] != nullptr)
                activeChannels++;
    return activeChannels;
}

#ifdef OPENKNX_DUALCORE
void IPCChannelOwnerModule::setup1(bool configured)
{
    OpenKNX::Module::setup1(configured);
}

void IPCChannelOwnerModule::setup1()
{
    OpenKNX::Module::setup1();
    if (_pChannels != nullptr)
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
            if (_pChannels[_channelIndex] != nullptr)
                _pChannels[_channelIndex]->setup1();
}

void IPCChannelOwnerModule::loop1(bool configured)
{
    OpenKNX::Module::loop1(configured);
    if (_pChannels != nullptr)
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
            if (_pChannels[_channelIndex] != nullptr)
                _pChannels[_channelIndex]->loop1(configured);
}

void IPCChannelOwnerModule::loop1()
{
    OpenKNX::Module::loop1();
    if (_pChannels != nullptr)
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
            if (_pChannels[_channelIndex] != nullptr)
                _pChannels[_channelIndex]->loop1();
}
#endif

#if (MASK_VERSION & 0x0900) != 0x0900
void IPCChannelOwnerModule::processInputKo(GroupObject &ko)
{
    OpenKNX::Module::processInputKo(ko);
    if (_pChannels != nullptr)
        for (uint8_t _channelIndex = 0; _channelIndex < _numberOfChannels; _channelIndex++)
            if (_pChannels[_channelIndex] != nullptr)
                _pChannels[_channelIndex]->processInputKo(ko);
}
#endif
