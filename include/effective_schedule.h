#pragma once
#include <Arduino.h>
#include <time.h>
#include "effective_schedule_core.h"
#include "date_overrides.h"

struct EffectiveDayContext {
    EffectiveDay day;
    String routine;
    String courses[12];
    DateOverrideInfo override;
};
EffectiveDayContext effectiveDayForDate(const tm& date);
