#include <cassert>
#include <cstring>
#include <cstdio>
#include "calendar_override_core.h"
int main() {
    JsonDocument doc;
    deserializeJson(doc, R"([{"date":"2026-10-01","dayOff":true,"lessons":[]},{"date":"2026-10-02","dayOff":false,"lessons":[{"slot":1,"subject":"英语"}]},{"date":"2026-10-03","dayOff":false,"lessons":[]}])");
    assert(!strcmp(calendarOverrideMark(doc.as<JsonVariantConst>(), "2026-10-01"), "假"));
    assert(!strcmp(calendarOverrideMark(doc.as<JsonVariantConst>(), "2026-10-02"), "课"));
    assert(!strcmp(calendarOverrideMark(doc.as<JsonVariantConst>(), "2026-10-03"), ""));
    assert(!strcmp(calendarOverrideMark(doc.as<JsonVariantConst>(), "2026-11-01"), ""));
    doc[1]["mode"] = "custom"; doc[1]["lessons"][0]["subject"] = "自习";
    assert(!strcmp(calendarOverrideMark(doc.as<JsonVariantConst>(), "2026-10-02"), "课"));
    doc[1]["date"] = "2026-10-04";
    const auto study = calendarDayStyle(calendarOverrideMark(doc.as<JsonVariantConst>(), "2026-10-04"), true, true, 1);
    assert(!study.red && !strcmp(study.mark,"课")); // School overrides both Sunday and national holiday.
    const auto changed = calendarDayStyle("课", true, true, 1);
    assert(!changed.red && !strcmp(changed.mark,"课"));
    const auto off = calendarDayStyle("假", false, false, -1);
    assert(off.red && !strcmp(off.mark,"假"));
    const auto cancelled = calendarDayStyle("课", false, true, 1);
    assert(cancelled.red && !strcmp(cancelled.mark,"假"));
    assert(calendarDayStyle("",false,true,0).red);
    assert(!calendarDayStyle("",false,false,0).red);
    assert(calendarDayStyle("",false,false,1).red);
    assert(!calendarDayStyle("",false,true,-1).red);
    puts("Calendar policy: school class/off overrides public holiday and weekend colors; black class mark passed");
}
