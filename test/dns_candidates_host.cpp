#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "dns_candidates_core.h"
int main() {
    uint8_t query[512];
    size_t length = makeDnsQuery("config.example.test",0x1234,query,sizeof(query));
    assert(length > 12);
    std::vector<uint8_t> packet(query,query+length);
    packet[2]=0x81;packet[3]=0x80;packet[7]=2;
    const uint8_t first[]={0xc0,0x0c,0,1,0,1,0,0,0,30,0,4,43,174,246,62};
    const uint8_t second[]={0xc0,0x0c,0,1,0,1,0,0,0,30,0,4,43,174,247,62};
    packet.insert(packet.end(),first,first+sizeof(first));
    packet.insert(packet.end(),second,second+sizeof(second));
    DnsCandidates result = {};
    assert(parseDnsCandidates(packet.data(),packet.size(),0x1234,"config.example.test",result));
    assert(result.count==2 && result.address[1][2]==247 && result.ttl==30);
    assert(!parseDnsCandidates(packet.data(),packet.size(),0x1235,"config.example.test",result));
    assert(!parseDnsCandidates(packet.data(),packet.size(),0x1234,"other.example",result));
    assert(!parseDnsCandidates(packet.data(),packet.size()-1,0x1234,"config.example.test",result));
    packet[2] |= 2;
    assert(!parseDnsCandidates(packet.data(),packet.size(),0x1234,"config.example.test",result));
    // A real Pages response normally follows a CNAME and compresses its owner.
    packet.assign(query,query+length);packet[2]=0x81;packet[3]=0x80;packet[7]=3;
    const uint8_t alias[]={0xc0,0x0c,0,5,0,1,0,0,0,15,0,14,4,'p','o','o','l',7,'e','x','a','m','p','l','e',0};
    const size_t target=length+12;
    packet.insert(packet.end(),alias,alias+sizeof(alias));
    std::vector<uint8_t> a(first,first+sizeof(first)),b(second,second+sizeof(second));
    a[0]=b[0]=0xc0|uint8_t(target>>8);a[1]=b[1]=uint8_t(target);
    packet.insert(packet.end(),a.begin(),a.end());packet.insert(packet.end(),b.begin(),b.end());
    assert(parseDnsCandidates(packet.data(),packet.size(),0x1234,"config.example.test",result));
    assert(result.count==2&&result.ttl==15);
    // Ignore A records belonging to a different name, even in the answer section.
    packet[7]=1;packet.resize(length);
    const uint8_t unrelated[]={5,'o','t','h','e','r',7,'e','x','a','m','p','l','e',0,0,1,0,1,0,0,0,30,0,4,1,2,3,4};
    packet.insert(packet.end(),unrelated,unrelated+sizeof(unrelated));
    assert(!parseDnsCandidates(packet.data(),packet.size(),0x1234,"config.example.test",result));
    packet[2] &= ~2;
    packet[12]=0xc0;packet[13]=0x0c;
    assert(!parseDnsCandidates(packet.data(),packet.size(),0x1234,"config.example.test",result));
    DnsPreferred cache;
    const uint8_t good[]={43,174,247,62};
    cache.remember("config.example.test",443,good,0xfffffff0,30);
    assert(cache.matches("config.example.test",443,10));
    assert(!cache.matches("other.example",443,10));
    assert(!cache.matches("config.example.test",443,30000));
    puts("DNS candidates: multiple A records, query identity, bounds, compression loop and TTL cache passed");
}
