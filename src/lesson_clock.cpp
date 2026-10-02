#include "lesson_clock.h"

#include <Preferences.h>
#include "_preference.h"
#include "school_schedule_data.h"
#include "preference_string.h"

LessonClock getLessonClock() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE, true)) return defaultLessonClock();
    const String routine = preferenceString(pref, PREF_DAILY_ROUTINE, DEFAULT_DAILY_ROUTINE, 4000);
    pref.end();
    LessonClock clock;
    if (parseLessonClock(routine.c_str(), clock)) return clock;
    return defaultLessonClock();
}
