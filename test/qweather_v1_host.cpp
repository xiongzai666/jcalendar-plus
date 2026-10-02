#include <cassert>
#include <string>
#include <cstdio>
#include <cstdlib>
#include "qweather_v1.h"

struct Weather {
    std::string time, text, windDir, updateTime;
    int8_t temp = 0, humidity = 0, windScale = 0;
    int16_t wind360 = 0;
    uint8_t windSpeed = 0;
    uint16_t icon = 0;
};
struct DailyWeather {
    std::string date, sunrise, sunset, moonPhase, textDay, textNight, windDirDay, windDirNight;
    int8_t tempMax = 0, tempMin = 0, humidity = 0, windScaleDay = 0, windScaleNight = 0;
    uint16_t moonPhaseIcon = 0, iconDay = 0, iconNight = 0;
    int16_t wind360Day = 0, wind360Night = 0;
    uint8_t windSpeedDay = 0, windSpeedNight = 0;
};
struct HourlyForecast { Weather* weather; uint8_t length, interval; };
struct DailyForecast { DailyWeather* weather; uint8_t length; std::string updateTime; };

int main() {
    JsonDocument doc;
    assert(!deserializeJson(doc, R"({"condition":{"text":"小雨","code":"305"},"temperature":{"value":-2.7,"unit":"°C"},"humidity":0.69,"wind":{"direction":{"degree":226,"compass":"sw"},"speed":{"value":4.74,"unit":"m/s"},"scale":3}})"));
    Weather current;
    assert(qweather_v1::parseWeather(doc.as<JsonVariantConst>(), current));
    assert(current.temp == -3 && current.humidity == 69 && current.windSpeed == 17);
    assert(current.windDir == "西南风" && current.icon == 305);
    doc["humidity"] = 0;
    doc["temperature"]["value"] = 0;
    doc["wind"]["direction"]["compass"] = "none";
    assert(qweather_v1::parseWeather(doc.as<JsonVariantConst>(), current));
    assert(current.humidity == 0 && current.temp == 0 && current.windDir == "无风");
    doc["humidity"] = 69;
    assert(!qweather_v1::parseWeather(doc.as<JsonVariantConst>(), current));
    assert(current.humidity == 0); // Failed responses must not overwrite cached fields.
    doc["humidity"] = 0.8;
    doc["temperature"]["unit"] = "°F";
    doc["temperature"]["value"] = 32;
    doc["wind"]["speed"]["unit"] = "km/h";
    doc["wind"]["speed"]["value"] = 20;
    assert(qweather_v1::parseWeather(doc.as<JsonVariantConst>(), current));
    assert(current.temp == 0 && current.windSpeed == 20);
    doc["wind"]["speed"]["unit"] = "invalid";
    assert(!qweather_v1::parseWeather(doc.as<JsonVariantConst>(), current));
    doc["wind"]["speed"]["unit"] = "m/s";
    doc["condition"].remove("code");
    assert(!qweather_v1::parseWeather(doc.as<JsonVariantConst>(), current));

    assert(!deserializeJson(doc, R"({"forecastStartTime":"2026-10-01T06:00+08:00","temperatureMax":{"value":31.4,"unit":"°C"},"temperatureMin":{"value":22.6,"unit":"°C"},"astro":{"sunrise":"2026-10-01T06:11+08:00","sunset":"2026-10-01T17:50+08:00","moonPhase":"waning-gibbous"},"daytime":{"forecastStartTime":"2026-10-01T07:00+08:00","condition":{"text":"多云","code":"101"},"humidity":0.56,"wind":{"direction":{"degree":45,"compass":"ne"},"speed":{"value":2,"unit":"m/s"},"scale":2}},"nighttime":{"forecastStartTime":"2026-10-01T19:00+08:00","condition":{"text":"中雨","code":"306"},"humidity":0.9,"wind":{"direction":{"degree":0,"compass":"n"},"speed":{"value":3,"unit":"m/s"},"scale":2}}})"));
    DailyWeather daily;
    assert(qweather_v1::parseDaily(doc.as<JsonVariantConst>(), daily));
    assert(daily.date == "2026-10-01" && daily.tempMax == 31 && daily.tempMin == 23);
    assert(daily.humidity == 56 && daily.textNight == "中雨" && daily.iconNight == 306);
    assert(daily.windDirDay == "东北风" && daily.sunrise == "06:11");
    doc.remove("nighttime");
    assert(!qweather_v1::parseDaily(doc.as<JsonVariantConst>(), daily));
    assert(daily.textNight == "中雨");

    JsonDocument complete;
    assert(!deserializeJson(complete, R"({"forecastStartTime":"2026-10-01T06:00+08:00","temperatureMax":{"value":31.4,"unit":"°C"},"temperatureMin":{"value":22.6,"unit":"°C"},"daytime":{"forecastStartTime":"2026-10-01T07:00+08:00","condition":{"text":"多云","code":"101"},"humidity":0.56,"wind":{"direction":{"degree":45,"compass":"ne"},"speed":{"value":2,"unit":"m/s"},"scale":2}},"nighttime":{"condition":{"text":"中雨","code":"306"},"humidity":0.9,"wind":{"direction":{"degree":0,"compass":"n"},"speed":{"value":3,"unit":"m/s"},"scale":2}}})"));
    doc.clear();
    doc["days"][0].set(complete);
    doc["days"][1].set(complete);
    doc["days"][1]["daytime"]["forecastStartTime"] = "2026-10-02T07:00+08:00";
    DailyWeather days[2];
    DailyForecast df = {days, 2, ""};
    assert(qweather_v1::parseDays(doc.as<JsonVariantConst>(), df, "2026-10-01T10:00+08:00"));
    assert(days[1].date == "2026-10-02" && df.length == 2 && !df.updateTime.empty());
    doc["days"][0]["daytime"]["condition"]["text"] = "晴";
    doc["days"][1].remove("nighttime");
    assert(!qweather_v1::parseDays(doc.as<JsonVariantConst>(), df, ""));
    assert(days[0].textDay == "多云"); // No partially updated forecast.
    doc["days"].as<JsonArray>().remove(1);
    assert(!qweather_v1::parseDays(doc.as<JsonVariantConst>(), df, ""));

    doc.clear();
    for (int i = 0; i < 3; ++i) {
        doc["hours"][i].set(complete["daytime"]);
        doc["hours"][i]["forecastTime"] = i == 2 ? "2026-10-02T01:00+08:00" : "2026-10-01T23:00+08:00";
        doc["hours"][i]["temperature"]["value"] = 20 + i;
        doc["hours"][i]["temperature"]["unit"] = "°C";
    }
    Weather hours[2];
    HourlyForecast hf = {hours, 2, 2};
    assert(qweather_v1::parseHours(doc.as<JsonVariantConst>(), hf, ""));
    assert(hours[0].temp == 20 && hours[1].temp == 22 && hours[1].time == "2026-10-02T01:00+08:00");
    doc["hours"][0]["temperature"]["value"] = 10;
    doc["hours"][2].remove("condition");
    assert(!qweather_v1::parseHours(doc.as<JsonVariantConst>(), hf, ""));
    assert(hours[0].temp == 20);
    hf.interval = 0;
    assert(!qweather_v1::parseHours(doc.as<JsonVariantConst>(), hf, ""));

    assert(!deserializeJson(doc, R"({"code":"200","location":[{"id":"101220609","lat":"31.27","lon":"118.36"}]})"));
    char coordinates[32] = {};
    assert(qweather_v1::coordinateLocation("118.365,31.274", coordinates, sizeof(coordinates)));
    assert(std::string(coordinates) == "31.27/118.36");
    assert(!qweather_v1::coordinateLocation("181,31", coordinates, sizeof(coordinates)));
    assert(!qweather_v1::coordinateLocation("118,31,22", coordinates, sizeof(coordinates)));
    assert(qweather_v1::parseLocation(doc.as<JsonVariantConst>(), "101220609", coordinates, sizeof(coordinates)));
    assert(std::string(coordinates) == "31.27/118.36"); // V1 uses latitude FIRST.
    assert(!qweather_v1::parseLocation(doc.as<JsonVariantConst>(), "101010100", coordinates, sizeof(coordinates)));
    doc["location"][0]["lat"] = "91";
    assert(!qweather_v1::parseLocation(doc.as<JsonVariantConst>(), "101220609", coordinates, sizeof(coordinates)));
    doc["location"][0]["lat"] = "31oops";
    assert(!qweather_v1::parseLocation(doc.as<JsonVariantConst>(), "101220609", coordinates, sizeof(coordinates)));

#ifdef _WIN32
    _putenv_s("TZ", "CST-8"); _tzset();
#else
    setenv("TZ", "CST-8", 1); tzset();
#endif
    char timestamp[32];
    assert(qweather_v1::httpDateLocal("Thu, 01 Oct 2026 20:59:03 GMT", timestamp, sizeof(timestamp)));
    assert(std::string(timestamp) == "2026-10-02T04:59:03+08:00");
    assert(!qweather_v1::httpDateLocal("", timestamp, sizeof(timestamp)));
    assert(!qweather_v1::httpDateLocal("Thu, 40 Oct 2026 20:59:03 GMT", timestamp, sizeof(timestamp)));
    assert(!qweather_v1::nightIcon("", 12)); // V1 has no observation timestamp.
    assert(qweather_v1::nightIcon("", 20));
    assert(qweather_v1::nightIcon("2026-10-01T12:00+08:00", 20)); // Offline cached data, current night.
    assert(!qweather_v1::nightIcon("2026-10-01T20:00+08:00", 12));
    assert(qweather_v1::nightIcon("2026-10-01T20:00+08:00", -1));
    assert(!qweather_v1::nightIcon("", -1));
    puts("QWeather V1 normalization, day/night, location and timestamp tests passed.");
}
