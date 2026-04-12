#pragma once

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <HTTPClient.h>

// Subscription lifetime: 15 minutes
#define ONVIF_SUBSCRIPTION_MINUTES   15
// PullMessages Timeout: 5 minutes (camera blocks for up to this long)
#define ONVIF_PULL_TIMEOUT_MINUTES   5
// HTTP socket timeout for PullMessages (5 min + 15 sec buffer)
#define ONVIF_HTTP_PULL_TIMEOUT_MS   (315UL * 1000UL)
// HTTP socket timeout for all other requests
#define ONVIF_HTTP_TIMEOUT_MS        (8UL * 1000UL)
// Renew if less than this many ms remain on subscription
#define ONVIF_RENEW_THRESHOLD_MS     (2UL * 60UL * 1000UL)
// Max number of events to return from a single PullMessages
#define ONVIF_MAX_EVENTS             8
// Max topic string length
#define ONVIF_MAX_TOPIC_LEN          80
// Max URL length for subscription manager URL
#define ONVIF_MAX_URL_LEN            128

struct OnvifEvent
{
    char topic[ONVIF_MAX_TOPIC_LEN];
    bool state;
};

struct OnvifEventList
{
    OnvifEvent events[ONVIF_MAX_EVENTS];
    uint8_t    count = 0;
};

class OnvifClient
{
  public:
    // Subscribe to ONVIF PullPoint. Returns true on success.
    bool subscribe(const char* host, uint16_t port, const char* path,
                   const char* user, const char* pass);

    // Blocking PullMessages call (up to ONVIF_PULL_TIMEOUT_MINUTES).
    // Fills events with any received notifications.
    // Returns false on connection error.
    bool pullMessages(OnvifEventList& out);

    // Renew subscription before it expires. Returns true on success.
    bool renew();

    // Unsubscribe and clean up.
    void unsubscribe();

    // True if currently subscribed.
    bool isSubscribed() const { return _subscribed; }

    // Milliseconds remaining before subscription expires (-1 if not subscribed).
    int32_t renewTimerMs() const;

  private:
    char     _host[64]  = {};
    uint16_t _port      = 80;
    char     _path[64]  = {};
    char     _user[32]  = {};
    char     _pass[32]  = {};
    char     _subscriptionManagerUrl[ONVIF_MAX_URL_LEN] = {};
    uint32_t _subscriptionStartMs  = 0;
    uint32_t _subscriptionLifeMs   = 0;
    bool     _subscribed            = false;

    // Build WS-Security block into dst (size dstLen)
    void buildWsSecurity(char* dst, size_t dstLen);

    // Build full SOAP envelope for Subscribe
    bool buildSubscribeBody(char* dst, size_t dstLen);

    // Build full SOAP envelope for PullMessages
    bool buildPullMessagesBody(char* dst, size_t dstLen);

    // Build full SOAP envelope for Renew
    bool buildRenewBody(char* dst, size_t dstLen);

    // Build full SOAP envelope for Unsubscribe
    bool buildUnsubscribeBody(char* dst, size_t dstLen);

    // POST SOAP body to url, read response into dynamically allocated buffer.
    // Caller must free() the returned pointer (or it is nullptr on error).
    char* postSoap(const char* url, const char* body, uint32_t timeoutMs);

    // Extract first value between <tag ...> and </tag> or value="..." attribute
    static bool extractTagContent(const char* xml, const char* tag, char* out, size_t outLen);

    // Parse SubscriptionReference Address from Subscribe response
    bool parseSubscribeResponse(const char* xml);

    // Parse NotificationMessage elements from PullMessages response
    static void parsePullResponse(const char* xml, OnvifEventList& out);

    // Helpers for WS-Security digest
    static void generateNonce(uint8_t* buf, size_t len);
    static void getCurrentUtcTimestamp(char* buf, size_t bufLen);
    static void computePasswordDigest(const uint8_t* nonce, size_t nonceLen,
                                      const char* created, const char* pass,
                                      char* digest64Out, size_t digest64Len);
    static void base64Encode(const uint8_t* in, size_t inLen, char* out, size_t outLen);
    static void generateMsgId(char* buf, size_t bufLen);
};

#endif // ARDUINO_ARCH_ESP32
