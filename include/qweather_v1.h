#ifndef JCALENDAR_QWEATHER_V1_H
#define JCALENDAR_QWEATHER_V1_H

#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

// Normalize V1 at the API boundary. Screen/cache units stay °C, %, km/h.
namespace qweather_v1 {
inline bool number(JsonVariantConst value, double& out, double low, double high) {
    if (!value.is<double>()) return false;
    out = value.as<double>();
    return std::isfinite(out) && out >= low && out <= high;
}

inline bool temperature(JsonVariantConst value, int8_t& out) {
    double n;
    if (!number(value["value"], n, -150, 300)) return false;
    const char* unit = value["unit"] | "";
    if (!strcmp(unit, "°F")) n = (n - 32) / 1.8;
    else if (strcmp(unit, "°C")) return false;
    if (n < -128 || n > 127) return false;
    out = static_cast<int8_t>(std::lround(n));
    return true;
}

inline const char* windDirection(const char* code) {
    const char* codes[] = {"n", "nne", "ne", "ene", "e", "ese", "se", "sse",
                           "s", "ssw", "sw", "wsw", "w", "wnw", "nw", "nnw", "none", "vrb"};
    const char* names[] = {"北风", "北东北风", "东北风", "东东北风", "东风", "东东南风", "东南风", "南东南风",
                           "南风", "南西南风", "西南风", "西西南风", "西风", "西西北风", "西北风", "北西北风", "无风", "旋转风"};
    for (unsigned i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
        if (!strcmp(code, codes[i])) return names[i];
    return nullptr;
}

struct Conditions {
    const char* text;
    const char* windDir;
    uint16_t icon;
    int8_t humidity, windScale;
    int16_t wind360;
    uint8_t windSpeed;
};

inline bool conditions(JsonVariantConst json, Conditions& out) {
    const char* text = json["condition"]["text"] | "";
    const char* code = json["condition"]["code"] | "";
    if (!*text || !*code) return false;
    char* end = nullptr;
    long icon = strtol(code, &end, 10);
    if (*end || icon <= 0 || icon > 65535) return false;
    double humidity, degree, scale, speed;
    JsonVariantConst wind = json["wind"];
    const char* direction = windDirection(wind["direction"]["compass"] | "");
    if (!direction || !number(json["humidity"], humidity, 0, 1) ||
        !number(wind["direction"]["degree"], degree, 0, 359) ||
        !number(wind["scale"], scale, 0, 17) ||
        !number(wind["speed"]["value"], speed, 0, 400)) return false;
    const char* unit = wind["speed"]["unit"] | "";
    if (!strcmp(unit, "m/s")) speed *= 3.6;
    else if (strcmp(unit, "km/h")) return false;
    out = {text, direction, static_cast<uint16_t>(icon),
           static_cast<int8_t>(std::lround(humidity * 100)),
           static_cast<int8_t>(std::lround(scale)), static_cast<int16_t>(std::lround(degree)),
           static_cast<uint8_t>(std::lround(speed > 255 ? 255 : speed))};
    return true;
}

template<class Weather>
bool parseWeather(JsonVariantConst json, Weather& out) {
    Conditions c;
    int8_t temp;
    if (!conditions(json, c) || !temperature(json["temperature"], temp)) return false;
    out.temp = temp; out.humidity = c.humidity; out.wind360 = c.wind360;
    out.windDir = c.windDir; out.windScale = c.windScale; out.windSpeed = c.windSpeed;
    out.icon = c.icon; out.text = c.text;
    return true;
}

inline int monthDays(int year, int month) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month < 1 || month > 12) return 0;
    return days[month - 1] + (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

inline bool isoDate(const char* value, char* out) {
    int year, month, day;
    if (!value || strlen(value) < 16 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        sscanf(value, "%4d-%2d-%2d", &year, &month, &day) != 3 || year < 2025 ||
        day < 1 || day > monthDays(year, month)) return false;
    memcpy(out, value, 10); out[10] = '\0';
    return true;
}

inline bool nightIcon(const char* cachedTime, int currentHour) {
    int hour = currentHour;
    if (hour < 0 || hour > 23) {
        char date[11];
        if (!isoDate(cachedTime, date) || sscanf(cachedTime + 11, "%2d", &hour) != 1 ||
            hour < 0 || hour > 23) return false;
    }
    return hour < 6 || hour >= 18;
}

template<class DailyWeather>
bool parseDaily(JsonVariantConst json, DailyWeather& out) {
    Conditions day, night;
    int8_t high, low;
    char date[11];
    if (!conditions(json["daytime"], day) || !conditions(json["nighttime"], night) ||
        !temperature(json["temperatureMax"], high) || !temperature(json["temperatureMin"], low) ||
        high < low || !isoDate(json["daytime"]["forecastStartTime"] | "", date)) return false;
    out.date = date; out.tempMax = high; out.tempMin = low; out.humidity = day.humidity;
    out.iconDay = day.icon; out.textDay = day.text;
    out.iconNight = night.icon; out.textNight = night.text;
    out.wind360Day = day.wind360; out.windDirDay = day.windDir;
    out.windScaleDay = day.windScale; out.windSpeedDay = day.windSpeed;
    out.wind360Night = night.wind360; out.windDirNight = night.windDir;
    out.windScaleNight = night.windScale; out.windSpeedNight = night.windSpeed;
    const char* rise = json["astro"]["sunrise"] | "";
    const char* set = json["astro"]["sunset"] | "";
    char time[6] = {};
    if (strlen(rise) >= 16) memcpy(time, rise + 11, 5);
    out.sunrise = time;
    memset(time, 0, sizeof(time));
    if (strlen(set) >= 16) memcpy(time, set + 11, 5);
    out.sunset = time;
    const char* phase = json["astro"]["moonPhase"] | "";
    const char* phases[] = {"new-moon", "waxing-crescent", "first-quarter", "waxing-gibbous",
                            "full-moon", "waning-gibbous", "last-quarter", "waning-crescent"};
    const char* names[] = {"新月", "蛾眉月", "上弦月", "盈凸月", "满月", "亏凸月", "下弦月", "残月"};
    out.moonPhase = ""; out.moonPhaseIcon = 0;
    for (unsigned i = 0; i < 8; ++i) if (!strcmp(phase, phases[i])) {
        out.moonPhase = names[i]; out.moonPhaseIcon = 800 + i; break;
    }
    return true;
}

inline bool coordinate(const char* value, double low, double high, double& out) {
    if (!value || !*value) return false;
    char* end = nullptr;
    out = strtod(value, &end);
    return !*end && std::isfinite(out) && out >= low && out <= high;
}

// Legacy local settings also accept "longitude,latitude".
inline bool coordinateLocation(const char* location, char* out, size_t size) {
    if (!location) return false;
    const char* comma = strchr(location, ',');
    if (!comma || comma == location || comma - location >= 32 || strchr(comma + 1, ',')) return false;
    char longitude[32] = {};
    memcpy(longitude, location, comma - location);
    double lat, lon;
    if (!coordinate(longitude, -180, 180, lon) || !coordinate(comma + 1, -90, 90, lat)) return false;
    int length = snprintf(out, size, "%.2f/%.2f", lat, lon);
    return length > 0 && static_cast<size_t>(length) < size;
}

template<class Forecast>
bool parseDays(JsonVariantConst json, Forecast& out, const char* retrievedAt) {
    if (!out.weather || !out.length || out.length > 10) return false;
    JsonArrayConst days = json["days"].as<JsonArrayConst>();
    if (days.size() < out.length) return false;
    typedef typename std::remove_reference<decltype(out.weather[0])>::type Day;
    for (unsigned i = 0; i < out.length; ++i) {
        Day checked = {};
        if (!parseDaily(days[i], checked)) return false;
    }
    for (unsigned i = 0; i < out.length; ++i) parseDaily(days[i], out.weather[i]);
    out.updateTime = retrievedAt;
    return true;
}

template<class Forecast>
bool parseHours(JsonVariantConst json, Forecast& out, const char* retrievedAt) {
    if (!out.weather || !out.length || !out.interval) return false;
    const unsigned needed = (out.length - 1u) * out.interval + 1u;
    JsonArrayConst hours = json["hours"].as<JsonArrayConst>();
    if (needed > 240 || hours.size() < needed) return false;
    typedef typename std::remove_reference<decltype(out.weather[0])>::type Hour;
    for (unsigned i = 0; i < out.length; ++i) {
        Hour checked = {};
        JsonVariantConst hour = hours[i * out.interval];
        char date[11];
        if (!parseWeather(hour, checked) || !isoDate(hour["forecastTime"] | "", date)) return false;
    }
    for (unsigned i = 0; i < out.length; ++i) {
        JsonVariantConst hour = hours[i * out.interval];
        parseWeather(hour, out.weather[i]);
        out.weather[i].time = hour["forecastTime"].template as<const char*>();
        out.weather[i].updateTime = retrievedAt;
    }
    return true;
}

inline bool parseLocation(JsonVariantConst json, const char* id, char* out, size_t size) {
    if (strcmp(json["code"] | "", "200")) return false;
    JsonVariantConst city = json["location"][0];
    double lat, lon;
    if (strcmp(city["id"] | "", id) ||
        !coordinate(city["lat"] | "", -90, 90, lat) ||
        !coordinate(city["lon"] | "", -180, 180, lon)) return false;
    int length = snprintf(out, size, "%.2f/%.2f", lat, lon);
    return length > 0 && static_cast<size_t>(length) < size;
}

// V1 has no obsTime/updateTime. Use HTTP Date as retrieval time, never a forecast
// start time as a clock source. The firmware's configured timezone is UTC+8.
inline bool httpDateLocal(const char* value, char* out, size_t size) {
    if (!value) return false;
    char weekday[4], month[4], zone[4];
    int day, year, hour, minute, second;
    if (sscanf(value, "%3s, %d %3s %d %d:%d:%d %3s", weekday, &day, month, &year,
               &hour, &minute, &second, zone) != 8 || strcmp(zone, "GMT")) return false;
    const char* names[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    int m = 0;
    for (int i = 0; i < 12; ++i) if (!strcmp(month, names[i])) m = i + 1;
    if (!m || year < 2025 || day < 1 || day > monthDays(year, m) ||
        hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return false;
    hour += 8;
    if (hour >= 24) {
        hour -= 24;
        if (++day > monthDays(year, m)) { day = 1; if (++m > 12) { m = 1; ++year; } }
    }
    int length = snprintf(out, size, "%04d-%02d-%02dT%02d:%02d:%02d+08:00", year, m, day, hour, minute, second);
    return length > 0 && static_cast<size_t>(length) < size;
}
} // namespace qweather_v1
#endif
