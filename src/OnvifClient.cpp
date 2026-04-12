#include "OnvifClient.h"

#ifdef ARDUINO_ARCH_ESP32

#include <mbedtls/sha1.h>
#include <mbedtls/base64.h>
#include <esp_random.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

bool OnvifClient::subscribe(const char* host, uint16_t port, const char* path,
                             const char* user, const char* pass)
{
    strncpy(_host, host, sizeof(_host) - 1);
    _port = port;
    strncpy(_path, path && path[0] ? path : "/onvif/event_service", sizeof(_path) - 1);
    strncpy(_user, user, sizeof(_user) - 1);
    strncpy(_pass, pass, sizeof(_pass) - 1);
    _subscribed = false;
    _subscriptionManagerUrl[0] = '\0';

    char url[128];
    snprintf(url, sizeof(url), "http://%s:%u%s", _host, _port, _path);

    char body[1500];
    if (!buildSubscribeBody(body, sizeof(body)))
    {
        log_e("ONVIF: buildSubscribeBody failed (body too large?)");
        return false;
    }

    char* resp = postSoap(url, body, ONVIF_HTTP_TIMEOUT_MS);
    if (!resp)
        return false;

    bool ok = parseSubscribeResponse(resp);
    if (!ok)
        log_e("ONVIF: parseSubscribeResponse fehlgeschlagen, resp=\n%.600s", resp);
    free(resp);

    if (ok)
    {
        _subscribed = true;
        _subscriptionStartMs = millis();
        _subscriptionLifeMs  = (uint32_t)ONVIF_SUBSCRIPTION_MINUTES * 60UL * 1000UL;
        log_i("ONVIF: subscribed, manager=%s", _subscriptionManagerUrl);
    }
    return ok;
}

bool OnvifClient::pullMessages(OnvifEventList& out)
{
    if (!_subscribed)
        return false;

    out.count = 0;

    char body[1500];
    if (!buildPullMessagesBody(body, sizeof(body)))
        return false;

    // Blocking call – waits up to ONVIF_PULL_TIMEOUT_MINUTES for events
    char* resp = postSoap(_subscriptionManagerUrl, body, ONVIF_HTTP_PULL_TIMEOUT_MS);
    if (!resp)
    {
        _subscribed = false;  // connection lost, will re-subscribe
        return false;
    }

    parsePullResponse(resp, out);
    free(resp);
    return true;
}

bool OnvifClient::renew()
{
    if (!_subscribed)
        return false;

    char body[1500];
    if (!buildRenewBody(body, sizeof(body)))
        return false;

    char* resp = postSoap(_subscriptionManagerUrl, body, ONVIF_HTTP_TIMEOUT_MS);
    if (!resp)
        return false;

    free(resp);
    _subscriptionStartMs = millis();
    _subscriptionLifeMs  = (uint32_t)ONVIF_SUBSCRIPTION_MINUTES * 60UL * 1000UL;
    return true;
}

void OnvifClient::unsubscribe()
{
    if (!_subscribed)
        return;

    char body[1500];
    if (buildUnsubscribeBody(body, sizeof(body)))
    {
        char* resp = postSoap(_subscriptionManagerUrl, body, ONVIF_HTTP_TIMEOUT_MS);
        if (resp) free(resp);
    }
    _subscribed = false;
}

int32_t OnvifClient::renewTimerMs() const
{
    if (!_subscribed)
        return -1;
    uint32_t elapsed = millis() - _subscriptionStartMs;
    if (elapsed >= _subscriptionLifeMs)
        return 0;
    return (int32_t)(_subscriptionLifeMs - elapsed);
}

// ---------------------------------------------------------------------------
// SOAP body builders
// ---------------------------------------------------------------------------

void OnvifClient::buildWsSecurity(char* dst, size_t dstLen)
{
    uint8_t nonce[16];
    generateNonce(nonce, sizeof(nonce));

    char nonce64[32];
    base64Encode(nonce, sizeof(nonce), nonce64, sizeof(nonce64));

    char created[32];
    getCurrentUtcTimestamp(created, sizeof(created));

    char digest64[32];
    computePasswordDigest(nonce, sizeof(nonce), created, _pass, digest64, sizeof(digest64));

    snprintf(dst, dstLen,
        "<Security s:mustUnderstand=\"1\" "
        "xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">"
        "<UsernameToken>"
        "<Username>%s</Username>"
        "<Password Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">%s</Password>"
        "<Nonce EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">%s</Nonce>"
        "<Created xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">%s</Created>"
        "</UsernameToken>"
        "</Security>",
        _user, digest64, nonce64, created);
}

