#pragma once
#include <ArduinoJson.h>
#include <cstring>
#include <cstdlib>
#include "lesson_clock_core.h"
#include "custom_date_core.h"
namespace remote_contract {
inline bool textRange(const char* value, size_t bytes, unsigned maxChars, size_t maxBytes) {
    if (!value || !bytes || bytes > maxBytes) return false;
    unsigned count = 0; bool nonSpace = false;
    for (size_t i = 0; i < bytes;) {
        const unsigned char c = value[i];
        unsigned cp = c; size_t length = 1;
        if (c < 0x20 || c == 0x7f) return false;
        if (c >= 0xc2 && c <= 0xdf) { length = 2; cp = c & 0x1f; }
        else if (c >= 0xe0 && c <= 0xef) { length = 3; cp = c & 0xf; }
        else if (c >= 0x80) return false; // Display font supports BMP, no emoji.
        if (i + length > bytes) return false;
        for (size_t j = 1; j < length; ++j) {
            const unsigned char tail = value[i+j];
            if ((tail & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (tail & 0x3f);
        }
        if ((length == 2 && cp < 0x80) || (length == 3 && cp < 0x800) ||
            (cp >= 0xd800 && cp <= 0xdfff)) return false;
        nonSpace |= cp != 0x20 && cp != 0x3000;
        i += length; if (++count > maxChars) return false;
    }
    return nonSpace;
}
inline bool text(const char* value, unsigned maxChars, size_t maxBytes) {
    return value && textRange(value, strlen(value), maxChars, maxBytes);
}
inline bool validDate(const char* value, bool compact) {
    if (!value || strlen(value) != (compact ? 8u : 10u)) return false;
    for (size_t i = 0; value[i]; ++i) {
        if (!compact && (i == 4 || i == 7)) { if (value[i] != '-') return false; }
        else if (value[i] < '0' || value[i] > '9') return false;
    }
    const int year = (value[0]-'0')*1000 + (value[1]-'0')*100 + (value[2]-'0')*10 + value[3]-'0';
    const int m = compact ? 4 : 5, d = compact ? 6 : 8;
    const int month = (value[m]-'0')*10 + value[m+1]-'0';
    const int day = (value[d]-'0')*10 + value[d+1]-'0';
    const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return year >= 2025 && year <= 2099 && month >= 1 && month <= 12 && day >= 1 &&
        day <= days[month-1] + (month == 2 && year % 4 == 0 && (year % 100 || year % 400 == 0));
}
inline bool schedule(const char* value) {
    if (!value || strlen(value) > 4000 || strncmp(value, "444;", 4)) return false;
    static const char* days[] = {"一","二","三","四","五","六"};
    const char* cursor = value + 4;
    for (int i = 0; i < 6; ++i) {
        const char* end = strchr(cursor, ';'); if (!end) end = cursor + strlen(cursor);
        const char* comma = static_cast<const char*>(memchr(cursor, ',', end-cursor));
        if (!comma || size_t(comma-cursor) != strlen(days[i]) || memcmp(cursor,days[i],comma-cursor)) return false;
        cursor = comma+1;
        for (int j = 0; j < 12; ++j) {
            const char* next = static_cast<const char*>(memchr(cursor, ',', end-cursor));
            if (j < 11 && !next) return false;
            if (j == 11 && next) return false;
            if (!next) next = end;
            if (!textRange(cursor,next-cursor,12,48)) return false;
            cursor = next < end ? next+1 : end;
        }
        cursor = *end ? end+1 : end;
    }
    return !*cursor;
}
inline bool routine(const char* value) {
    if (!value || strlen(value) > 4000) return false;
    LessonClock clock; if (!parseLessonClock(value,clock)) return false;
    unsigned entries = 0;
    for (const char* cursor = value; *cursor;) {
        const char* end = strchr(cursor,';'); if (!end) end = cursor+strlen(cursor);
        if (++entries > 64 || end-cursor < 13 || cursor[11] != ',' ||
            !textRange(cursor+12,end-cursor-12,16,64)) return false;
        cursor = *end ? end+1 : end;
    }
    return entries > 0;
}
inline bool reading(const char* value) {
    if (!value || strlen(value) > 300) return false;
    static const char* days[] = {"一","二","三","四","五","六"};
    const char* cursor = value;
    for (int i = 0; i < 6; ++i) {
        const char* end = strchr(cursor,';'); if (!end) end = cursor+strlen(cursor);
        const char* colon = static_cast<const char*>(memchr(cursor,':',end-cursor));
        if (!colon || size_t(colon-cursor) != strlen(days[i]) || memcmp(cursor,days[i],colon-cursor) ||
            memchr(colon+1,':',end-colon-1) || !textRange(colon+1,end-colon-1,12,48)) return false;
        cursor = *end ? end+1 : end;
    }
    return !*cursor;
}
inline bool validPayload(JsonVariantConst doc) {
    if ((doc["schema"] != 1 && doc["schema"] != 2) || !doc["revision"].is<uint32_t>() || measureJson(doc) > 11500) return false;
    const auto settings = doc["settings"].as<JsonObjectConst>();
    if (settings.isNull() || !text(settings["countdownLabel"] | "",8,24) ||
        !validDate(settings["countdownDate"] | "",true) ||
        !schedule(settings["studySchedule"] | "") || !routine(settings["dailyRoutine"] | "") ||
        !reading(settings["earlyReading"] | "") || !settings["defaultPage"].is<int>() ||
        settings["defaultPage"].as<int>() < 0 || settings["defaultPage"].as<int>() > 3) return false;
    const char* location = settings["weatherLocation"] | "";
    if (strlen(location) != 9) return false;
    for (int i = 0; i < 9; ++i) if (location[i] < '0' || location[i] > '9') return false;
    const auto overrides = settings["dateOverrides"].as<JsonArrayConst>();
    if (!settings["dateOverrides"].isNull() && (overrides.isNull() || overrides.size() > 14 ||
        measureJson(overrides) > 3500)) return false;
    const char* dates[14] = {}; unsigned count = 0;
    for (JsonVariantConst entry : overrides) {
        if (!entry.is<JsonObjectConst>() || !validDate(entry["date"] | "",false) || !entry["dayOff"].is<bool>()) return false;
        const bool custom = entry["mode"] == "custom";
        if ((!entry["mode"].isNull() && !custom) || (custom && doc["schema"] != 2)) return false;
        for (JsonPairConst field : entry.as<JsonObjectConst>()) {
            const char* key = field.key().c_str();
            if (strcmp(key,"date") && strcmp(key,"dayOff") && strcmp(key,"lessons") && strcmp(key,"mode")) return false;
        }
        const char* date = entry["date"];
        for (unsigned i = 0; i < count; ++i) if (!strcmp(dates[i],date)) return false;
        dates[count++] = date;
        const auto lessons = entry["lessons"].as<JsonArrayConst>();
        if (lessons.isNull() || lessons.size() > 12 || (entry["dayOff"] == true && lessons.size())) return false;
        if (custom) {
            LessonClock clock = defaultLessonClock(); uint16_t mask = 0;
            if (!customClockFromRecord(entry,clock,mask)) return false;
        }
        uint16_t slots = 0;
        for (JsonVariantConst lesson : lessons) {
            if (!lesson.is<JsonObjectConst>() || !lesson["slot"].is<int>()) return false;
            for (JsonPairConst field : lesson.as<JsonObjectConst>()) {
                const char* key = field.key().c_str();
                if (strcmp(key,"slot") && strcmp(key,"subject") &&
                    (!custom || (strcmp(key,"start") && strcmp(key,"end")))) return false;
            }
            const int slot = lesson["slot"];
            const char* subject = lesson["subject"] | "";
            if (slot < 1 || slot > 12 || (slots & (1u << slot)) || !text(subject,12,48) ||
                strchr(subject,';') || strchr(subject,',')) return false;
            slots |= 1u << slot;
        }
    }
    const auto todos = doc["todos"].as<JsonArrayConst>();
    if (todos.isNull() || todos.size() > 6 || !doc["pendingCount"].is<int>() ||
        doc["pendingCount"].as<int>() < int(todos.size()) || doc["pendingCount"].as<int>() > 30) return false;
    for (JsonVariantConst todo : todos) {
        const char* due = todo["due"] | "";
        const char* priority = todo["priority"] | "";
        if (!todo.is<JsonObjectConst>() || !text(todo["title"] | "",16,48) ||
            !todo["due"].is<const char*>() || (*due && !validDate(due,false)) ||
            (strcmp(priority,"normal") && strcmp(priority,"high")) ||
            !todo["done"].is<bool>() || todo["done"] != false) return false;
    }
    return true;
}
}
