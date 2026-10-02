#include <cassert>
#include <cstring>
#include <cstdio>
#include "weather_cache_core.h"
int main() {
    WeatherCacheData cache = {};
    strcpy(cache.days[0].date, "2026-09-30");
    strcpy(cache.days[1].date, "2026-10-01");
    strcpy(cache.days[2].date, "2026-10-02");
    strcpy(cache.days[1].textDay, "晴");
    strcpy(cache.days[1].textNight, "小雨");
    assert(weatherCacheDayIndex(cache, "2026-10-01") == 1);
    assert(weatherCacheDayIndex(cache, "2026-10-03") == -1);
    assert(!strcmp(weatherCacheForecastText(cache.days[1], true), "小雨"));
    assert(!strcmp(weatherCacheForecastText(cache.days[1], false), "晴"));
    assert(weatherReadingFresh(10000, false, 11000, 1800));
    assert(!weatherReadingFresh(10000, true, 11000, 1800));
    assert(!weatherReadingFresh(10000, false, 12000, 1800));
    assert(!weatherReadingFresh(0, false, 11000, 1800));
    assert(!strcmp(weatherUmbrellaHint("晴", true, "小雨", true), "带伞·预报雨"));
    assert(!strcmp(weatherUmbrellaHint("小雨", false, "晴", true), "预报无雨"));
    assert(!strcmp(weatherUmbrellaHint("晴", true, "小雨", false), "预报未更新"));
    assert(!strcmp(weatherUmbrellaHint("雨", false, "", false), "天气未更新"));
    // No observation timestamp is required: freshness uses retrieval only.
    assert(!strcmp(weatherUmbrellaHint("雨", weatherReadingFresh(10000, false, 10001, 1800), "", false), "带伞·有雨"));
    LegacyWeatherCache old = {};
    old.magic = 0x57584332;
    strcpy(old.location, "101220609");
    strcpy(old.date[1], "2026-10-01"); strcpy(old.text[1], "小雨");
    WeatherCacheData decoded = {};
    assert(decodeWeatherCache(&old, sizeof(old), decoded));
    assert(!strcmp(decoded.days[1].textDay, "小雨"));
    assert(decoded.days[1].textNight[0] == 0 && decoded.nowReadAt == 0 && decoded.dailyFailed);
    assert(!decodeWeatherCache(&old, sizeof(old) - 1, decoded));
    sealWeatherCache(cache);
    assert(decodeWeatherCache(&cache, sizeof(cache), decoded));
    cache.days[1].textNight[0] ^= 1;
    assert(!decodeWeatherCache(&cache, sizeof(cache), decoded));
    puts("weather cache: midnight dates, night forecast, independent failure and retrieval-age checks passed");
}
