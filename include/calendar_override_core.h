#pragma once
#include <ArduinoJson.h>
#include <cstring>
struct CalendarDayStyle { const char* mark; bool red; };
inline CalendarDayStyle calendarDayStyle(const char* schoolMark, bool hasCourses,
                                         bool weekend, int publicHoliday) {
    if (schoolMark && *schoolMark) {
        if (!strcmp(schoolMark,"假") || !hasCourses) return {"假",true};
        return {schoolMark,false};
    }
    if (publicHoliday > 0) return {"休",true};
    if (publicHoliday < 0) return {"班",false};
    return {"",weekend};
}
inline const char* calendarOverrideMark(JsonVariantConst records, const char* date) {
    for (JsonObjectConst day : records.as<JsonArrayConst>()) {
        if (strcmp(day["date"] | "", date)) continue;
        if (day["dayOff"] == true) return "假";
        if (day["mode"] == "custom") return "课";
        return day["lessons"].as<JsonArrayConst>().size() ? "课" : "";
    }
    return "";
}
