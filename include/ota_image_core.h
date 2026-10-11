#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// ESP-IDF app image format: 24-byte header, 8-byte first segment header,
// then the 256-byte SDK descriptor and our 68-byte custom descriptor in DROM.
class OtaImageGuard {
 public:
  static const size_t PrefixSize = 356;
  OtaImageGuard(const char* project, size_t capacity)
      : project_(project), capacity_(capacity) {}
  bool feed(const uint8_t* bytes, size_t count) {
    if (failed_) return false;
    if (count > capacity_ - position_) return fail("固件超过可用空间");
    for (size_t i=0; i<count; ++i) {
      if (expected_ && position_ >= expected_) return fail("固件长度不正确");
      const uint8_t b=bytes[i];
      if (position_ < PrefixSize) prefix_[position_]=b;
      ++position_;
      if (position_ == 24) {
        if (prefix_[0]!=0xe9 || !prefix_[1] || prefix_[1]>16 ||
            prefix_[12] || prefix_[13] || prefix_[23]>1)
          return fail("不是兼容的 ESP32 固件");
        segments_=prefix_[1]; hash_=prefix_[23];
      } else if (position_ == 32) {
        const uint32_t address=le32(prefix_+24);
        remaining_=le32(prefix_+28);
        if (address < 0x3f400000 || address >= 0x3f800000 ||
            remaining_ < PrefixSize-32 || remaining_ > 0x3f800000-address)
          return fail("固件应用描述缺失");
        if (remaining_ > capacity_-32) return fail("固件超过可用空间");
      } else if (position_ > 32 && !expected_) {
        if (remaining_) {
          --remaining_;
          if (!remaining_) {
            ++finishedSegments_;
            if (finishedSegments_==segments_) {
              expected_=((position_+16)/16)*16 + (hash_ ? 32 : 0);
              if (expected_>capacity_) return fail("固件超过可用空间");
            }
          }
        } else {
          segmentHeader_[headerBytes_++]=b;
          if (headerBytes_==8) {
            headerBytes_=0; remaining_=le32(segmentHeader_+4);
            if (remaining_ > capacity_-position_) return fail("固件超过可用空间");
            if (!remaining_) {
              ++finishedSegments_;
              if (finishedSegments_==segments_) {
                expected_=((position_+16)/16)*16 + (hash_ ? 32 : 0);
                if (expected_>capacity_) return fail("固件超过可用空间");
              }
            }
          }
        }
      }
      if (position_==PrefixSize) {
        if (le32(prefix_+32)!=0xabcd5432)
          return fail("固件应用描述缺失");
        if (le32(prefix_+288)!=0x41544f4a)
          return fail("固件缺少屏幕标识，请使用串口回退旧版");
        if (!sameString(prefix_+292,project_,32)) return fail("固件与当前屏幕不匹配");
        if (!validVersion(prefix_+324)) return fail("固件版本标识无效");
        ready_=true;
      }
    }
    return true;
  }
  bool ready() const { return ready_ && !failed_; }
  bool complete() const { return ready() && expected_ && position_==expected_; }
  size_t size() const { return position_; }
  const uint8_t* prefix() const { return prefix_; }
  const char* error() const { return error_; }
 private:
  static uint32_t le32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
  }
  static bool sameString(const uint8_t* p, const char* s, size_t limit) {
    const size_t n=strlen(s);
    return n<limit && !memcmp(p,s,n) && p[n]==0;
  }
  static bool validVersion(const uint8_t* p) {
    // Semantic release version, allowing a prerelease suffix. Bounded by IDF's field.
    size_t i=0;
    for (int group=0;group<3;++group) {
      const size_t start=i;
      while (i<32 && p[i]>='0' && p[i]<='9') ++i;
      if (i==start || i>=32) return false;
      if (group<2 && p[i++]!='.') return false;
    }
    if (p[i]==0) return true;
    if (p[i++]!='-' || i>=32 || !p[i]) return false;
    for (;i<32 && p[i];++i)
      if (!((p[i]>='0' && p[i]<='9') || (p[i]>='a' && p[i]<='z') ||
            (p[i]>='A' && p[i]<='Z') || p[i]=='.' || p[i]=='-')) return false;
    return i<32;
  }
  bool fail(const char* message) { failed_=true;error_=message;return false; }
  const char* project_;
  size_t capacity_,position_=0,expected_=0;
  uint32_t remaining_=0;
  uint8_t prefix_[PrefixSize]={},segmentHeader_[8]={};
  uint8_t segments_=0,finishedSegments_=0,headerBytes_=0,hash_=0;
  bool ready_=false,failed_=false;
  const char* error_="上传尚未完成";
};
