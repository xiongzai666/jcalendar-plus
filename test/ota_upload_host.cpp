#include <Update.h>
#include <WebServer.h>
#include "local_ota.h"
#include <cassert>
#include <cstring>
#include <cstdio>
TestUpdate Update;
TestEsp ESP;
static std::vector<uint8_t> image(const char* project="jcalendar-plus-z15") {
  std::vector<uint8_t> b(400,0);b[0]=0xe9;b[1]=1;b[23]=1;
  b[26]=0x40;b[27]=0x3f;b[28]=0x44;b[29]=1;
  b[32]=0x32;b[33]=0x54;b[34]=0xcd;b[35]=0xab;
  b[288]='J';b[289]='O';b[290]='T';b[291]='A';
  std::strcpy(reinterpret_cast<char*>(&b[324]),"2.3.2-rc.1");
  std::strcpy(reinterpret_cast<char*>(&b[292]),project);return b;
}
static void event(WebServer& s,int status) { s.file.status=status;s.chunk(); }
static void file(WebServer& s,const std::vector<uint8_t>& b,size_t length) {
  event(s,UPLOAD_FILE_START);
  // Cross the prefix over two HTTP chunks, as on the device.
  for(size_t at=0;at<length;) {
    size_t n=at==0 ? 31 : length-at;
    std::memcpy(s.file.buf,b.data()+at,n);s.file.currentSize=n;
    event(s,UPLOAD_FILE_WRITE);at+=n;
  }
  s.file.totalSize=length;event(s,UPLOAD_FILE_END);
}
int main() {
  WebServer s;installLocalOta(s);auto valid=image();
  // Regression: FILE_END occurs before the multipart request has completed.
  // Moving Update.end to FILE_END would activate an interrupted request.
  file(s,valid,valid.size());
  if(Update.activated) { std::puts("FAIL: FILE_END activated before HTTP completion");return 1; }
  event(s,UPLOAD_FILE_ABORTED);assert(!Update.activated && !Update.open);
  file(s,valid,valid.size());s.finish();
  assert(s.code==200 && Update.activated && Update.bytes==valid && ESP.restarts==1);
  Update.activated=false;
  auto wrong=image("jcalendar-plus-z21");file(s,wrong,wrong.size());s.finish();
  assert(s.code==400 && !Update.activated && !Update.open && ESP.restarts==1);
  file(s,valid,399);s.finish();assert(s.code==400 && !Update.activated && !Update.open);
  // Two files in one request must not activate the first before rejecting the second.
  file(s,valid,valid.size());file(s,wrong,wrong.size());s.finish();
  assert(s.code==400 && !Update.activated && !Update.open);
  file(s,valid,valid.size());s.finish();assert(s.code==200 && Update.activated);
  std::puts("OTA upload callbacks: valid, wrong driver, truncation, abort after FILE_END, two files passed");
}
