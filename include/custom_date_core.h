#pragma once
#include <ArduinoJson.h>
#include "effective_schedule_core.h"
inline bool customClockFromRecord(JsonVariantConst record,LessonClock& clock,uint16_t& mask) {
    if(strcmp(record["mode"]|"","custom"))return false;
    const auto lessons=record["lessons"].as<JsonArrayConst>();
    if(lessons.isNull()||lessons.size()>12)return false;
    uint16_t occupied=0;LessonClock changed=clock;uint16_t effective=0;
    for(JsonVariantConst row:lessons) {
        const int slot=row["slot"]|0;const char* start=row["start"]|"",*end=row["end"]|"";
        const char* subject=row["subject"]|"";
        if(!row.is<JsonObjectConst>()||!row["slot"].is<int>()||slot<1||slot>12||
           (occupied&(1u<<(slot-1)))||!*subject||strlen(start)!=5||strlen(end)!=5)return false;
        const int first=routineMinute(start),last=routineMinute(end);
        if(first<0||last<=first)return false;
        occupied|=1u<<(slot-1);changed.start[slot-1]=first;changed.end[slot-1]=last;
        if(effectiveSubject(subject))effective|=1u<<(slot-1);
    }
    int last=-1;
    for(int slot=0;slot<12;++slot)if(occupied&(1u<<slot)) {
        if(changed.start[slot]<last)return false;last=changed.end[slot];
    }
    clock=changed;mask=effective;return true;
}
template<class Emit>
void visitCustomTimeline(const EffectiveDay& day,const Emit& emit) {
    static const char* labels[]={"第1节","第2节","第3节","第4节","第5节","第6节","第7节","第8节",
        "晚自习1","晚自习2","晚自习3","晚自习4"};
    for(int slot=0;slot<12;++slot)if(day.mask&(1u<<slot)) {
        emit(day.clock.start[slot],day.clock.end[slot],labels[slot]);
        int next=slot+1;while(next<12&&!(day.mask&(1u<<next)))++next;
        if(next<12&&day.clock.end[slot]<day.clock.start[next]) {
            const char* label=slot/4==next/4?"课间":slot/4==0?"午休":"晚间休息";
            emit(day.clock.end[slot],day.clock.start[next],label);
        }
    }
}
