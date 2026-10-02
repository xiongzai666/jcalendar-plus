#include "effective_schedule.h"
#include <Preferences.h>
#include "_preference.h"
#include "school_schedule_data.h"
#include "preference_string.h"

EffectiveDayContext effectiveDayForDate(const tm& date) {
    EffectiveDayContext context;
    Preferences pref;
    pref.begin(PREF_NAMESPACE, true);
    const String schedule = preferenceString(pref, PREF_STUDY_SCHEDULE, DEFAULT_STUDY_SCHEDULE, 4000);
    context.routine = preferenceString(pref, PREF_DAILY_ROUTINE, DEFAULT_DAILY_ROUTINE, 4000);
    pref.end();
    LessonClock clock;
    if (!parseLessonClock(context.routine.c_str(), clock)) {
        context.routine = DEFAULT_DAILY_ROUTINE;
        clock = defaultLessonClock();
    }
    static const char* const days[] = {"日", "一", "二", "三", "四", "五", "六"};
    if (date.tm_wday >= 1 && date.tm_wday <= 6) {
        const String marker = String(";") + days[date.tm_wday] + ",";
        const int begin = schedule.indexOf(marker);
        if (begin >= 0) {
            int field = begin + marker.length();
            int recordEnd = schedule.indexOf(';', field);
            if (recordEnd < 0) recordEnd = schedule.length();
            for (int i = 0; i < 12 && field < recordEnd; ++i) {
                int end = schedule.indexOf(',', field);
                if (end < 0 || end > recordEnd) end = recordEnd;
                context.courses[i] = schedule.substring(field, end);
                context.courses[i].trim();
                field = end + 1;
            }
        }
    }
    context.override = readDateOverride(date, context.courses, &clock);
    const char* values[12];
    for (int i = 0; i < 12; ++i) values[i] = context.courses[i].c_str();
    context.day = makeEffectiveDay(clock, values, context.override.dayOff);
    context.day.independent=context.override.independent;
    if(context.day.independent) {
        context.routine="";
        visitCustomTimeline(context.day,[&](int start,int end,const char* label) {
            char times[16];snprintf(times,sizeof(times),"%02d:%02d-%02d:%02d,",start/60,start%60,end/60,end%60);
            context.routine+=times;context.routine+=label;context.routine+=';';
        });
    }
    return context;
}