bool OnvifClient::buildSubscribeBody(char* dst, size_t dstLen)
{
    char msgId[48];
    generateMsgId(msgId, sizeof(msgId));

    char toUrl[128];
    snprintf(toUrl, sizeof(toUrl), "http://%s:%u%s", _host, _port, _path);

    char wsSec[700];
    buildWsSecurity(wsSec, sizeof(wsSec));

    int n = snprintf(dst, dstLen,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<s:Envelope"
        " xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://www.w3.org/2005/08/addressing\""
        " xmlns:w=\"http://docs.oasis-open.org/wsn/b-2\">"
        "<s:Header>"
        "<a:Action>http://www.onvif.org/ver10/events/wsdl/EventPortType/CreatePullPointSubscriptionRequest</a:Action>"
        "<a:MessageID>%s</a:MessageID>"
        "<a:To>%s</a:To>"
        "%s"
        "</s:Header>"
        "<s:Body>"
        "<CreatePullPointSubscription xmlns=\"http://www.onvif.org/ver10/events/wsdl\">"
        "<InitialTerminationTime>PT%dM</InitialTerminationTime>"
        "</CreatePullPointSubscription>"
        "</s:Body>"
        "</s:Envelope>",
        msgId, toUrl, wsSec, ONVIF_SUBSCRIPTION_MINUTES);

    return (n > 0 && (size_t)n < dstLen);
}

bool OnvifClient::buildPullMessagesBody(char* dst, size_t dstLen)
{
    char msgId[48];
    generateMsgId(msgId, sizeof(msgId));

    char wsSec[700];
    buildWsSecurity(wsSec, sizeof(wsSec));

    int n = snprintf(dst, dstLen,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<s:Envelope"
        " xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://www.w3.org/2005/08/addressing\""
        " xmlns:w=\"http://docs.oasis-open.org/wsn/b-2\">"
        "<s:Header>"
        "<a:Action>http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest</a:Action>"
        "<a:MessageID>%s</a:MessageID>"
        "<a:To>%s</a:To>"
        "%s"
        "</s:Header>"
        "<s:Body>"
        "<PullMessages xmlns=\"http://www.onvif.org/ver10/events/wsdl\">"
        "<Timeout>PT%dM</Timeout>"
        "<MessageLimit>%d</MessageLimit>"
        "</PullMessages>"
        "</s:Body>"
        "</s:Envelope>",
        msgId, _subscriptionManagerUrl, wsSec,
        ONVIF_PULL_TIMEOUT_MINUTES, ONVIF_MAX_EVENTS);

    return (n > 0 && (size_t)n < dstLen);
}

bool OnvifClient::buildRenewBody(char* dst, size_t dstLen)
{
    char msgId[48];
    generateMsgId(msgId, sizeof(msgId));

    char wsSec[700];
    buildWsSecurity(wsSec, sizeof(wsSec));

    int n = snprintf(dst, dstLen,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<s:Envelope"
        " xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://www.w3.org/2005/08/addressing\""
        " xmlns:w=\"http://docs.oasis-open.org/wsn/b-2\">"
        "<s:Header>"
        "<a:Action>http://docs.oasis-open.org/wsn/bw-2/SubscriptionManager/RenewRequest</a:Action>"
        "<a:MessageID>%s</a:MessageID>"
        "<a:To>%s</a:To>"
        "%s"
        "</s:Header>"
        "<s:Body>"
        "<Renew xmlns=\"http://docs.oasis-open.org/wsn/b-2\">"
        "<TerminationTime>PT%dM</TerminationTime>"
        "</Renew>"
        "</s:Body>"
        "</s:Envelope>",
        msgId, _subscriptionManagerUrl, wsSec, ONVIF_SUBSCRIPTION_MINUTES);

    return (n > 0 && (size_t)n < dstLen);
}

