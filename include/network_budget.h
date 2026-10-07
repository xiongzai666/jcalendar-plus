#pragma once
#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include "network_budget_core.h"
#include "http_body_core.h"
#include "deadline_client_core.h"
#include "dns_candidates_core.h"
NetworkBudget& wakeNetworkBudget();
inline DnsPreferred& preferredRemoteAddress() { static DnsPreferred value; return value; }
inline bool resolveDnsCandidates(const char* host,DnsCandidates& answer) {
    auto& budget=wakeNetworkBudget();if(!budget.canRequest(millis()))return false;
    WiFiUDP udp;uint8_t wire[512];const uint16_t id=uint16_t(esp_random());
    const size_t length=makeDnsQuery(host,id,wire,sizeof(wire));
    const IPAddress resolver=WiFi.dnsIP(0);
    if(!length||!resolver||!udp.begin(0))return false;
    if(!udp.beginPacket(resolver,53)||udp.write(wire,length)!=length||!udp.endPacket())return false;
    const uint32_t started=millis();
    while(millis()-started<1500&&budget.remaining(millis())) {
        const int size=udp.parsePacket();
        if(size>0) {
            if(udp.remoteIP()==resolver&&udp.remotePort()==53&&size<=int(sizeof(wire))) {
                const int got=udp.read(wire,sizeof(wire));
                if(got==size&&parseDnsCandidates(wire,got,id,host,answer))return true;
            }
            udp.flush();
        }
        delay(1);
    }
    return false;
}
struct NetworkClock { static uint32_t now() { return millis(); } };
class SecureTransport : public WiFiClientSecure {
    bool dnsCandidates = false;
protected:
    bool prepareConnection(int32_t requested) {
        const auto limits=blockingTransportLimits(wakeNetworkBudget(),millis(),requested);
        if(!limits.allowed)return false;
        _timeout=limits.connectMs;setHandshakeTimeout(limits.handshakeSeconds);return true;
    }
    virtual bool resolvePrimaryAddress(const char* host,IPAddress& address) {
        if(address.fromString(host))return true;
        DnsCandidates candidates;
        if(!resolveDnsCandidates(host,candidates)||!candidates.count)return false;
        address=IPAddress(candidates.address[0]);return true;
    }
public:
    void enableDnsCandidates() { dnsCandidates=true; }
    void disableReadWait() { Stream::setTimeout(0); }
    using WiFiClientSecure::connect;
    int connect(const char* host,uint16_t port,int32_t timeout) override {
        auto& budget=wakeNetworkBudget();
        if(!dnsCandidates)return WiFiClientSecure::connect(host,port,timeout);
        auto& preferred=preferredRemoteAddress();
        if(preferred.matches(host,port,millis())&&budget.canRequest(millis())) {
            if(!prepareConnection(timeout))return 0;
            const IPAddress ip(preferred.address);
            if(WiFiClientSecure::connect(ip,port,host,_CA_cert,_cert,_private_key))return 1;
            preferred.lifetime=0;
        }
        if(!budget.canRequest(millis()))return 0;
        IPAddress primary;
        if(resolvePrimaryAddress(host,primary)&&prepareConnection(timeout)&&
            WiFiClientSecure::connect(primary,port,host,_CA_cert,_cert,_private_key))return 1;
        DnsCandidates candidates;
        if(!resolveDnsCandidates(host,candidates))return 0;
        unsigned tried=0;
        for(unsigned i=0;i<candidates.count&&tried<2&&budget.canRequest(millis());++i) {
            const IPAddress ip(candidates.address[i]);if(ip==primary)continue;
            ++tried;if(!prepareConnection(timeout))break;
            Serial.printf("Remote same-host DNS fallback: %s\n",ip.toString().c_str());
            if(WiFiClientSecure::connect(ip,port,host,_CA_cert,_cert,_private_key)) {
                preferred.remember(host,port,candidates.address[i],millis(),candidates.ttl);return 1;
            }
        }
        return 0;
    }
};
class BudgetSecureClient : public DeadlineClient<SecureTransport,NetworkClock> {
public:
    BudgetSecureClient() : DeadlineClient(wakeNetworkBudget()) { enableDnsCandidates(); }
};
inline bool configureNetworkTimeouts(HTTPClient& http, WiFiClientSecure& client) {
    const auto& budget = wakeNetworkBudget();
    if (!budget.canRequest(millis())) return false;
    const uint32_t slice = budget.phaseTimeout(millis());
    http.setConnectTimeout(slice); http.setTimeout(slice);
    client.setHandshakeTimeout(slice / 1000);
    static const char* headers[] = {"Content-Encoding", "Date", "Transfer-Encoding"};
    http.collectHeaders(headers, 3);
    return true;
}
inline bool readBoundedBody(HTTPClient& http, String& body, size_t limit) {
    body = "";
    if (http.getSize() > int(limit)) return false;
    struct Clock {
        uint32_t now() { return millis(); }
        void pause() { delay(1); }
    } clock;
    WiFiClient* stream = http.getStreamPtr();
    if (!stream) return false;
    return readHttpBody(*stream,clock,wakeNetworkBudget(),http.getSize(),
        http.header("Transfer-Encoding").equalsIgnoreCase("chunked"),limit,
        wakeNetworkBudget().phaseTimeout(millis()),
        [&body](const uint8_t* data,size_t size) { return body.concat(reinterpret_cast<const char*>(data),size); });
}
