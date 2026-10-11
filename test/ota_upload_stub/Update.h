#pragma once
#include "Arduino.h"
#include <vector>
#define UPDATE_SIZE_UNKNOWN 0xffffffff
#define U_FLASH 0
// External flash boundary: records boot selection and exact bytes handed to Update.
struct TestUpdate {
  bool activated=false,open=false;
  std::vector<uint8_t> bytes;
  bool begin(size_t,int) { open=true;bytes.clear();return true; }
  size_t write(uint8_t* p,size_t n) { bytes.insert(bytes.end(),p,p+n);return n; }
  void abort() { open=false; }
  bool end(bool) { activated=true;open=false;return true; }
};
extern TestUpdate Update;
