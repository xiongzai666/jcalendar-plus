#include <cassert>
#include <cstring>
#include <string>
#include <cstdio>
#include <set>
#include <vector>
#include "schedule_optimization.h"
#include "routine_fixture.h"

struct Canvas {
    int pixels[300][400] = {};
    void fillRect(int x, int y, int w, int h, int color) {
        for (int dy = 0; dy < h; ++dy)
            for (int dx = 0; dx < w; ++dx) pixels[y + dy][x + dx] = color;
    }
};

void checkWholeDay(const LessonClock& clock, int count) {
    std::set<int> activities;
    for (int i = 0; i < count; ++i) {
        activities.insert(clock.start[i] * 60 + 10);
        activities.insert(clock.end[i] * 60 + 10);
    }
    activities.insert(afternoonDisplayStart(clock) * 60 + 10);
    activities.insert(eveningDisplayStart(clock) * 60 + 10);
    for (const char* cursor = TEST_DAILY_ROUTINE; *cursor;) {
        const char* end = strchr(cursor, ';');
        if (!end) end = cursor + strlen(cursor);
        const int minute = routineMinute(cursor);
        if (count == 12 || minute <= clock.end[5]) activities.insert(minute * 60 + 10);
        cursor = *end ? end + 1 : end;
    }
    std::set<int> baseline = activities;
    for (int at = 7210; at < 86400; at += 7200) baseline.insert(at);
    std::vector<int> weather;
    for (int section = 0; section < 3; ++section) {
        const int minute = dismissalWeatherMinute(clock, section, count);
        if (minute >= 0) weather.push_back(minute * 60 + 10);
    }
    int now = 0, wakes = 0;
    int64_t servedRegular = 0, servedWeather = 0;
    std::set<int> rendered;
    while (true) {
        const int regular = static_cast<int>(nextRegularNetworkAt(now, servedRegular));
        int wx = 0;
        for (int at : weather)
            if (at > now && at > servedWeather && (!wx || at < wx)) wx = at;
        const auto nextActivity = activities.upper_bound(now);
        const int activity = nextActivity == activities.end() ? -1 : *nextActivity - now;
        const int network = wx && wx < regular ? wx : regular;
        const auto p = chooseRefreshWake(network - now, activity, -1,
            network == wx ? REFRESH_MERGE_SECONDS : REGULAR_NETWORK_MERGE_SECONDS);
        now += p.seconds;
        if (now >= 86400) break;
        assert(rendered.insert(now).second);
        ++wakes;
        if (p.network) {
            if (regular - now >= -600 && regular - now <= 600) servedRegular = regular;
            if (wx && wx - now >= -120 && wx - now <= 120) servedWeather = wx;
        }
    }
    // Every visible activity still gets a frame at its exact boundary.
    for (int at : activities) assert(rendered.count(at));
    assert(wakes < static_cast<int>(baseline.size()));
    printf("%d lessons: %d unmerged refreshes -> %d with merge and dismissal weather\n",
           count, static_cast<int>(baseline.size()), wakes);
}

