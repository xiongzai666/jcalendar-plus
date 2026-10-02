#pragma once
#include "lesson_clock_core.h"
#include "schedule_optimization.h"

struct EffectiveDay {
    LessonClock clock;
    uint16_t mask;
    bool independent;
};

inline bool effectiveSubject(const char* value) {
    return value && *value && strcmp(value, "放假") && strcmp(value, "无课") &&
           strcmp(value, "待配置") && strcmp(value, "—");
}
inline EffectiveDay makeEffectiveDay(const LessonClock& clock,
                                      const char* const subjects[12], bool dayOff) {
    EffectiveDay day = {clock, 0, false};
    if (!dayOff) for (int i = 0; i < 12; ++i)
        if (effectiveSubject(subjects[i])) day.mask |= static_cast<uint16_t>(1u << i);
    return day;
}
inline int effectiveDismissal(const EffectiveDay& day, int section) {
    if (section < 0 || section > 2) return -1;
    for (int i = section * 4 + 3; i >= section * 4; --i)
        if (day.mask & (1u << i)) return day.clock.end[i];
    return -1;
}
inline bool effectiveHasRemaining(const EffectiveDay& day, int minute) {
    for (int i = 0; i < 12; ++i)
        if ((day.mask & (1u << i)) && minute < day.clock.end[i]) return true;
    return false;
}
inline int effectiveActiveLesson(const EffectiveDay& day, int minute) {
    for (int i = 0; i < 12; ++i)
        if ((day.mask & (1u << i)) && minute >= day.clock.start[i] && minute < day.clock.end[i]) return i;
    return -1;
}
inline int effectiveSectionAt(const EffectiveDay& day, int minute) {
    const int active=effectiveActiveLesson(day,minute);
    if(day.independent&&active>=0)return active/4;
    const int section = lessonSectionAt(day.clock, minute);
    if (day.mask & (0xfu << (section * 4))) return section;
    for (int group = section + 1; group < 3; ++group)
        if (day.mask & (0xfu << (group * 4))) return group;
    for (int group = section - 1; group >= 0; --group)
        if (day.mask & (0xfu << (group * 4))) return group;
    return 0;
}
inline bool effectiveHintVisible(const EffectiveDay& day, int minute) {
    for (int group = 0; group < 3; ++group) {
        const int finish = effectiveDismissal(day, group);
        if (finish >= 0 && minute >= finish - 12 && minute < finish) return true;
    }
    return false;
}
inline bool routineLabelIs(const char* label, size_t length, const char* expected) {
    return length == strlen(expected) && !memcmp(label, expected, length);
}
inline bool effectiveRoutineEnabled(const EffectiveDay& day, int start,
                                     const char* label, size_t length, int weekday) {
    const int lesson = lessonLabelIndex(label, length);
    if (lesson >= 0) return day.mask & (1u << lesson);
    if(day.independent)return routineLabelIs(label,length,"课间")||routineLabelIs(label,length,"午休")||
        routineLabelIs(label,length,"晚间休息")||routineLabelIs(label,length,"休息");
    if (weekday == 0 || routineLabelIs(label, length, "中午放学") ||
        routineLabelIs(label, length, "下午放学") || routineLabelIs(label, length, "放学")) return false;
    const int section = lessonSectionAt(day.clock, start);
    const int finish = effectiveDismissal(day, section);
    if (finish < 0 || start >= finish) return false;
    if (routineLabelIs(label, length, "课间")) {
        bool later = false, earlier = start < day.clock.start[section * 4];
        for (int i = section * 4; i < section * 4 + 4; ++i) {
            if (!(day.mask & (1u << i))) continue;
            if (day.clock.start[i] > start) later = true;
            if (day.clock.end[i] == start) earlier = true;
        }
        return earlier && later;
    }
    return true;
}

enum class RoutineSelectionKind { Next, EarlyReading, Dismissal };
struct RoutineSelection {
    RoutineSelectionKind kind;
    int minute;
    int lesson;
    int section;
    const char* label;
    size_t labelLength;
};
inline bool selectEffectiveActivity(const EffectiveDay& day, const char* routine,
                                    int minute, int weekday, RoutineSelection& result) {
    if (!day.mask) return false;
    bool found = false;
    for (int group = 0; group < 3; ++group) {
        const int finish = effectiveDismissal(day, group);
        if (finish > minute && (!found || finish < result.minute)) {
            result = {RoutineSelectionKind::Dismissal, finish, -1, group, nullptr, 0};
            found = true;
        }
    }
    if (!routine) return found;
    for (const char* cursor = routine; *cursor;) {
        const char* end = strchr(cursor, ';');
        if (!end) end = cursor + strlen(cursor);
        if (end - cursor >= 13 && cursor[5] == '-' && cursor[11] == ',') {
            const int start = routineMinute(cursor), finish = routineMinute(cursor + 6);
            const char* label = cursor + 12;
            const size_t length = static_cast<size_t>(end - label);
            if (effectiveRoutineEnabled(day, start, label, length, weekday)) {
                if (start <= minute && minute < finish && routineLabelIs(label, length, "早读")) {
                    result = {RoutineSelectionKind::EarlyReading, start, -1, 0, label, length};
                    return true;
                }
                if (start > minute && (!found || start < result.minute || (day.independent&&start==result.minute))) {
                    result = {RoutineSelectionKind::Next, start, lessonLabelIndex(label, length),
                              lessonSectionAt(day.clock, start), label, length};
                    found = true;
                }
            }
        }
        cursor = *end ? end + 1 : end;
    }
    return found;
}

inline bool sameEffectiveView(const EffectiveDay& day, const char* routine,
                              int fromMinute, int toMinute, int weekday) {
    if (effectiveHasRemaining(day, fromMinute) != effectiveHasRemaining(day, toMinute) ||
        effectiveSectionAt(day, fromMinute) != effectiveSectionAt(day, toMinute) ||
        effectiveActiveLesson(day, fromMinute) != effectiveActiveLesson(day, toMinute)) return false;
    RoutineSelection before = {}, after = {};
    const bool first = selectEffectiveActivity(day, routine, fromMinute, weekday, before);
    const bool second = selectEffectiveActivity(day, routine, toMinute, weekday, after);
    return first == second && (!first ||
        (before.kind == after.kind && before.minute == after.minute &&
         before.lesson == after.lesson && before.section == after.section && before.label == after.label));
}
inline int nextEffectiveBoundary(const EffectiveDay& day, const char* routine,
                                 int secondsToday, int weekday) {
    int next = -1;
    const auto consider = [&](int minute) {
        const int second = minute * 60;
        if (second > secondsToday && (next < 0 || second < next) &&
            !sameEffectiveView(day, routine, minute - 1, minute, weekday)) next = second;
    };
    for (int i = 0; i < 12; ++i) if (day.mask & (1u << i)) {
        consider(day.clock.start[i]); consider(day.clock.end[i]);
    }
    consider(afternoonDisplayStart(day.clock));
    consider(eveningDisplayStart(day.clock));
    if (routine) for (const char* cursor = routine; *cursor;) {
        const char* end = strchr(cursor, ';');
        if (!end) end = cursor + strlen(cursor);
        if (end - cursor >= 13 && cursor[5] == '-' && cursor[11] == ',') {
            consider(routineMinute(cursor)); consider(routineMinute(cursor + 6));
        }
        cursor = *end ? end + 1 : end;
    }
    return next;
}
