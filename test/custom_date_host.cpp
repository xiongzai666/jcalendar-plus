#include <cassert>
#include <cstdio>
#include <string>
#include <ArduinoJson.h>
#include "custom_date_core.h"
#include "routine_fixture.h"
int main() {
    JsonDocument record;
    deserializeJson(record,R"({"date":"2026-10-04","dayOff":false,"mode":"custom","lessons":[{"slot":1,"subject":"自习","start":"08:00","end":"09:30"},{"slot":2,"subject":"自习","start":"09:45","end":"11:30"},{"slot":5,"subject":"自习","start":"14:00","end":"15:30"},{"slot":6,"subject":"自习","start":"15:45","end":"17:00"},{"slot":9,"subject":"自习","start":"18:10","end":"20:10"},{"slot":10,"subject":"自习","start":"20:20","end":"22:00"}]})");
    LessonClock clock;assert(parseLessonClock(TEST_DAILY_ROUTINE,clock));
    uint16_t mask=0;assert(customClockFromRecord(record.as<JsonVariantConst>(),clock,mask));
    assert(mask==0x333&&clock.start[0]==480&&clock.end[1]==690&&clock.start[8]==1090&&clock.end[9]==1320);
    EffectiveDay day={clock,mask,true};std::string routine;
    visitCustomTimeline(day,[&](int start,int end,const char* label) {
        char times[32];snprintf(times,sizeof(times),"%02d:%02d-%02d:%02d,",start/60,start%60,end/60,end%60);
        routine+=times;routine+=label;routine+=';';
    });
    RoutineSelection next={};
    assert(selectEffectiveActivity(day,routine.c_str(),540,0,next));
    assert(next.minute==570&&routineLabelIs(next.label,next.labelLength,"课间"));
    assert(selectEffectiveActivity(day,routine.c_str(),680,0,next));
    assert(next.minute==690&&routineLabelIs(next.label,next.labelLength,"午休"));
    assert(selectEffectiveActivity(day,routine.c_str(),780,0,next)&&next.minute==840&&next.lesson==4);
    assert(nextEffectiveBoundary(day,routine.c_str(),9*3600,0)==9*3600+30*60);
    assert(nextEffectiveBoundary(day,routine.c_str(),11*3600+30*60,0)==12*3600);
    assert(effectiveDismissal(day,0)==690&&effectiveDismissal(day,1)==1020&&effectiveDismissal(day,2)==1320);
    assert(effectiveHintVisible(day,681)&&!effectiveHintVisible(day,690));
    assert(effectiveHintVisible(day,1011)&&effectiveHintVisible(day,1311));
    assert(selectEffectiveActivity(day,routine.c_str(),1290,0,next)&&next.minute==1320&&next.kind==RoutineSelectionKind::Dismissal);
    assert(!effectiveHasRemaining(day,1320));
    day.clock.end[1]=750;assert(effectiveSectionAt(day,735)==0);
    record["lessons"][1]["start"]="09:15";
    assert(!customClockFromRecord(record.as<JsonVariantConst>(),clock,mask));
    puts("Custom date: six self-study periods, gaps, Sunday, exact dismissal and noon boundary passed");
}
