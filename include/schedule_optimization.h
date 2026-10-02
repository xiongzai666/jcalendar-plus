#pragma once
#include "lesson_clock_core.h"

inline const char* morningActivityLabel(const char* label, int weekday) {
    if (!strcmp(label, "晨会或跑操") || !strcmp(label, "晨会/跑操"))
        return weekday == 1 ? "晨会" : "跑操";
    return label;
}

enum class FooterKind { None, Next, EarlyReading };
inline FooterKind routineFooterItem(const char* routine, int minute,
                                    int lastMinute, NextRoutineItem& result) {
    if (!routine) return FooterKind::None;
    for (const char* cursor = routine; *cursor;) {
        const char* end = strchr(cursor, ';');
        if (!end) end = cursor + strlen(cursor);
        if (end - cursor >= 13 && cursor[5] == '-' && cursor[11] == ',') {
            const int start = routineMinute(cursor), finish = routineMinute(cursor + 6);
            const size_t length = static_cast<size_t>(end - cursor - 12);
            if (start <= minute && minute < finish &&
                length == strlen("早读") && !memcmp(cursor + 12, "早读", length)) {
                result = {start, cursor + 12, length};
                return FooterKind::EarlyReading;
            }
        }
        cursor = *end ? end + 1 : end;
    }
    return nextRoutineItem(routine, minute, lastMinute, result)
        ? FooterKind::Next : FooterKind::None;
}

constexpr int REFRESH_MERGE_SECONDS = 120;
constexpr int REGULAR_NETWORK_MERGE_SECONDS = 600;
struct RefreshWakePlan { int seconds; bool network; bool retry; };
inline RefreshWakePlan chooseRefreshWake(int networkSeconds, int activitySeconds,
                                        int retrySeconds,
                                        int networkMergeSeconds = REGULAR_NETWORK_MERGE_SECONDS) {
    RefreshWakePlan plan = {networkSeconds, true, false};
    if (retrySeconds >= 0 && retrySeconds < plan.seconds) {
        plan = {retrySeconds, true, true};
        networkMergeSeconds = REFRESH_MERGE_SECONDS;
    }
    if (activitySeconds >= 0) {
        const int distance = plan.seconds - activitySeconds;
        if (distance >= -networkMergeSeconds && distance <= networkMergeSeconds)
            plan.seconds = activitySeconds;
        else if (activitySeconds < plan.seconds)
            plan = {activitySeconds, false, false};
    }
    if (plan.network && retrySeconds >= 0 &&
        retrySeconds - plan.seconds >= -REFRESH_MERGE_SECONDS &&
        retrySeconds - plan.seconds <= REFRESH_MERGE_SECONDS)
        plan.retry = true;
    return plan;
}

// A fixed two-hour slot may be served a little early by a combined wake.
// Remember the served slot to prevent another wake a minute later.
inline int64_t nextRegularNetworkAt(int64_t now, int64_t servedSlot) {
    int64_t slot = (now / 7200 + 1) * 7200 + 10;
    if (servedSlot >= slot) slot = servedSlot + 7200;
    return slot;
}

inline int dismissalWeatherMinute(const LessonClock& clock, int section,
                                  int lessonCount) {
    if (section < 0 || section > 2 || section * 4 >= lessonCount) return -1;
    const int last = (section + 1) * 4 < lessonCount ? (section + 1) * 4 : lessonCount;
    return clock.end[last - 1] - 10;
}

inline bool dismissalHintVisible(const LessonClock& clock, int minute,
                                  int lessonCount) {
    for (int section = 0; section < 3; ++section) {
        const int updateMinute = dismissalWeatherMinute(clock, section, lessonCount);
        // The existing weather wake may be merged two minutes early. Show
        // its result in that same frame instead of adding another refresh.
        if (updateMinute >= 0 && minute >= updateMinute - REFRESH_MERGE_SECONDS / 60 &&
            minute < updateMinute + 10) return true;
    }
    return false;
}

inline bool wetWeatherText(const char* text) {
    return text && (strstr(text, "雨") || strstr(text, "雪"));
}
inline const char* umbrellaHint(const char* current, const char* forecast, bool fresh) {
    if (!fresh) return "天气未更新";
    if (wetWeatherText(current)) return "带伞·有雨";
    if (wetWeatherText(forecast)) return "带伞·预报雨";
    return "当前无雨";
}

template<class Canvas>
void drawTodayColumnFrame(Canvas& canvas, int x, int y, int width,
                          int height, uint16_t red) {
    // Grid boundaries are x and x+width, inclusive. Cover their black pixels
    // on both sides, instead of leaving a black line beside the right edge.
    canvas.fillRect(x, y, 2, height, red);
    canvas.fillRect(x + width - 1, y, 2, height, red);
    canvas.fillRect(x, y, width + 1, 2, red);
    canvas.fillRect(x, y + height - 2, width + 1, 2, red);
}
