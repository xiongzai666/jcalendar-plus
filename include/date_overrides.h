#ifndef JC_DATE_OVERRIDES_H
#define JC_DATE_OVERRIDES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>
#include "_preference.h"
#include "custom_date_core.h"
#include "preference_string.h"

struct DateOverrideInfo {
    bool found = false;
    bool dayOff = false;
    bool hasLessons = false;
    bool lateLessons = false;
    bool independent = false;
};

inline DateOverrideInfo readDateOverride(const tm& local, String* courses = nullptr, LessonClock* clock = nullptr) {
    DateOverrideInfo info;
    if (local.tm_year + 1900 < 2025) return info;
    char date[11];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", local.tm_year + 1900,
             local.tm_mon + 1, local.tm_mday);
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE, true)) return info;
    const String json = preferenceString(pref, PREF_DATE_OVERRIDES, "[]", 3500);
    pref.end();
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArray>()) return info;
    for (JsonObject day : doc.as<JsonArray>()) {
        if (day["date"].as<String>() != date) continue;
        info.found = true;
        info.dayOff = day["dayOff"].as<bool>();
        info.independent=!strcmp(day["mode"]|"","custom");
        if(info.independent) {
            if(courses)for(int i=0;i<12;++i)courses[i]="";
            uint16_t mask=0;
            if(clock&&!customClockFromRecord(day,*clock,mask))return info;
        }
        if (info.dayOff) return info;
        for (JsonObject lesson : day["lessons"].as<JsonArray>()) {
            const int slot = lesson["slot"].as<int>();
            if (slot < 1 || slot > 12) continue;
            info.hasLessons = true;
            if (slot > 6) info.lateLessons = true;
            if (courses) { courses[slot - 1] = lesson["subject"].as<String>();courses[slot-1].trim(); }
        }
        return info;
    }
    return info;
}

#endif
