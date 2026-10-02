#pragma once
#include "school_schedule_data.h"
static const char DEFAULT_REMOTE_HOST[] = "";
// Initialize only absent UI preference; never overwrite user schedule, goals or weather.
template<class PreferencesLike> void initializePublicPreferences(PreferencesLike& pref) {
    if (!pref.isKey("SI_TYPE")) pref.putInt("SI_TYPE", 2);
}
inline const char* unconfiguredDayLabel(bool configured) {
    return configured ? nullptr : "课表待配置";
}
