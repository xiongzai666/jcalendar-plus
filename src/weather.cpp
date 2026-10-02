#include "weather.h"

#include <_preference.h>
#include "weather_cache_core.h"

TaskHandle_t WEATHER_HANDLER;

String _qweather_host;
String _qweather_key;
String _qweather_loc;

int8_t _weather_status = -1;
int8_t _weather_type = -1;
bool _weather_fallback = false;
Weather _weather_now = {};
DailyWeather dailyWeather[3] = {};
DailyForecast _daily_forecast = {
    .weather = dailyWeather,
    .length = 3
};

namespace {

WeatherCacheData cached = {};
bool cacheLoaded = false;
String dateKey(time_t stamp, int offset = 0) {
    tm local = {}; localtime_r(&stamp, &local);
    if (local.tm_year + 1900 < 2025) return "";
    local.tm_hour = 12; local.tm_min = local.tm_sec = 0;
    local.tm_mday += offset; local.tm_isdst = -1; mktime(&local);
    char key[11]; strftime(key, sizeof(key), "%Y-%m-%d", &local); return key;
}
uint32_t readingTime(const String& responseTime) {
    time_t now = time(nullptr);
    if (now >= 1735689600) return static_cast<uint32_t>(now);
    tm local = {};
    if (!strptime(responseTime.c_str(), "%Y-%m-%dT%H:%M", &local)) return 0;
    local.tm_isdst = -1;
    now = mktime(&local);
    return now >= 1735689600 ? static_cast<uint32_t>(now) : 0;
}
void cacheDay(CachedWeatherDay& dst, const DailyWeather& src) {
    dst = {};
    src.date.toCharArray(dst.date, sizeof(dst.date));
    src.textDay.toCharArray(dst.textDay, sizeof(dst.textDay));
    src.textNight.toCharArray(dst.textNight, sizeof(dst.textNight));
    src.windDirDay.toCharArray(dst.windDirDay, sizeof(dst.windDirDay));
    src.windDirNight.toCharArray(dst.windDirNight, sizeof(dst.windDirNight));
    dst.high = src.tempMax; dst.low = src.tempMin; dst.humidity = src.humidity;
    dst.iconDay = src.iconDay; dst.iconNight = src.iconNight;
    dst.windScaleDay = src.windScaleDay; dst.windScaleNight = src.windScaleNight;
}
void restoreDay(DailyWeather& dst, const CachedWeatherDay& src) {
    dst = {};
    dst.date = src.date; dst.textDay = src.textDay; dst.textNight = src.textNight;
    dst.windDirDay = src.windDirDay; dst.windDirNight = src.windDirNight;
    dst.tempMax = src.high; dst.tempMin = src.low; dst.humidity = src.humidity;
    dst.iconDay = src.iconDay; dst.iconNight = src.iconNight;
    dst.windScaleDay = src.windScaleDay; dst.windScaleNight = src.windScaleNight;
}
// Legacy binary data is read only at its exact length; zero timestamps are
// deliberately stale. Its missing night forecast is never replaced by daytime.
bool loadCache() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE, true)) return false;
    const String location = pref.getString(PREF_QWEATHER_LOC, "");
    _weather_type = pref.getString(PREF_QWEATHER_TYPE) == "1" ? 1 : 0;
    WeatherCacheData candidate = {};
    const size_t size = pref.getBytesLength(PREF_WEATHER_CACHE);
    bool valid = false;
    if (size == sizeof(WeatherCacheData) || size == sizeof(LegacyWeatherCache)) {
        // Bounded read; corrupt or incompatible records never reach rendering.
        uint8_t bytes[sizeof(WeatherCacheData)] = {};
        if (pref.getBytes(PREF_WEATHER_CACHE, bytes, size) == size)
            valid = decodeWeatherCache(bytes, size, candidate);
    }
    pref.end();
    if (!valid || location.isEmpty() || location != candidate.location) {
        cached = {}; location.toCharArray(cached.location, sizeof(cached.location));
        cached.magic = WEATHER_CACHE_V3_MAGIC; cacheLoaded = true; return false;
    }
    cached = candidate; cacheLoaded = true; return true;
}
void publishCache() {
    const time_t now = time(nullptr);
    for (int i = 0; i < 3; ++i) {
        dailyWeather[i] = {};
        const String wanted = dateKey(now, i);
        const int index = wanted.isEmpty() ? -1 : weatherCacheDayIndex(cached, wanted.c_str());
        if (index >= 0) restoreDay(dailyWeather[i], cached.days[index]);
    }
    _weather_now = {};
    if (weatherReadingFresh(cached.nowReadAt, false, now, 4 * 3600)) {
        _weather_now.text = cached.nowText; _weather_now.time = cached.nowTime;
        _weather_now.updateTime = cached.nowUpdateTime; _weather_now.windDir = cached.nowWindDir;
        _weather_now.temp = cached.nowTemp; _weather_now.humidity = cached.nowHumidity;
        _weather_now.windScale = cached.nowWindScale; _weather_now.icon = cached.nowIcon;
    }
    _daily_forecast.updateTime = cached.dailyUpdateTime; _daily_forecast.length = 3;
    _weather_fallback = (weather_now_available() && cached.nowFailed) ||
        (weather_daily_available(0) && (cached.dailyFailed ||
            !weatherReadingFresh(cached.dailyReadAt, false, now, 4 * 3600)));
}
void saveWeatherCache() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return;
    sealWeatherCache(cached);
    const bool saved = pref.putBytes(PREF_WEATHER_CACHE, &cached, sizeof(cached)) == sizeof(cached);
    pref.end();
    Serial.println(saved ? "Weather cache saved." : "Weather cache save failed.");
}
}

