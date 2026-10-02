#include <cassert>
#include <cstring>
#include <cstdio>
#include "effective_schedule_core.h"
#include "routine_fixture.h"

int main() {
    LessonClock clock;
    assert(parseLessonClock(TEST_DAILY_ROUTINE, clock));
    const char* all[12] = {"语文","英语","物理","数学","地理","化学","体育","班会","自习","自习","自习","自习"};
    auto day = makeEffectiveDay(clock, all, false);
    assert(day.mask == 0xfff && effectiveDismissal(day, 0) == 685);
    assert(effectiveDismissal(day, 1) == 1035 && effectiveDismissal(day, 2) == 1320);
    assert(effectiveActiveLesson(day, 475) == 0 && effectiveActiveLesson(day, 520) == -1);
    const char* partial[12] = {"化学","数学","英语","物理","语文","地理","补课","放假","放假","放假","放假","放假"};
    day = makeEffectiveDay(clock, partial, false);
    assert(day.mask == 0x7f && effectiveDismissal(day, 1) == 980);
    assert(effectiveDismissal(day, 2) == -1);
    assert(effectiveActiveLesson(day, 990) == -1);
    assert(!effectiveHasRemaining(day, 980));
    assert(!effectiveHintVisible(day, 1025));
    assert(effectiveHintVisible(day, 970));
    const char* sunday[12] = {"","临时英语","","","","","","","","","",""};
    day = makeEffectiveDay(clock, sunday, false);
    assert(day.mask == 2 && effectiveDismissal(day, 0) == 575);
    assert(effectiveDismissal(day, 1) == -1 && effectiveDismissal(day, 2) == -1);
    assert(effectiveActiveLesson(day, 475) == -1);
    assert(effectiveActiveLesson(day, 530) == 1);
    assert(makeEffectiveDay(clock, all, true).mask == 0);
    RoutineSelection next = {};
    day = makeEffectiveDay(clock, partial, false);
    assert(selectEffectiveActivity(day, TEST_DAILY_ROUTINE, 965, 6, next));
    assert(next.minute == 980 && next.kind == RoutineSelectionKind::Dismissal);
    assert(!selectEffectiveActivity(day, TEST_DAILY_ROUTINE, 980, 6, next));
    day = makeEffectiveDay(clock, sunday, false);
    assert(selectEffectiveActivity(day, TEST_DAILY_ROUTINE, 529, 0, next));
    assert(next.minute == 530 && next.lesson == 1);
    assert(selectEffectiveActivity(day, TEST_DAILY_ROUTINE, 550, 0, next));
    assert(next.minute == 575 && next.kind == RoutineSelectionKind::Dismissal);
    assert(nextEffectiveBoundary(day, TEST_DAILY_ROUTINE, 500 * 60, 0) == 530 * 60);
    assert(nextEffectiveBoundary(day, TEST_DAILY_ROUTINE, 550 * 60, 0) == 575 * 60);
    assert(nextEffectiveBoundary(day, TEST_DAILY_ROUTINE, 576 * 60, 0) == -1);
    puts("effective schedule: partial Saturday, Sunday, rest, footer and wake checks passed");
}
