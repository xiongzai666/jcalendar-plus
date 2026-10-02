#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
struct DnsCandidates { uint8_t address[4][4] = {}; unsigned count = 0; uint32_t ttl = 0; };
inline bool dnsSame(const char* a,const char* b) {
    while (*a && *b) { char x=*a++,y=*b++; if(x>='A'&&x<='Z')x+=32; if(y>='A'&&y<='Z')y+=32; if(x!=y)return false; }
    return *a==*b;
}
inline size_t makeDnsQuery(const char* host,uint16_t id,uint8_t* out,size_t capacity) {
    const size_t length=strlen(host); if(!length||length>253||capacity<length+18)return 0;
    memset(out,0,12); out[0]=id>>8;out[1]=id;out[2]=1;out[5]=1;size_t at=12;
    for(const char* p=host;*p;) {
        const char* end=strchr(p,'.'); if(!end)end=p+strlen(p);
        const size_t n=end-p;if(!n||n>63)return 0;
        out[at++]=n;memcpy(out+at,p,n);at+=n;p=*end?end+1:end;
    }
    out[at++]=0;out[at++]=0;out[at++]=1;out[at++]=0;out[at++]=1;return at;
}
inline bool dnsName(const uint8_t* wire,size_t size,size_t& at,char* out) {
    size_t p=at,count=0;bool jumped=false;unsigned hops=0;
    while(p<size&&++hops<=64) {
        const uint8_t length=wire[p++];
        if(!length) { if(!jumped)at=p;out[count]=0;return true; }
        if((length&0xc0)==0xc0) {
            if(p>=size)return false;size_t pointer=((length&0x3f)<<8)|wire[p++];
            if(!jumped)at=p;jumped=true;p=pointer;continue;
        }
        if(length>63||p+length>size||count+length+(count?1:0)>253)return false;
        if(count)out[count++]='.';
        for(unsigned i=0;i<length;++i) { const uint8_t c=wire[p++];if(c<=32||c>=127)return false;out[count++]=c; }
    }
    return false;
}
inline uint16_t dns16(const uint8_t* p) { return uint16_t(p[0])<<8|p[1]; }
inline bool parseDnsCandidates(const uint8_t* wire,size_t size,uint16_t id,const char* host,DnsCandidates& result) {
    result={};if(size<12||dns16(wire)!=id||(wire[2]&0xfa)!=0x80||(wire[3]&15)||dns16(wire+4)!=1)return false;
    char allowed[5][254]={};strncpy(allowed[0],host,253);unsigned allowedCount=1;
    char name[254];size_t at=12;
    if(!dnsName(wire,size,at,name)||!dnsSame(name,host)||at+4>size||dns16(wire+at)!=1||dns16(wire+at+2)!=1)return false;
    at+=4;uint32_t ttl=0xffffffff;
    const unsigned answers=dns16(wire+6);if(answers>32)return false;
    for(unsigned i=0;i<answers;++i) {
        if(!dnsName(wire,size,at,name)||at+10>size)return false;
        const unsigned type=dns16(wire+at),klass=dns16(wire+at+2),length=dns16(wire+at+8);
        const uint32_t lifetime=uint32_t(wire[at+4])<<24|uint32_t(wire[at+5])<<16|uint32_t(wire[at+6])<<8|wire[at+7];
        at+=10;if(at+length>size)return false;
        bool trusted=false;for(unsigned j=0;j<allowedCount;++j)if(dnsSame(name,allowed[j]))trusted=true;
        if(klass==1&&trusted&&type==5&&allowedCount<5) {
            size_t target=at;if(!dnsName(wire,size,target,allowed[allowedCount])||target!=at+length)return false;
            ++allowedCount;if(lifetime<ttl)ttl=lifetime;
        } else if(klass==1&&trusted&&type==1&&length==4) {
            bool duplicate=false;for(unsigned j=0;j<result.count;++j)if(!memcmp(result.address[j],wire+at,4))duplicate=true;
            if(!duplicate&&result.count<4)memcpy(result.address[result.count++],wire+at,4);
            if(lifetime<ttl)ttl=lifetime;
        }
        at+=length;
    }
    result.ttl=ttl==0xffffffff?0:ttl;return result.count>0;
}
struct DnsPreferred {
    char host[254]={};uint16_t port=0;uint8_t address[4]={};uint32_t learned=0,lifetime=0;
    void remember(const char* value,uint16_t service,const uint8_t* ip,uint32_t now,uint32_t ttl) {
        strncpy(host,value,253);host[253]=0;port=service;memcpy(address,ip,4);learned=now;lifetime=(ttl<60?ttl:60)*1000;
    }
    bool matches(const char* value,uint16_t service,uint32_t now) const { return port==service&&dnsSame(host,value)&&now-learned<lifetime; }
};