bool weather_restore_cached() {
    loadCache(); publishCache();
    const bool available = weather_now_available() || weather_daily_available(0) ||
                           weather_daily_available(1) || weather_daily_available(2);
    if (available) _weather_status = 1;
    return available;
}
bool weather_is_fallback() { return _weather_fallback; }
uint32_t weather_now_read_at() { if (!cacheLoaded) loadCache(); return cached.nowReadAt; }
uint32_t weather_daily_read_at() { if (!cacheLoaded) loadCache(); return cached.dailyReadAt; }
bool weather_now_failed() { if (!cacheLoaded) loadCache(); return cached.nowFailed; }
bool weather_daily_failed() { if (!cacheLoaded) loadCache(); return cached.dailyFailed; }
bool weather_now_available() { return !_weather_now.text.isEmpty(); }
bool weather_daily_available(int index) {
    return index >= 0 && index < 3 && !dailyWeather[index].date.isEmpty() &&
           !dailyWeather[index].textDay.isEmpty();
}
String weather_dismissal_hint(bool night, time_t snapshot) {
    const bool nowFresh = weather_now_available() &&
        weatherReadingFresh(cached.nowReadAt, cached.nowFailed, snapshot, 1800);
    const DailyWeather& day = dailyWeather[0];
    const String forecast = night ? day.textNight : day.textDay;
    const bool dailyFresh = !forecast.isEmpty() &&
        weatherReadingFresh(cached.dailyReadAt, cached.dailyFailed, snapshot, 1800);
    return weatherUmbrellaHint(_weather_now.text.c_str(), nowFresh, forecast.c_str(), dailyFresh);
}
int8_t weather_type() { return _weather_type; }
int8_t weather_status() { return _weather_status; }
Weather* weather_data_now() { return &_weather_now; }
DailyForecast* weather_data_daily() { return &_daily_forecast; }

