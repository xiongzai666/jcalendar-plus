#include "ota_image_core.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
// One DROM segment: header 24 + segment header 8 + SDK/custom descriptors 324,
// checksum at offset 367, SHA256 at 368..399.
static std::vector<uint8_t> image(const char* project="jcalendar-plus-z15") {
 std::vector<uint8_t> b(400,0); b[0]=0xe9;b[1]=1;b[23]=1;
 b[26]=0x40;b[27]=0x3f;b[28]=0x44;b[29]=1;
 b[32]=0x32;b[33]=0x54;b[34]=0xcd;b[35]=0xab;
 b[288]='J';b[289]='O';b[290]='T';b[291]='A';
 std::strcpy(reinterpret_cast<char*>(&b[324]),"2.3.2-rc.1");
 std::strcpy(reinterpret_cast<char*>(&b[292]),project);return b;
}
int main(int argc,char** argv){
 auto valid=image();
 OtaImageGuard g("jcalendar-plus-z15",0x1e0000);
 assert(g.feed(valid.data(),31));assert(!g.ready());
 assert(g.feed(valid.data()+31,325));assert(g.ready());assert(!g.complete());
 assert(g.feed(valid.data()+356,44));assert(g.complete());assert(g.size()==400);
 for(int variant=0;variant<9;++variant){auto b=image();
  if(variant==0)b[0]=0;
  if(variant==1)b[12]=5;
  if(variant==2)b[32]=0;
  if(variant==3)std::strcpy(reinterpret_cast<char*>(&b[292]),"jcalendar-plus-z21");
  if(variant==4)b[288]=0; // Legacy image has no custom driver identity.
  if(variant==5)b[31]=0x7f;
  if(variant==6)b[1]=0;
  if(variant==7)std::memset(&b[324],'a',32);
  if(variant==8)b[23]=2;
  OtaImageGuard bad("jcalendar-plus-z15",0x1e0000);
  assert(!bad.feed(b.data(),b.size()));assert(!bad.ready());assert(!bad.complete());
 }
 for(size_t n: {size_t(0),size_t(23),size_t(355),size_t(367),size_t(399)}){
  OtaImageGuard truncated("jcalendar-plus-z15",0x1e0000);
  assert(truncated.feed(valid.data(),n));assert(!truncated.complete());
 }
 OtaImageGuard overflow("jcalendar-plus-z15",320);
 assert(!overflow.feed(valid.data(),valid.size()));assert(!overflow.complete());
 valid.push_back(0);OtaImageGuard trailing("jcalendar-plus-z15",0x1e0000);
 assert(!trailing.feed(valid.data(),valid.size()));assert(!trailing.complete());
 auto two=image();two[1]=2;
 // Second empty segment at offset 356. End is 364, checksum=367, hash end=400.
 OtaImageGuard multi("jcalendar-plus-z15",0x1e0000);
 for(auto c:two) assert(multi.feed(&c,1));assert(multi.complete());
 std::puts("OTA image guard tests passed");
 if(argc==2){
  FILE* f=std::fopen(argv[1],"rb");assert(f);
  std::vector<uint8_t> actual;int byte;
  while((byte=std::fgetc(f))!=EOF)actual.push_back(uint8_t(byte));
  std::fclose(f);OtaImageGuard built("jcalendar-plus-z15",0x1e0000);
  for(size_t at=0;at<actual.size();){
   size_t n=actual.size()-at;if(n>1436)n=1436;
   assert(built.feed(actual.data()+at,n));at+=n;
  }
  assert(built.complete());
  assert(!std::strcmp(reinterpret_cast<const char*>(built.prefix()+324),"2.3.2-rc.1"));
  std::printf("Real Z15 image accepted: %zu bytes, 2.3.2-rc.1\n",actual.size());
 }
}
