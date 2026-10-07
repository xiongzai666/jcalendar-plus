#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "bounded_gzip.h"
static const uint8_t valid[] = {31,139,8,0,0,0,0,0,2,255,171,86,74,206,79,73,85,178,82,50,50,48,80,210,81,202,207,86,178,42,41,42,77,173,5,0,160,47,59,99,24,0,0,0};
static const uint8_t empty[] = {31,139,8,0,0,0,0,0,2,255,3,0,0,0,0,0,0,0,0,0};
static const uint8_t expands[] = {31,139,8,0,0,0,0,0,2,255,171,168,24,5,163,96,20,140,84,0,0,99,240,215,72,0,4,0,0};
int main() {
    GzipBuffer out;
    assert(decodeGzipBounded(valid,sizeof(valid),65536,out));
    assert(out.size==24 && std::memcmp(out.data,"{\"code\":\"200\",\"ok\":true}",24)==0);
    for(size_t n=0;n<18;++n) { assert(!decodeGzipBounded(valid,n,65536,out));assert(!out.data && out.size==0); }
    auto reject=[&](std::vector<uint8_t> raw) { assert(!decodeGzipBounded(raw.data(),raw.size(),65536,out));assert(!out.data && out.size==0); };
    std::vector<uint8_t> bad(valid,valid+sizeof(valid));bad.back()=255;reject(bad);
    bad.assign(valid,valid+sizeof(valid));bad[3]=2;
    const uint16_t headerCrc=uint16_t(~bounded_uzlib_crc32(bad.data(),10,0xffffffff));
    bad.insert(bad.begin()+10,{uint8_t(headerCrc),uint8_t(headerCrc>>8)});
    assert(decodeGzipBounded(bad.data(),bad.size(),65536,out));
    bad[10]^=1;reject(bad);
    bad.assign(expands,expands+sizeof(expands));bad[bad.size()-4]=1;bad[bad.size()-3]=0;reject(bad);
    bad.assign(valid,valid+sizeof(valid));bad[bad.size()-8]^=1;reject(bad);
    bad.assign(valid,valid+sizeof(valid));bad.erase(bad.begin()+12);reject(bad);
    bad.assign(valid,valid+sizeof(valid));bad[3]=8;bad[10]=0;reject(bad);
    bad.assign(32769,0);reject(bad);
    assert(decodeGzipBounded(empty,sizeof(empty),65536,out) && out.size==0);
    assert(!decodeGzipBounded(valid,sizeof(valid),23,out));
    for(int i=0;i<100;++i) { assert(decodeGzipBounded(valid,sizeof(valid),65536,out));assert(out.size==24); }
    puts("Gzip: bounded input/allocation/output, forged size, CRC, truncation, empty and repeated decoding");
}
