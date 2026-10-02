#include <cassert>
#include <cstring>
#include <string>

#include "lesson_clock_core.h"
#include "routine_fixture.h"

int main() {
    LessonClock clock = {};
    assert(parseLessonClock(TEST_DAILY_ROUTINE, clock));
    assert(clock.start[0] == 7 * 60 + 55 && clock.end[0] == 8 * 60 + 40);
    assert(clock.start[5] == 14 * 60 + 35 && clock.end[5] == 15 * 60 + 20);
    assert(clock.start[11] == 21 * 60 + 20);

    // Start is inclusive; end is exclusive. Saturday has only six periods.
    assert(activeLessonAt(clock, 7 * 60 + 54) == -1);
    assert(activeLessonAt(clock, 7 * 60 + 55) == 0);
    assert(activeLessonAt(clock, 8 * 60 + 40) == -1);
    assert(activeLessonAt(clock, 15 * 60 + 35, 6) == -1);
    assert(activeLessonAt(clock, 15 * 60 + 35, 12) == 6);
    assert(lessonSectionAt(clock, 11 * 60 + 59) == 0);
    assert(lessonSectionAt(clock, 12 * 60) == 1);
    assert(lessonSectionAt(clock, 17 * 60 + 49) == 1);
    assert(lessonSectionAt(clock, 17 * 60 + 50) == 2);
    LessonClock earlierAfternoon = clock;
    earlierAfternoon.start[4] = 11 * 60 + 50;
    assert(lessonSectionAt(earlierAfternoon, 11 * 60 + 50) == 1);
    assert(crossedLessonBoundary(clock, 8 * 3600 + 39 * 60, 8 * 3600 + 40 * 60));
    assert(!crossedLessonBoundary(clock, 8 * 3600 + 40 * 60, 8 * 3600 + 49 * 60));

    // The footer advances at the start of an activity, including lunch.
    NextRoutineItem item = {};
    assert(nextRoutineItem(TEST_DAILY_ROUTINE, 12 * 60 + 39, 1439, item));
    assert(item.minute == 12 * 60 + 40 &&
           std::string(item.label, item.labelLength) == "午休");
    assert(nextRoutineItem(TEST_DAILY_ROUTINE, 12 * 60 + 40, 1439, item));
    assert(item.minute == 13 * 60 + 30 &&
           std::string(item.label, item.labelLength) == "课间");
    assert(!nextRoutineItem(TEST_DAILY_ROUTINE, 15 * 60 + 20,
                            clock.end[5] - 1, item));

    // A change in the routine moves the same lesson's highlight and wake.
    std::string moved = TEST_DAILY_ROUTINE;
    const std::string oldBreak = "13:30-13:40,课间";
    const std::string oldClass = "13:40-14:25,第5节";
    moved.replace(moved.find(oldBreak), oldBreak.size(), "13:30-13:45,课间");
    moved.replace(moved.find(oldClass), oldClass.size(), "13:45-14:25,第5节");
    assert(parseLessonClock(moved.c_str(), clock));
    assert(clock.start[4] == 13 * 60 + 45);
    assert(activeLessonAt(clock, 13 * 60 + 44) == -1);
    assert(activeLessonAt(clock, 13 * 60 + 45) == 4);
    assert(crossedLessonBoundary(clock, 13 * 3600 + 44 * 60,
                                 13 * 3600 + 45 * 60 + 10));

    std::string invalid = TEST_DAILY_ROUTINE;
    invalid.replace(invalid.find("第5节"), std::strlen("第5节"), "课外活动");
    assert(!parseLessonClock(invalid.c_str(), clock));
    return 0;
}
