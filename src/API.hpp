#ifndef __API_HPP__
#define __API_HPP__

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_http_client.h>

#include <ArduinoUZlib.h> // 解压gzip
#include <_preference.h>
#include <qweather_v1.h>
#include "network_budget.h"

struct Weather {
    String time;
    int8_t temp;
    int8_t humidity;
    int16_t wind360;
    String windDir;
    int8_t windScale;
    uint8_t windSpeed;
    uint16_t icon;
    String text;
    String updateTime;
};

struct DailyWeather {
    String date;
    String sunrise;
    String sunset;
    String moonPhase;
    uint16_t moonPhaseIcon;
    int8_t tempMax;
    int8_t tempMin;
    int8_t humidity;
    uint16_t iconDay;
    String textDay;
    uint16_t iconNight;
    String textNight;
    int16_t wind360Day;
    String windDirDay;
    int8_t windScaleDay;
    uint8_t windSpeedDay;
    int16_t wind360Night;
    String windDirNight;
    int8_t windScaleNight;
    uint8_t windSpeedNight;
};

struct HourlyForecast {
    Weather* weather;
    uint8_t length;
    uint8_t interval;
};

struct DailyForecast {
    DailyWeather* weather;
    uint8_t length;
    String updateTime;
};

struct Hitokoto {
    String sentence;
    String from;
    String from_who;
};

struct Bilibili {
    uint64_t follower;
    uint64_t view;
    uint64_t likes;
};

template<uint8_t MAX_RETRY = 3>
class API {
    using callback = std::function<bool(JsonDocument&)>;
    using precall = std::function<void()>;

private:
    HTTPClient http;
    BudgetSecureClient wifiClient;
    String responseDate;
    String coordinates;
    String coordinateSource;
    bool locationAttempted = false;

    bool getQWeather(const char* host, const char* key, const String& path, callback cb) {
        return getRestfulAPI("https://" + String(host) + path, cb, [this, key]() {
            // Never forward the API key to a redirect target.
            http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
            http.addHeader("X-QW-Api-Key", key);
        });
    }

    bool resolveLocation(const char* host, const char* key, const char* locid) {
        if (!host || !*host || !key || !*key || !locid) return false;
        char direct[32];
        if (qweather_v1::coordinateLocation(locid, direct, sizeof(direct))) {
            coordinates = direct;
            coordinateSource = "";
            locationAttempted = false;
            return true;
        }
        if (strlen(locid) != 9) return false;
        for (unsigned i = 0; i < 9; ++i) if (locid[i] < '0' || locid[i] > '9') return false;
        const String source = String(host) + "/" + locid;
        if (locationAttempted && coordinateSource == source) return !coordinates.isEmpty();
        locationAttempted = true;
        coordinateSource = source;
        coordinates = "";
        Preferences pref;
        pref.begin(PREF_NAMESPACE, true);
        const String saved = pref.getString(PREF_WEATHER_GEO, "");
        pref.end();
        JsonDocument cache;
        char path[32];
        if (!deserializeJson(cache, saved) && String(cache["host"] | "") == host &&
            qweather_v1::parseLocation(cache.as<JsonVariantConst>(), locid, path, sizeof(path))) {
            coordinates = path;
            return true;
        }
        return getQWeather(host, key, "/geo/v2/city/lookup?location=" + String(locid) +
                           "&number=1&lang=zh", [this, host, locid](JsonDocument& json) {
            char path[32];
            if (!qweather_v1::parseLocation(json.as<JsonVariantConst>(), locid, path, sizeof(path)))
                return false;
            coordinates = path;
            // Persist only the selected coordinates, never the key/full GeoAPI response.
            JsonDocument cache;
            cache["host"] = host;
            cache["code"] = "200";
            cache["location"][0]["id"] = locid;
            cache["location"][0]["lat"] = json["location"][0]["lat"];
            cache["location"][0]["lon"] = json["location"][0]["lon"];
            String saved;
            serializeJson(cache, saved);
            Preferences pref;
            if (pref.begin(PREF_NAMESPACE)) {
                if (pref.putString(PREF_WEATHER_GEO, saved) == 0)
                    Serial.println("Weather coordinate cache save failed.");
                pref.end();
            }
            return true;
        });
    }

