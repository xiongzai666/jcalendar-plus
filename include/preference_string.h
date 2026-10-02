#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "bounded_string_core.h"

inline String preferenceString(Preferences& pref, const char* key,
                               const char* fallback, size_t maximumBytes) {
    if (!pref.isKey(key)) return String(fallback);
    String value;
    const bool loaded = readBoundedString(maximumBytes,
        [&](char* buffer, size_t capacity) { return pref.getString(key, buffer, capacity); },
        [&](const char* buffer, size_t) { value = buffer; });
    return loaded ? value : String(fallback);
}