bool OnvifClient::buildUnsubscribeBody(char* dst, size_t dstLen)
{
    char msgId[48];
    generateMsgId(msgId, sizeof(msgId));

    char wsSec[700];
    buildWsSecurity(wsSec, sizeof(wsSec));

    int n = snprintf(dst, dstLen,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<s:Envelope"
        " xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
        " xmlns:a=\"http://www.w3.org/2005/08/addressing\">"
        "<s:Header>"
        "<a:Action>http://docs.oasis-open.org/wsn/bw-2/SubscriptionManager/UnsubscribeRequest</a:Action>"
        "<a:MessageID>%s</a:MessageID>"
        "<a:To>%s</a:To>"
        "%s"
        "</s:Header>"
        "<s:Body>"
        "<Unsubscribe xmlns=\"http://docs.oasis-open.org/wsn/b-2\"/>"
        "</s:Body>"
        "</s:Envelope>",
        msgId, _subscriptionManagerUrl, wsSec);

    return (n > 0 && (size_t)n < dstLen);
}

// ---------------------------------------------------------------------------
// HTTP POST
// ---------------------------------------------------------------------------

char* OnvifClient::postSoap(const char* url, const char* body, uint32_t timeoutMs)
{
    HTTPClient http;
    http.begin(url);
    http.setTimeout(timeoutMs);
    http.addHeader("Content-Type", "application/soap+xml; charset=utf-8");
    http.addHeader("Connection", "close");

    int code = http.POST((uint8_t*)body, strlen(body));
    if (code <= 0)
    {
        log_e("ONVIF: HTTP POST failed, error=%d url=%s", code, url);
        http.end();
        return nullptr;
    }
    if (code != 200)
        log_e("ONVIF: HTTP %d url=%s", code, url);

    String body_str = http.getString();
    http.end();

    if (body_str.length() == 0)
    {
        log_e("ONVIF: empty response, HTTP %d url=%s", code, url);
        return nullptr;
    }

    char* buf = (char*)malloc(body_str.length() + 1);
    if (!buf)
        return nullptr;

    memcpy(buf, body_str.c_str(), body_str.length() + 1);
    return buf;
}

// ---------------------------------------------------------------------------
// Response parsers
// ---------------------------------------------------------------------------

bool OnvifClient::parseSubscribeResponse(const char* xml)
{
    // Look for <Address> inside <SubscriptionReference>
    const char* ref = strstr(xml, "SubscriptionReference");
    if (!ref) ref = xml;  // fall back to first <Address>

    const char* addr = strstr(ref, "<Address>");
    if (!addr) addr = strstr(ref, "<wsa:Address>");
    if (!addr) addr = strstr(ref, "<wsa5:Address>");
    if (!addr) addr = strstr(ref, "<a:Address>");
    if (!addr) addr = strstr(xml,  "<Address>");        // nochmal im gesamten XML
    if (!addr) addr = strstr(xml,  "<wsa:Address>");
    if (!addr) addr = strstr(xml,  "<wsa5:Address>");
    if (!addr) addr = strstr(xml,  "<a:Address>");
    if (!addr)
    {
        log_e("ONVIF: Kein Address-Tag in Subscribe-Antwort:\n%.600s", xml);
        return false;
    }

    // Find text content
    const char* start = strchr(addr, '>');
    if (!start) return false;
    start++;

    const char* end = strchr(start, '<');
    if (!end) return false;

    size_t len = (size_t)(end - start);
    if (len == 0 || len >= ONVIF_MAX_URL_LEN)
        return false;

    strncpy(_subscriptionManagerUrl, start, len);
    _subscriptionManagerUrl[len] = '\0';
    return true;
}

void OnvifClient::parsePullResponse(const char* xml, OnvifEventList& out)
{
    // Each event is inside a <NotificationMessage> block
    const char* ptr = xml;
    while (out.count < ONVIF_MAX_EVENTS)
    {
        // Find next NotificationMessage
        const char* nm = strstr(ptr, "NotificationMessage");
        if (!nm) break;
        ptr = nm + 1;

        // Find Topic
        const char* topicStart = strstr(nm, "<Topic");
        if (!topicStart) continue;
        const char* topicContent = strchr(topicStart, '>');
        if (!topicContent) continue;
        topicContent++;
        const char* topicEnd = strchr(topicContent, '<');
        if (!topicEnd) continue;

        // Get last path segment (basename) of topic, e.g. "tns1:Motion/.../Visitor" → "Visitor"
        size_t topicLen = (size_t)(topicEnd - topicContent);
        char fullTopic[ONVIF_MAX_TOPIC_LEN] = {};
        if (topicLen >= sizeof(fullTopic)) topicLen = sizeof(fullTopic) - 1;
        strncpy(fullTopic, topicContent, topicLen);

        // Extract last component after '/' or ':'
        const char* topicName = fullTopic;
        for (const char* c = fullTopic; *c; c++)
            if (*c == '/' || *c == ':') topicName = c + 1;

        // Find SimpleItem with Name='State' or Name='IsMotion'
        bool state = false;
        const char* si = strstr(nm, "SimpleItem");
        while (si && si < ptr + 2000)
        {
            const char* nameAttr = strstr(si, "Name=\"");
            if (!nameAttr) break;
            nameAttr += 6;

            if (strncmp(nameAttr, "State", 5) == 0 ||
                strncmp(nameAttr, "IsMotion", 8) == 0)
            {
                const char* valAttr = strstr(si, "Value=\"");
                if (valAttr)
                {
                    valAttr += 7;
                    state = (strncmp(valAttr, "true", 4) == 0 ||
                             strncmp(valAttr, "1", 1) == 0);
                }
                break;
            }
            si = strstr(si + 1, "SimpleItem");
        }

        OnvifEvent& ev = out.events[out.count++];
        strncpy(ev.topic, topicName, sizeof(ev.topic) - 1);
        ev.state = state;
        log_d("ONVIF event: topic=%s state=%d", ev.topic, (int)ev.state);
    }
}