    bool getRestfulAPI(String url, callback cb, precall pre = precall()) {
        // Serial.printf("Request Url: %s\n", url.c_str());
        JsonDocument doc;

        for (uint8_t i = 0; i < MAX_RETRY; i++) {
            if (!configureNetworkTimeouts(http, wifiClient)) return false;
            bool shouldRetry = false;
            responseDate = "";
            if (http.begin(wifiClient, url)) {
                http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
                if (pre) pre();
                // Serial.printf("Before GET\n");
                int httpCode = http.GET();
                // Serial.printf("GET %d\n", httpCode);
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NOT_MODIFIED) {
                    bool isGzip = false;
                    int headers = http.headers();
                    for (int j = 0; j < headers; j++) {
                        String headerName = http.headerName(j);
                        String headerValue = http.header(j);
                        // Serial.println(headerName + ": " + headerValue);
                        if (headerName.equalsIgnoreCase("Content-Encoding") && headerValue.equalsIgnoreCase("gzip")) {
                            isGzip = true;
                            break;
                        }
                    }
                    String s;
                    if (!readBoundedBody(http, s, 32768)) { http.end(); return false; }
                    DeserializationError error;
                    if (isGzip) {
                        // gzip解压缩
                        uint8_t* outBuf = NULL;
                        uint32_t outLen = 0;
                        ArduinoUZlib::decompress((uint8_t*)s.c_str(), (uint32_t)s.length(), outBuf, outLen);
                        if (!outBuf || outLen > 65536) { free(outBuf); http.end(); return false; }
                        error = deserializeJson(doc, (char*)outBuf, outLen);
                        free(outBuf);
                    } else {
                        error = deserializeJson(doc, s);
                    }

                    if (!error) {
                        char timestamp[32];
                        if (qweather_v1::httpDateLocal(http.header("Date").c_str(), timestamp, sizeof(timestamp)))
                            responseDate = timestamp;
                        wifiClient.flush();
                        http.end();
                        return cb(doc);
                    } else {
                        Serial.print(F("Parse JSON failed, error: "));
                        Serial.println(error.c_str());
                        shouldRetry = error == DeserializationError::IncompleteInput;
                    }
                } else {
                    Serial.print(F("Get failed, error: "));
                    if (httpCode < 0) {
                        Serial.println(http.errorToString(httpCode));
                        shouldRetry = httpCode == HTTPC_ERROR_CONNECTION_REFUSED || httpCode == HTTPC_ERROR_CONNECTION_LOST || httpCode == HTTPC_ERROR_READ_TIMEOUT;
                    } else {
                        Serial.println(httpCode);
                    }
                }
                wifiClient.flush();
                http.end();
            } else {
                Serial.println(F("Unable to connect"));
            }
            if (!shouldRetry) break;
            if (!wakeNetworkBudget().canRequest(millis() + 5000)) break;
            Serial.println(F("Retry after 5 seconds"));
            delay(5000);
        }
        return false;
    }

public:
    API() {
        // http.setTimeout(10000);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        const char* headerKeys[] = {"Content-Encoding", "Date"};
        http.collectHeaders(headerKeys, 2);

        wifiClient.setInsecure();
    }

    ~API() {}

    // 获取 HTTPClient
    HTTPClient& httpClient() {
        return http;
    }

    // V1 requires latitude/longitude; keep the existing Location ID setting.
    bool getWeatherNow(Weather& result, const char* host, const char* key, const char* locid) {
        if (!resolveLocation(host, key, locid)) return false;
        return getQWeather(host, key, "/weather/v1/current/" + coordinates +
                           "?localTime=true&lang=zh", [&result, this](JsonDocument& json) {
            Weather fetched = {};
            if (!qweather_v1::parseWeather(json.as<JsonVariantConst>(), fetched)) return false;
            // V1 does not expose observation time. This is retrieval time only.
            fetched.updateTime = responseDate;
            result = fetched;
            return true;
        });
    }

    bool getForecastHourly(HourlyForecast& result, const char* host, const char* key, const char* locid) {
        if (!result.weather || !result.length || !result.interval ||
            !resolveLocation(host, key, locid)) return false;
        const unsigned needed = (result.length - 1u) * result.interval + 1u;
        if (needed > 240) return false;
        return getQWeather(host, key, "/weather/v1/hourly/" + coordinates +
                           "?localTime=true&lang=zh&hours=" + String(needed),
                           [&result, this](JsonDocument& json) {
            return qweather_v1::parseHours(json.as<JsonVariantConst>(), result, responseDate.c_str());
        });
    }

    bool getForecastDaily(DailyForecast& result, const char* host, const char* key, const char* locid) {
        if (!result.weather || !result.length || result.length > 10 ||
            !resolveLocation(host, key, locid)) return false;
        return getQWeather(host, key, "/weather/v1/daily/" + coordinates +
                           "?localTime=true&lang=zh&days=" + String(result.length),
                           [&result, this](JsonDocument& json) {
            return qweather_v1::parseDays(json.as<JsonVariantConst>(), result, responseDate.c_str());
        });
    }
    

    // 一言: https://developer.hitokoto.cn/sentence/
    bool getHitokoto(Hitokoto& result) {
        return getRestfulAPI("https://v1.hitokoto.cn/?max_length=15", [&result](JsonDocument& json) {
            result.sentence = json["hitokoto"].as<const char*>();
            result.from = json["from"].as<const char*>();
            result.from_who = json["from_who"].as<const char*>();
            return true;
            });
    }

    // B站粉丝
    bool getFollower(Bilibili& result, uint32_t uid) {
        return getRestfulAPI("https://api.bilibili.com/x/relation/stat?vmid=" + String(uid), [&result](JsonDocument& json) {
            if (json["code"] != 0) {
                Serial.print(F("Get bilibili follower failed, error: "));
                Serial.println(json["message"].as<const char*>());
                return false;
            }
            result.follower = json["data"]["follower"];
            return true;
            });
    }

    // B站总播放量和点赞数
    bool getLikes(Bilibili& result, uint32_t uid, const char* cookie) {
        return getRestfulAPI(
            "https://api.bilibili.com/x/space/upstat?mid=" + String(uid), [&result](JsonDocument& json) {
                if (json["code"] != 0) {
                    Serial.print(F("Get bilibili likes failed, error: "));
                    Serial.println(json["message"].as<const char*>());
                    return false;
                }
                result.view = json["data"]["archive"]["view"];
                result.likes = json["data"]["likes"];
                return true;
            },
            [this, &cookie]() {
                http.addHeader("Cookie", String("SESSDATA=") + cookie + ";");
            });
    }
};
#endif  // __API_HPP__
