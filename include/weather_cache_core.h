#pragma once
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "schedule_optimization.h"
constexpr uint32_t WEATHER_CACHE_V3_MAGIC = 0x57584333;
struct CachedWeatherDay {
    char date[11], textDay[32], textNight[32], windDirDay[24], windDirNight[24];
    int8_t high, low, humidity, windScaleDay, windScaleNight;
    uint16_t iconDay, iconNight;
};
struct WeatherCacheData {
    uint32_t magic;
    char location[64];
    CachedWeatherDay days[3];
    char nowText[32], nowTime[40], nowUpdateTime[40], nowWindDir[24], dailyUpdateTime[40];
    int8_t nowTemp, nowHumidity, nowWindScale;
    uint16_t nowIcon;
    uint32_t nowReadAt, dailyReadAt;
    bool nowFailed, dailyFailed;
    uint32_t checksum;
};
struct LegacyWeatherCache {
    uint32_t magic;
    char location[64];
    char today[11];
    char date[3][11];
    char text[3][32];
    int8_t high[3];
    int8_t low[3];
    int8_t dailyHumidity[3];
    uint16_t dailyIcon[3];
    char dailyWindDir[3][24];
    int8_t dailyWindScale[3];
    char dailyUpdateTime[40];
    char nowText[32];
    char nowTime[40];
    char nowUpdateTime[40];
    char nowWindDir[24];
    int8_t nowTemp;
    int8_t nowHumidity;
    int8_t nowWindScale;
    uint16_t nowIcon;
};
inline uint32_t weatherCacheChecksum(const WeatherCacheData& value) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < offsetof(WeatherCacheData, checksum); ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
inline void sealWeatherCache(WeatherCacheData& value) {
    value.magic = WEATHER_CACHE_V3_MAGIC; value.checksum = weatherCacheChecksum(value);
}
inline bool decodeWeatherCache(const void* data, size_t size, WeatherCacheData& out) {
    out = {};
    if (!data) return false;
    if (size == sizeof(WeatherCacheData)) {
        memcpy(&out,data,size);
        if (out.magic != WEATHER_CACHE_V3_MAGIC || out.checksum != weatherCacheChecksum(out)) return false;
    } else if (size == sizeof(LegacyWeatherCache)) {
        LegacyWeatherCache old = {}; memcpy(&old,data,size);
        if (old.magic != 0x57584332) return false;
            out.magic = WEATHER_CACHE_V3_MAGIC;
            memcpy(out.location, old.location, sizeof(out.location));
            for (int i = 0; i < 3; ++i) {
                memcpy(out.days[i].date, old.date[i], sizeof(old.date[i]));
                memcpy(out.days[i].textDay, old.text[i], sizeof(old.text[i]));
                memcpy(out.days[i].windDirDay, old.dailyWindDir[i], sizeof(old.dailyWindDir[i]));
                out.days[i].high = old.high[i]; out.days[i].low = old.low[i];
                out.days[i].humidity = old.dailyHumidity[i]; out.days[i].iconDay = old.dailyIcon[i];
                out.days[i].windScaleDay = old.dailyWindScale[i];
            }
            memcpy(out.nowText, old.nowText, sizeof(old.nowText));
            memcpy(out.nowTime, old.nowTime, sizeof(old.nowTime));
            memcpy(out.nowUpdateTime, old.nowUpdateTime, sizeof(old.nowUpdateTime));
            memcpy(out.nowWindDir, old.nowWindDir, sizeof(old.nowWindDir));
            memcpy(out.dailyUpdateTime, old.dailyUpdateTime, sizeof(old.dailyUpdateTime));
            out.nowTemp = old.nowTemp; out.nowHumidity = old.nowHumidity;
            out.nowWindScale = old.nowWindScale; out.nowIcon = old.nowIcon;
            out.nowFailed = out.dailyFailed = true;
    } else return false;
    out.location[63] = 0;
    for (auto& day : out.days) {
        day.date[10] = 0; day.textDay[31] = day.textNight[31] = 0;
        day.windDirDay[23] = day.windDirNight[23] = 0;
    }
    out.nowText[31] = 0; out.nowTime[39] = out.nowUpdateTime[39] = 0;
    out.nowWindDir[23] = 0; out.dailyUpdateTime[39] = 0;
    return true;
}
inline int weatherCacheDayIndex(const WeatherCacheData& cache, const char* date) {
    for (int i = 0; i < 3; ++i)
        if (strncmp(cache.days[i].date, date, sizeof(cache.days[i].date)) == 0) return i;
    return -1;
}
inline const char* weatherCacheForecastText(const CachedWeatherDay& day, bool night) {
    return night ? day.textNight : day.textDay;
}
inline bool weatherReadingFresh(int64_t readAt, bool failed, int64_t now, int64_t maxAge) {
    return readAt > 0 && !failed && now >= readAt - 300 && now - readAt <= maxAge;
}
inline const char* weatherUmbrellaHint(const char* current, bool nowFresh,
                                      const char* forecast, bool dailyFresh) {
    if (nowFresh && wetWeatherText(current)) return "带伞·有雨";
    if (dailyFresh && wetWeatherText(forecast)) return "带伞·预报雨";
    if (nowFresh && dailyFresh) return "当前无雨";
    if (dailyFresh) return "预报无雨";
    if (nowFresh) return "预报未更新";
    return "天气未更新";
}