// ---------------------------------------------------------------------------
// WS-Security helpers
// ---------------------------------------------------------------------------

void OnvifClient::generateNonce(uint8_t* buf, size_t len)
{
    for (size_t i = 0; i < len; i += 4)
    {
        uint32_t r = esp_random();
        size_t copy = (len - i < 4) ? (len - i) : 4;
        memcpy(buf + i, &r, copy);
    }
}

void OnvifClient::getCurrentUtcTimestamp(char* buf, size_t bufLen)
{
    time_t now;
    time(&now);
    // Fallback: if time not set (before year 2020), use a plausible timestamp
    if (now < 1577836800L)
        now = 1577836800L;  // 2020-01-01 00:00:00 UTC
    struct tm* t = gmtime(&now);
    strftime(buf, bufLen, "%Y-%m-%dT%H:%M:%SZ", t);
}

void OnvifClient::computePasswordDigest(const uint8_t* nonce, size_t nonceLen,
                                         const char* created, const char* pass,
                                         char* digest64Out, size_t digest64Len)
{
    // PasswordDigest = Base64(SHA1(nonce_raw + created + password))
    size_t createdLen = strlen(created);
    size_t passLen    = strlen(pass);
    size_t totalLen   = nonceLen + createdLen + passLen;

    uint8_t* concat = (uint8_t*)malloc(totalLen);
    if (!concat)
    {
        digest64Out[0] = '\0';
        return;
    }
    memcpy(concat, nonce, nonceLen);
    memcpy(concat + nonceLen, created, createdLen);
    memcpy(concat + nonceLen + createdLen, pass, passLen);

    uint8_t sha1result[20];
    mbedtls_sha1(concat, totalLen, sha1result);
    free(concat);

    base64Encode(sha1result, 20, digest64Out, digest64Len);
}

void OnvifClient::base64Encode(const uint8_t* in, size_t inLen,
                                char* out, size_t outLen)
{
    size_t written = 0;
    mbedtls_base64_encode((unsigned char*)out, outLen, &written, in, inLen);
    if (written < outLen)
        out[written] = '\0';
    else if (outLen > 0)
        out[outLen - 1] = '\0';
}

void OnvifClient::generateMsgId(char* buf, size_t bufLen)
{
    uint32_t a = esp_random(), b = esp_random(), c = esp_random(), d = esp_random();
    snprintf(buf, bufLen, "urn:uuid:%08lx-%04lx-%04lx-%04lx-%08lx%04lx",
             (unsigned long)a,
             (unsigned long)(b >> 16),
             (unsigned long)(b & 0xFFFF),
             (unsigned long)(c >> 16),
             (unsigned long)c,
             (unsigned long)(d >> 16));
}

bool OnvifClient::extractTagContent(const char* xml, const char* tag,
                                     char* out, size_t outLen)
{
    const char* pos = strstr(xml, tag);
    if (!pos) return false;
    const char* start = strchr(pos, '>');
    if (!start) return false;
    start++;
    const char* end = strchr(start, '<');
    if (!end) return false;
    size_t len = (size_t)(end - start);
    if (len >= outLen) len = outLen - 1;
    strncpy(out, start, len);
    out[len] = '\0';
    return true;
}

#endif // ARDUINO_ARCH_ESP32