#include "owned_task_core.h"
void task_weather(void* param) {
    Serial.println("[Task] get weather begin...");
    runOwnedTask([]() -> int {
    loadCache();
    API<1> api;
    DailyWeather fetchedDays[3] = {};
    DailyForecast fetchedDaily = {.weather = fetchedDays, .length = 3};
    Weather fetchedNow = {};
    const bool dailySuccess = api.getForecastDaily(fetchedDaily, _qweather_host.c_str(),
        _qweather_key.c_str(), _qweather_loc.c_str());
    const bool nowSuccess = _weather_type == 1 && api.getWeatherNow(fetchedNow,
        _qweather_host.c_str(), _qweather_key.c_str(), _qweather_loc.c_str());
    cached.dailyFailed = !dailySuccess;
    cached.nowFailed = _weather_type == 1 && !nowSuccess;
    if (dailySuccess) {
        for (int i = 0; i < 3; ++i) cacheDay(cached.days[i], fetchedDays[i]);
        fetchedDaily.updateTime.toCharArray(cached.dailyUpdateTime, sizeof(cached.dailyUpdateTime));
        cached.dailyReadAt = readingTime(fetchedDaily.updateTime);
    }
    if (nowSuccess) {
        fetchedNow.text.toCharArray(cached.nowText, sizeof(cached.nowText));
        fetchedNow.time.toCharArray(cached.nowTime, sizeof(cached.nowTime));
        fetchedNow.updateTime.toCharArray(cached.nowUpdateTime, sizeof(cached.nowUpdateTime));
        fetchedNow.windDir.toCharArray(cached.nowWindDir, sizeof(cached.nowWindDir));
        cached.nowTemp = fetchedNow.temp; cached.nowHumidity = fetchedNow.humidity;
        cached.nowWindScale = fetchedNow.windScale; cached.nowIcon = fetchedNow.icon;
        cached.nowReadAt = readingTime(fetchedNow.updateTime);
    }
    publishCache();
    // Clock recovery can use a fresh HTTP Date even before SNTP is available.
    if (time(nullptr) < 1735689600) {
        if (dailySuccess) { for (int i = 0; i < 3; ++i) dailyWeather[i] = fetchedDays[i];
            _daily_forecast.updateTime = fetchedDaily.updateTime; }
        if (nowSuccess) _weather_now = fetchedNow;
    }
    const bool any = weather_now_available() || weather_daily_available(0) ||
                     weather_daily_available(1) || weather_daily_available(2);
    saveWeatherCache();
    return any ? 1 : 2;
    }, [](int status) {
        _weather_status = status;
        Serial.println("[Task] get weather end...");
        WEATHER_HANDLER = NULL;
    }, []() { vTaskDelete(NULL); });
}
void weather_exec(int status) {
    if (status > 0 || !WiFi.isConnected()) {
        weather_restore_cached();
        cached.nowFailed = cached.dailyFailed = true;
        _weather_fallback = weather_now_available() || weather_daily_available(0);
        _weather_status = _weather_fallback || weather_daily_available(1) || weather_daily_available(2) ? 1 : (status > 0 ? status : 2);
        if (cacheLoaded) saveWeatherCache();
        return;
    }
    Preferences pref; pref.begin(PREF_NAMESPACE, true);
    _qweather_host = pref.getString(PREF_QWEATHER_HOST, "api.qweather.com");
    _qweather_key = pref.getString(PREF_QWEATHER_KEY, "");
    _qweather_loc = pref.getString(PREF_QWEATHER_LOC, ""); pref.end();
    if (_qweather_key.isEmpty() || _qweather_loc.isEmpty()) { _weather_status = 3; return; }
    if (WEATHER_HANDLER) { vTaskDelete(WEATHER_HANDLER); WEATHER_HANDLER = NULL; }
    _weather_status = 0;
    if (xTaskCreate(task_weather, "WeatherData", 1024 * 8, NULL, 2, &WEATHER_HANDLER) != pdPASS)
        weather_exec(2);
}
void weather_stop() {
    if (WEATHER_HANDLER) { vTaskDelete(WEATHER_HANDLER); WEATHER_HANDLER = NULL; }
    weather_exec(2);
}
