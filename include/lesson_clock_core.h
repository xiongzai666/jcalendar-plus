#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// The twelve labelled lesson rows in dailyRoutine define the regular clock;
// an independent date may override occupied slots. This header is Arduino-independent so the
// exact boundary logic can also be tested on a host compiler.
struct LessonClock {
    uint16_t start[12];
    uint16_t end[12];
};

inline LessonClock defaultLessonClock() {
    return {{480, 535, 590, 645, 840, 895, 950, 1005, 1110, 1165, 1220, 1275},
            {525, 580, 635, 690, 885, 940, 995, 1050, 1155, 1210, 1265, 1320}};
}

inline int lessonLabelIndex(const char* label, size_t length) {
    static const char* const labels[12] = {
        "第1节", "第2节", "第3节", "第4节", "第5节", "第6节",
        "第7节", "第8节", "晚自习1", "晚自习2", "晚自习3", "晚自习4"
    };
    for (int i = 0; i < 12; ++i)
        if (strlen(labels[i]) == length && memcmp(label, labels[i], length) == 0)
            return i;
    return -1;
}

inline int routineMinute(const char* text) {
    if (text[0] < '0' || text[0] > '2' ||
        text[1] < '0' || text[1] > '9' || text[2] != ':' ||
        text[3] < '0' || text[3] > '5' || text[4] < '0' || text[4] > '9')
        return -1;
    const int hour = (text[0] - '0') * 10 + text[1] - '0';
    return hour < 24 ? hour * 60 + (text[3] - '0') * 10 + text[4] - '0' : -1;
}

inline bool parseLessonClock(const char* routine, LessonClock& result) {
    if (!routine) return false;
    LessonClock parsed = {};
    const char* cursor = routine;
    int expectedLesson = 0;
    int previousEnd = -1;
    while (*cursor) {
        const char* end = strchr(cursor, ';');
        if (!end) end = cursor + strlen(cursor);
        const size_t length = static_cast<size_t>(end - cursor);
        if (length < 13 || cursor[5] != '-' || cursor[11] != ',') return false;
        const int start = routineMinute(cursor);
        const int finish = routineMinute(cursor + 6);
        if (start < 0 || finish <= start || start < previousEnd) return false;
        previousEnd = finish;
        const int lesson = lessonLabelIndex(cursor + 12, length - 12);
        if (lesson >= 0) {
            if (lesson != expectedLesson) return false;
            parsed.start[lesson] = static_cast<uint16_t>(start);
            parsed.end[lesson] = static_cast<uint16_t>(finish);
            ++expectedLesson;
        }
        cursor = *end ? end + 1 : end;
    }
    if (expectedLesson != 12) return false;
    result = parsed;
    return true;
}

inline int activeLessonAt(const LessonClock& clock, int minute, int count = 12) {
    for (int i = 0; i < count; ++i)
        if (minute >= clock.start[i] && minute < clock.end[i]) return i;
    return -1;
}

inline int afternoonDisplayStart(const LessonClock& clock) {
    return clock.start[4] < 720 ? clock.start[4] : 720;
}

inline int eveningDisplayStart(const LessonClock& clock) {
    return clock.start[8] < 1070 ? clock.start[8] : 1070;
}

inline int lessonSectionAt(const LessonClock& clock, int minute) {
    if (minute < afternoonDisplayStart(clock)) return 0;
    return minute < eveningDisplayStart(clock) ? 1 : 2;
}

inline bool crossedLessonBoundary(const LessonClock& clock, int fromSecond,
                                  int toSecond, int count = 12) {
    for (int i = 0; i < count; ++i)
        if ((fromSecond < clock.start[i] * 60 && clock.start[i] * 60 <= toSecond) ||
            (fromSecond < clock.end[i] * 60 && clock.end[i] * 60 <= toSecond))
            return true;
    return false;
}

struct NextRoutineItem {
    int minute;
    const char* label;
    size_t labelLength;
};

inline bool nextRoutineItem(const char* routine, int minute, int lastMinute,
                            NextRoutineItem& result) {
    if (!routine) return false;
    const char* cursor = routine;
    while (*cursor) {
        const char* end = strchr(cursor, ';');
        if (!end) end = cursor + strlen(cursor);
        if (end - cursor >= 13 && cursor[5] == '-' && cursor[11] == ',') {
            const int start = routineMinute(cursor);
            if (start > minute && start <= lastMinute) {
                result = {start, cursor + 12, static_cast<size_t>(end - cursor - 12)};
                return true;
            }
        }
        cursor = *end ? end + 1 : end;
    }
    return false;
}