int main() {
    LessonClock clock;
    assert(parseLessonClock(TEST_DAILY_ROUTINE, clock));
    assert(lessonSectionAt(clock, 719) == 0);
    assert(lessonSectionAt(clock, 720) == 1);
    assert(lessonSectionAt(clock, 1069) == 1);
    assert(lessonSectionAt(clock, 1070) == 2);
    assert(std::string(morningActivityLabel("晨会或跑操", 1)) == "晨会");
    for (int day = 2; day <= 6; ++day)
        assert(std::string(morningActivityLabel("晨会/跑操", day)) == "跑操");
    assert(std::string(morningActivityLabel("临时集会", 2)) == "临时集会");

    NextRoutineItem item = {};
    assert(routineFooterItem(TEST_DAILY_ROUTINE, 414, 1439, item) == FooterKind::Next);
    assert(routineFooterItem(TEST_DAILY_ROUTINE, 415, 1439, item) == FooterKind::EarlyReading);
    assert(routineFooterItem(TEST_DAILY_ROUTINE, 454, 1439, item) == FooterKind::EarlyReading);
    assert(routineFooterItem(TEST_DAILY_ROUTINE, 455, 1439, item) == FooterKind::Next);
    assert(item.minute == clock.start[0]);
    assert(std::string(item.label, item.labelLength) == "第1节");
    assert(routineFooterItem(TEST_DAILY_ROUTINE, 520, 1439, item) == FooterKind::Next);
    assert(item.minute == clock.start[1]);
    assert(routineFooterItem(TEST_DAILY_ROUTINE, 1069, 1439, item) == FooterKind::Next);
    assert(item.minute == 1070);

    // Pin activity refreshes and merge the network wake on either side.
    auto p = chooseRefreshWake(130, 100, -1);
    assert(p.seconds == 100 && p.network && !p.retry);
    p = chooseRefreshWake(100, 130, -1);
    assert(p.seconds == 130 && p.network);
    p = chooseRefreshWake(221, 100, -1, REFRESH_MERGE_SECONDS);
    assert(p.seconds == 100 && !p.network);
    p = chooseRefreshWake(100, 221, -1, REFRESH_MERGE_SECONDS);
    assert(p.seconds == 100 && p.network);
    p = chooseRefreshWake(500, 100, 150);
    assert(p.seconds == 100 && p.network && p.retry);
    p = chooseRefreshWake(500, -1, 50);
    assert(p.seconds == 50 && p.retry);
    p = chooseRefreshWake(100, 130, 150);
    assert(p.seconds == 130 && p.network && p.retry);
    p = chooseRefreshWake(310, 10, -1); // 07:55 lesson, 08:00 regular poll
    assert(p.seconds == 10 && p.network);
    p = chooseRefreshWake(610, 10, -1); // 09:50 lesson, 10:00 regular poll
    assert(p.seconds == 10 && p.network);
    p = chooseRefreshWake(10, 610, -1); // 20:00 poll, 20:10 lesson
    assert(p.seconds == 610 && p.network);
    p = chooseRefreshWake(10, 611, -1);
    assert(p.seconds == 10 && p.network);
    // An early combined refresh satisfies the fixed even-hour slot.
    assert(nextRegularNetworkAt(43100, 43210) == 50410);
    assert(nextRegularNetworkAt(43100, 0) == 43210);

    assert(dismissalWeatherMinute(clock, 0, 12) == 675); // 11:15
    assert(dismissalWeatherMinute(clock, 1, 12) == 1025); // 17:05
    assert(dismissalWeatherMinute(clock, 2, 12) == 1310); // 21:50
    assert(dismissalWeatherMinute(clock, 1, 6) == 910); // Saturday 15:10
    assert(dismissalWeatherMinute(clock, 2, 6) == -1);
    // Only show the umbrella note near dismissal, including the existing
    // two-minute early merge allowance, then clear at the lesson end.
    assert(!dismissalHintVisible(clock, 10 * 60, 12));
    assert(!dismissalHintVisible(clock, 11 * 60 + 12, 12));
    assert(dismissalHintVisible(clock, 11 * 60 + 13, 12));
    assert(dismissalHintVisible(clock, 11 * 60 + 15, 12));
    assert(dismissalHintVisible(clock, 11 * 60 + 24, 12));
    assert(!dismissalHintVisible(clock, 11 * 60 + 25, 12));
    assert(!dismissalHintVisible(clock, 12 * 60, 12));
    assert(dismissalHintVisible(clock, 17 * 60 + 5, 12));
    assert(!dismissalHintVisible(clock, 17 * 60 + 15, 12));
    assert(dismissalHintVisible(clock, 21 * 60 + 50, 12));
    assert(!dismissalHintVisible(clock, 22 * 60, 12));
    assert(dismissalHintVisible(clock, 15 * 60 + 10, 6));
    assert(!dismissalHintVisible(clock, 15 * 60 + 20, 6));
    assert(!dismissalHintVisible(clock, 17 * 60 + 5, 6));
    LessonClock shifted = clock;
    shifted.end[3] = 11 * 60 + 35;
    assert(!dismissalHintVisible(shifted, 11 * 60 + 15, 12));
    assert(dismissalHintVisible(shifted, 11 * 60 + 25, 12));
    assert(!dismissalHintVisible(shifted, 11 * 60 + 35, 12));
    assert(std::string(umbrellaHint("小雨", "多云", true)) == "带伞·有雨");
    assert(std::string(umbrellaHint("晴", "雷阵雨", true)) == "带伞·预报雨");
    assert(std::string(umbrellaHint("晴", "晴", true)) == "当前无雨");
    assert(std::string(umbrellaHint("晴", "晴", false)) == "天气未更新");
    checkWholeDay(clock, 12);
    checkWholeDay(clock, 6);

    Canvas c;
    // Grid lines share the left/right x coordinates; red must replace both.
    c.fillRect(223, 63, 1, 233, 1);
    c.fillRect(280, 63, 1, 233, 1);
    drawTodayColumnFrame(c, 223, 40, 57, 256, 2);
    for (int y = 65; y < 292; ++y) {
        assert(c.pixels[y][223] == 2 && c.pixels[y][224] == 2);
        assert(c.pixels[y][279] == 2 && c.pixels[y][280] == 2);
        assert(c.pixels[y][225] == 0 && c.pixels[y][278] == 0);
    }
    for (int x = 223; x <= 280; ++x) {
        assert(c.pixels[40][x] == 2 && c.pixels[41][x] == 2);
        assert(c.pixels[294][x] == 2 && c.pixels[295][x] == 2);
    }
    puts("schedule optimization: all boundary, merge, umbrella and frame checks passed");
}
