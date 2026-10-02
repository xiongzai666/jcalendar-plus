#include "school_schedule_data.h"
#include "subject_font.h"
#include "date_overrides.h"
#include "lesson_clock.h"
#include "schedule_optimization.h"
#include "effective_schedule.h"

// Compact timetable dashboard for the 400x300 three-colour panel.
static String fitText(const String& value, int maxWidth) {
    String result;
    for (size_t i = 0; i < value.length();) {
        const unsigned char first = static_cast<unsigned char>(value[i]);
        size_t bytes = first < 0x80 ? 1 : (first < 0xE0 ? 2 : (first < 0xF0 ? 3 : 4));
        if (i + bytes > value.length()) break;
        String next = result + value.substring(i, i + bytes);
        if (u8g2Fonts.getUTF8Width(next.c_str()) > maxWidth) break;
        result = next;
        i += bytes;
    }
    return result;
}

static String fitTextEllipsis(const String& value, int maxWidth) {
    if (u8g2Fonts.getUTF8Width(value.c_str()) <= maxWidth) return value;
    const String suffix = "...";
    const int available = maxWidth - u8g2Fonts.getUTF8Width(suffix.c_str());
    return available > 0 ? fitText(value, available) + suffix : fitText(value, maxWidth);
}

static void centered(const String& value, int x, int y) {
    const int left = x - u8g2Fonts.getUTF8Width(value.c_str()) / 2;
    u8g2Fonts.drawUTF8(left, y, value.c_str());
}

static void drawViewText(int x, int y, const String& value) {
    u8g2Fonts.drawUTF8(x, y, value.c_str());
}

static String lunarDayText(int day) {
    if (day == 10) return "初十";
    if (day == 20) return "二十";
    if (day == 30) return "三十";
    if (day < 1 || day > 30) return "";
    return nl10_str[day / 10] + nl_str[day % 10];
}

static String nextFestival(int& daysAway) {
    struct tm date = tmInfo;
    date.tm_hour = 12;
    date.tm_min = date.tm_sec = 0;
    date.tm_isdst = -1;
    const time_t start = mktime(&date);
    int monthDays[31] = {};
    int cachedYear = 0, cachedMonth = 0;
    for (int offset = 1; offset <= 370; ++offset) {
        time_t probe = start + static_cast<time_t>(offset) * 86400;
        struct tm candidate;
        localtime_r(&probe, &candidate);
        const int year = candidate.tm_year + 1900;
        const int month = candidate.tm_mon + 1;
        if (year < 1902 || year > 2099) break;
        if (year != cachedYear || month != cachedMonth) {
            nl_month_days(year, month, monthDays);
            cachedYear = year;
            cachedMonth = month;
        }
        const int lunar = monthDays[candidate.tm_mday - 1];
        const char* name = nullptr;
        if (month == 1 && candidate.tm_mday == 1) name = "元旦";
        else if (month == 5 && candidate.tm_mday == 1) name = "劳动节";
        else if (month == 10 && candidate.tm_mday == 1) name = "国庆节";
        else if (lunar > 0 && lunar == 101) name = "春节";
        else if (lunar > 0 && lunar == 115) name = "元宵节";
        else if (lunar > 0 && lunar == 505) name = "端午节";
        else if (lunar > 0 && lunar == 707) name = "七夕节";
        else if (lunar > 0 && lunar == 815) name = "中秋节";
        else if (lunar > 0 && lunar == 909) name = "重阳节";
        if (name) {
            daysAway = offset;
            return name;
        }
    }
    daysAway = -1;
    return "暂无节日";
}

static bool examCountdown(int& daysLeft, String& targetLabel) {
    if (_cd_day_date.length() != 8) return false;
    const int year = _cd_day_date.substring(0, 4).toInt();
    const int month = _cd_day_date.substring(4, 6).toInt();
    const int day = _cd_day_date.substring(6, 8).toInt();
    if (year < 2025 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31) return false;
    struct tm target = {};
    target.tm_year = year - 1900;
    target.tm_mon = month - 1;
    target.tm_mday = day;
    target.tm_hour = 12;
    target.tm_isdst = -1;
    const time_t targetTime = mktime(&target);
    if (target.tm_year != year - 1900 || target.tm_mon != month - 1 || target.tm_mday != day) return false;
    struct tm current = tmInfo;
    current.tm_hour = 12;
    current.tm_min = current.tm_sec = 0;
    current.tm_isdst = -1;
    const time_t currentTime = mktime(&current);
    daysLeft = static_cast<int>(difftime(targetTime, currentTime) / 86400);
    char buffer[18];
    snprintf(buffer, sizeof(buffer), "%04d.%02d.%02d", year, month, day);
    targetLabel = buffer;
    return true;
}

static void loadCoursesForDate(const struct tm& date, String courses[12]) {
    const EffectiveDayContext context = effectiveDayForDate(date);
    for (int i = 0; i < 12; ++i) courses[i] = context.courses[i];
}

static void loadTodayCourses(String courses[12]) {
    loadCoursesForDate(tmInfo, courses);
}

static String nextScheduledLesson() {
    struct tm date = tmInfo;
    date.tm_hour = 12;
    date.tm_min = date.tm_sec = 0;
    date.tm_isdst = -1;
    for (int offset = 1; offset <= 14; ++offset) {
        struct tm next = date;
        next.tm_mday += offset;
        mktime(&next);
        const EffectiveDayContext context = effectiveDayForDate(next);
        if (context.override.dayOff) continue;
        for (int slot = 0; slot < 12; ++slot) {
            if (!effectiveSubject(context.courses[slot].c_str())) continue;
            const int minute = context.day.clock.start[slot];
            char start[6];
            snprintf(start, sizeof(start), "%02d:%02d", minute / 60, minute % 60);
            return "周" + week_str[next.tm_wday] + " " + start + " " + context.courses[slot];
        }
    }
    return "暂无已排课程";
}

static String todayEarlyReading() {
    if (tmInfo.tm_wday < 1 || tmInfo.tm_wday > 6) return "";
    Preferences pref;
    pref.begin(PREF_NAMESPACE, true);
    const String source = pref.getString(PREF_EARLY_READING, DEFAULT_EARLY_READING);
    pref.end();
    const String marker = week_str[tmInfo.tm_wday] + ":";
    const int begin = source.indexOf(marker);
    if (begin < 0) return "";
    const int valueBegin = begin + marker.length();
    const int end = source.indexOf(';', valueBegin);
    const String subject = source.substring(valueBegin, end < 0 ? source.length() : end);
    return subject == "未注明" ? "" : subject;
}

static String nextDailyActivity(int clockMinute, const EffectiveDayContext& context) {
    RoutineSelection item = {};
    if (!selectEffectiveActivity(context.day, context.routine.c_str(), clockMinute,
                                 tmInfo.tm_wday, item)) return "今日作息已结束";
    if (item.kind == RoutineSelectionKind::EarlyReading) {
        const String reading = todayEarlyReading();
        return reading.isEmpty() ? String("早读") : "早读 " + reading;
    }
    String label;
    if (item.kind == RoutineSelectionKind::Dismissal) {
        bool later = false;
        for (int group = item.section + 1; group < 3; ++group)
            later |= effectiveDismissal(context.day, group) >= 0;
        label = later ? (item.section == 0 ? "中午放学" : "下午放学") : "放学";
    } else if (item.lesson >= 0) label = context.courses[item.lesson];
    else {
        label = String(item.label).substring(0, item.labelLength);
        label = morningActivityLabel(label.c_str(), tmInfo.tm_wday);
        if (label == "早读") label += todayEarlyReading();
    }
    char start[6];
    snprintf(start, sizeof(start), "%02d:%02d", item.minute / 60, item.minute % 60);
    return "下一项 " + label + " " + String(start);
}

static const SubjectGlyph* subjectGlyph(uint32_t codepoint) {
    for (int i = 0; i < SUBJECT_GLYPH_COUNT; ++i) {
        if (pgm_read_dword(&SUBJECT_GLYPHS[i].codepoint) == codepoint)
            return &SUBJECT_GLYPHS[i];
    }
    return nullptr;
}

static int largeSubjectWidth(const String& value) {
    int width = 0;
    for (size_t i = 0; i < value.length();) {
        const uint8_t first = static_cast<uint8_t>(value[i]);
        uint32_t codepoint;
        size_t bytes;
        if (first < 0x80) { codepoint = first; bytes = 1; }
        else if ((first & 0xE0) == 0xC0 && i + 1 < value.length()) {
            codepoint = ((first & 0x1F) << 6) |
                        (static_cast<uint8_t>(value[i + 1]) & 0x3F);
            bytes = 2;
        } else if ((first & 0xF0) == 0xE0 && i + 2 < value.length()) {
            codepoint = ((first & 0x0F) << 12) |
                        ((static_cast<uint8_t>(value[i + 1]) & 0x3F) << 6) |
                        (static_cast<uint8_t>(value[i + 2]) & 0x3F);
            bytes = 3;
        } else return -1;
        const SubjectGlyph* glyph = subjectGlyph(codepoint);
        if (!glyph) return -1;
        width += pgm_read_byte(&glyph->width);
        i += bytes;
    }
    return width;
}

static void drawLargeSubject(const String& value, int centerX, int top,
                             uint16_t color, int maxWidth = 104) {
    const int width = largeSubjectWidth(value);
    if (width < 0 || width > maxWidth) {
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.setForegroundColor(color);
        centered(fitTextEllipsis(value, maxWidth), centerX, top + 19);
        return;
    }
    int x = centerX - width / 2;
    for (size_t i = 0; i < value.length();) {
        const uint8_t first = static_cast<uint8_t>(value[i]);
        const size_t bytes = first < 0x80 ? 1 : (first < 0xE0 ? 2 : 3);
        uint32_t codepoint = first;
        if (bytes == 2) codepoint = ((first & 0x1F) << 6) |
                                    (static_cast<uint8_t>(value[i + 1]) & 0x3F);
        else if (bytes == 3) codepoint = ((first & 0x0F) << 12) |
                                         ((static_cast<uint8_t>(value[i + 1]) & 0x3F) << 6) |
                                         (static_cast<uint8_t>(value[i + 2]) & 0x3F);
        const SubjectGlyph* glyph = subjectGlyph(codepoint);
        const int glyphWidth = pgm_read_byte(&glyph->width);
        for (int y = 0; y < 24; ++y) {
            for (int dx = 0; dx < glyphWidth; ++dx) {
                const int bit = y * glyphWidth + dx;
                const uint8_t packed = pgm_read_byte(&glyph->pixels[bit / 8]);
                if (packed & (0x80 >> (bit % 8))) display.drawPixel(x + dx, top + y, color);
            }
        }
        x += glyphWidth;
        i += bytes;
    }
}

static void drawTodayLessons(int split) {
    const int hour = tmInfo.tm_hour;
    const int minute = tmInfo.tm_min;
    const int clockMinute = hour * 60 + minute;
    const EffectiveDayContext context = effectiveDayForDate(tmInfo);
    const LessonClock& clock = context.day.clock;
    const int section = effectiveSectionAt(context.day, clockMinute);
    const char* sectionNames[3] = {"上午课程", "下午课程", "晚间课程"};

    const bool rest = !effectiveHasRemaining(context.day, clockMinute);
    display.fillRect(0, 43, split, 31, GxEPD_BLACK);
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    drawViewText(11, 65, rest ? "今日安排" : sectionNames[section]);
    if (!rest && !lowBattery && effectiveHintVisible(context.day, clockMinute)) {
        const String hint = weather_dismissal_hint(section == 2, _screen_snapshot_time);
        u8g2Fonts.setFont(FONT_SUB);
        drawViewText(split - 8 - u8g2Fonts.getUTF8Width(hint.c_str()), 64, hint);
    }
    if (lowBattery) {
        u8g2Fonts.setFont(FONT_SUB);
        drawViewText(split - 53, 64, "电量低");
    }
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

    if (rest) {
        drawLargeSubject(_study_schedule.isEmpty() && !context.day.independent && !context.override.dayOff ? "课表待配置" :
                         context.day.mask ? "今日课程结束" :
                         context.day.independent && !context.override.dayOff ? "今日无课" :
                         (tmInfo.tm_wday == 0 && !context.override.dayOff ? "周日休息" : "今天放假"), split / 2, 125,
                         GxEPD_RED, split - 25);
        display.drawFastHLine(43, 178, split - 86, GxEPD_RED);
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        centered("下一次上课", split / 2, 209);
        centered(fitTextEllipsis(nextScheduledLesson(), split - 20), split / 2, 240);
        return;
    }

    const String* courses = context.courses;
    const int rowTop = 75;
    int indices[4] = {}, count = 0;
    for (int slot=section*4;slot<section*4+4;++slot)
        if (!context.day.independent || (context.day.mask & (1u << slot))) indices[count++] = slot;
    const int rowHeight = count ? 184 / count : 46;
    for (int row = 0; row < count; ++row) {
        const int top = rowTop + row * rowHeight;
        const int courseIndex = indices[row];
        const String subject = courses[courseIndex].isEmpty() ? "待配置" : courses[courseIndex];
        if (row > 0) display.drawFastHLine(8, top, split - 16, GxEPD_BLACK);
        const bool current = effectiveActiveLesson(context.day, clockMinute) == courseIndex;
        if (current) display.fillRect(1, top + rowHeight/2 - 17, 4, 34, GxEPD_RED);
        u8g2Fonts.setFont(u8g2_font_helvB12_tf);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        char times[12];
        snprintf(times, sizeof(times), "%02d:%02d-%02d:%02d",
                 clock.start[courseIndex] / 60, clock.start[courseIndex] % 60,
                 clock.end[courseIndex] / 60, clock.end[courseIndex] % 60);
        drawViewText(9, top + rowHeight/2 + 5, times);
        drawLargeSubject(subject, 187, top + rowHeight/2 - 14,
                         current ? GxEPD_RED : GxEPD_BLACK);
    }

    display.drawFastHLine(8, 265, split - 16, GxEPD_RED);
    String note = nextDailyActivity(clockMinute, context);
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    centered(fitTextEllipsis(note, split - 12), split / 2, 289);
}

static void drawTimetableCountdown() {
    const int split = 250;
    const int rightCenter = 325;
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);

    // Date and lunar header.
    int dateX = 8;
    const String dateParts[3] = {
        String(tmInfo.tm_year + 1900),
        String(tmInfo.tm_mon + 1),
        String(tmInfo.tm_mday)
    };
    const char* dateUnits[3] = {"年", "月", "日"};
    u8g2Fonts.setFont(u8g2_font_fub25_tn);
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    for (int i = 0; i < 3; ++i) {
        u8g2Fonts.drawUTF8(dateX, 32, dateParts[i].c_str());
        dateX += u8g2Fonts.getUTF8Width(dateParts[i].c_str());
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        u8g2Fonts.drawUTF8(dateX + 1, 29, dateUnits[i]);
        dateX += u8g2Fonts.getUTF8Width(dateUnits[i]) + 3;
        u8g2Fonts.setFont(u8g2_font_fub25_tn);
        u8g2Fonts.setForegroundColor(GxEPD_RED);
    }
    display.fillRoundRect(208, 7, 48, 30, 3, GxEPD_RED);
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setBackgroundColor(GxEPD_RED);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    centered(String("周") + week_str[tmInfo.tm_wday], 232, 29);
    const int rawLunar = lunarDates[tmInfo.tm_mday - 1];
    const int lunar = abs(rawLunar);
    const int lunarMonth = lunar / 100;
    String lunarText = "农历";
    if (lunarMonth >= 1 && lunarMonth <= 12) {
        lunarText += String(rawLunar < 0 ? "闰" : "") + nl_mon_str[lunarMonth] +
                     "月" + lunarDayText(lunar % 100);
    } else {
        lunarText += "未知";
    }
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    centered(lunarText, 328, 26);
    display.drawFastHLine(0, 42, 400, GxEPD_BLACK);
    display.drawFastVLine(split, 43, 257, GxEPD_BLACK);

    drawTodayLessons(split);

    // Exam countdown.
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    centered((_cd_day_label.isEmpty() ? String("目标") : _cd_day_label) + "倒计时",
             rightCenter, 64);
    int daysLeft = -1;
    String targetDate;
    const bool hasDate = examCountdown(daysLeft, targetDate);
    if (hasDate && daysLeft >= 0) {
        const String number = String(daysLeft);
        u8g2Fonts.setFont(FONT_TEXT);
        const int unitWidth = u8g2Fonts.getUTF8Width("天");
        const int gap = 2;
        u8g2Fonts.setFont(u8g2_font_fub49_tn);
        if (u8g2Fonts.getUTF8Width(number.c_str()) + gap + unitWidth > 142)
            u8g2Fonts.setFont(u8g2_font_fub35_tn);
        if (u8g2Fonts.getUTF8Width(number.c_str()) + gap + unitWidth > 142)
            u8g2Fonts.setFont(u8g2_font_fub25_tn);
        const int numberWidth = u8g2Fonts.getUTF8Width(number.c_str());
        const int numberX = split + (150 - numberWidth - gap - unitWidth) / 2;
        u8g2Fonts.setForegroundColor(GxEPD_RED);
        u8g2Fonts.drawUTF8(numberX, 135, number.c_str());
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        drawViewText(numberX + numberWidth + gap, 133, "天");
        u8g2Fonts.setFont(FONT_TEXT);
        centered(targetDate, rightCenter, 158);
    } else {
        u8g2Fonts.setFont(FONT_TEXT);
        centered(hasDate ? "目标日期已到" : "请设置目标日期", rightCenter, 115);
    }
    display.drawFastHLine(split + 1, 169, 149, GxEPD_BLACK);

    // Three real daily forecasts with no redundant section title.
    const char* labels[3] = {"今天", "明天", "后天"};
    DailyForecast* forecast = weather_data_daily();
    const bool hasWeather = weather_status() == 1 && forecast;
    Weather* currentWeather = weather_data_now();
    const bool hasLiveWeather = weather_type() == 1 && weather_now_available();
    const bool oldWeather = weather_is_fallback();
    if (oldWeather) {
        u8g2Fonts.setFont(FONT_SUB);
        u8g2Fonts.setForegroundColor(GxEPD_RED);
        centered("天气为上次数据", rightCenter, 183);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    }
    for (int i = 0; i < 3; ++i) {
        const int center = split + 25 + i * 49;
        if (i > 0) display.drawFastVLine(split + i * 49, oldWeather ? 203 : 176,
                                         oldWeather ? 40 : 67, GxEPD_BLACK);
        u8g2Fonts.setFont(FONT_TEXT);
        centered(labels[i], center, oldWeather ? 204 : 195);
        u8g2Fonts.setFont(FONT_TEXT);
        const String condition = i == 0 && hasLiveWeather ? currentWeather->text :
                                 (hasWeather && weather_daily_available(i) ? forecast->weather[i].textDay : "暂无");
        centered(fitText(condition, 45), center, oldWeather ? 222 : 218);
        const String temps = i == 0 && hasLiveWeather ? String(currentWeather->temp) + "°" :
                             (hasWeather && weather_daily_available(i) ? String(forecast->weather[i].tempMax) + "/" +
                                           String(forecast->weather[i].tempMin) : "--/--");
        u8g2Fonts.setFont(u8g2_font_helvB12_tf);
        centered(temps, center, 240);
    }
    display.drawFastHLine(split + 1, 248, 149, GxEPD_BLACK);

    // Next festival and its remaining days are the card's heading.
    int festivalDays = -1;
    String festival = nextFestival(festivalDays);
    if (festivalDays >= 0) {
        const String number = String(festivalDays);
        const uint8_t* numberFont = festivalDays >= 100
            ? u8g2_font_fub17_tn : u8g2_font_fub25_tn;
        u8g2Fonts.setFont(numberFont);
        const int numberWidth = u8g2Fonts.getUTF8Width(number.c_str());
        u8g2Fonts.setFont(FONT_TEXT);
        const int festivalWidth = u8g2Fonts.getUTF8Width(festival.c_str());
        const int unitWidth = u8g2Fonts.getUTF8Width("天");
        const auto layout = festivalInlineLayout(split,150,festivalWidth,numberWidth,unitWidth);
        const int startX = layout.labelX;
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        drawViewText(startX, 283, festival);
        const int numberX = layout.numberX;
        u8g2Fonts.setFont(numberFont);
        u8g2Fonts.setForegroundColor(GxEPD_RED);
        u8g2Fonts.drawUTF8(numberX, 285, number.c_str());
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        u8g2Fonts.drawUTF8(layout.unitX, 283, "天");
    }
}

static void loadDaytimeCourses(String courses[6][8]) {
    const String source = _study_schedule.length() >= 4 && _study_schedule[3] == ';'
                              ? _study_schedule : String(DEFAULT_STUDY_SCHEDULE);
    int begin = 4;
    while (begin < source.length()) {
        int end = source.indexOf(';', begin);
        if (end < 0) end = source.length();
        const String record = source.substring(begin, end);
        const int firstComma = record.indexOf(',');
        if (firstComma > 0) {
            const String day = record.substring(0, firstComma);
            for (int col = 0; col < 6; ++col) {
                if (day != week_str[col + 1]) continue;
                int fieldBegin = firstComma + 1;
                for (int row = 0; row < 8 && fieldBegin <= record.length(); ++row) {
                    int fieldEnd = record.indexOf(',', fieldBegin);
                    if (fieldEnd < 0) fieldEnd = record.length();
                    courses[col][row] = record.substring(fieldBegin, fieldEnd);
                    courses[col][row].trim();
                    fieldBegin = fieldEnd + 1;
                }
                break;
            }
        }
        begin = end + 1;
    }
}

// Six weekdays, eight daytime periods and only the start time in the narrow
// first column. The full start/end routine remains saved in device preferences.
static void drawDaytimeWeekly() {
    const LessonClock clock = getLessonClock();
    const int left = 6;
    const int timeWidth = 46;
    const int dayWidth = 57;
    const int headerTop = 40;
    const int headerHeight = 23;
    const int rowHeight = 29;
    const int coursesTop = headerTop + headerHeight;
    const int gridRight = left + timeWidth + 6 * dayWidth;
    const bool hasDate = tmInfo.tm_year + 1900 >= 2025;
    const bool isSchoolDay = hasDate && tmInfo.tm_wday >= 1 && tmInfo.tm_wday <= 6;
    const int todayColumn = tmInfo.tm_wday - 1;
    String courses[6][8];
    bool custom[6] = {};
    LessonClock dateClocks[6];
    loadDaytimeCourses(courses);
    if (hasDate) for (int col=0;col<6;++col) {
        tm date={};if(!weekColumnDate(tmInfo,col,date))continue;
        const EffectiveDayContext context=effectiveDayForDate(date);
        custom[col] = context.day.independent && !context.override.dayOff;
        dateClocks[col] = context.day.clock;
        for(int row=0;row<8;++row)
            courses[col][row]=context.override.dayOff ? "放假" : context.courses[row];
    }

    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawUTF8(8, 27, "日间周课表");

    char date[12];
    snprintf(date, sizeof(date), "%02d.%02d", tmInfo.tm_mon + 1, tmInfo.tm_mday);
    String dateLabel = hasDate
        ? String(date) + " 周" + week_str[tmInfo.tm_wday] : "时间待同步";
    if (lowBattery) dateLabel = "电量低 · " + dateLabel;
    u8g2Fonts.setFont(FONT_SUB);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    u8g2Fonts.drawUTF8(393 - u8g2Fonts.getUTF8Width(dateLabel.c_str()), 25,
                       dateLabel.c_str());
    display.drawFastHLine(left, 34, 388, GxEPD_RED);

    display.fillRect(left, headerTop, gridRight - left, headerHeight, GxEPD_BLACK);
    if (isSchoolDay)
        display.fillRect(left + timeWidth + todayColumn * dayWidth, headerTop,
                         dayWidth, headerHeight, GxEPD_RED);
    u8g2Fonts.setForegroundColor(GxEPD_WHITE);
    u8g2Fonts.setBackgroundColor(GxEPD_BLACK);
    centered("时间", left + timeWidth / 2, headerTop + 17);
    for (int col = 0; col < 6; ++col) {
        const int x = left + timeWidth + col * dayWidth;
        const uint16_t background = isSchoolDay && col == todayColumn ? GxEPD_RED : GxEPD_BLACK;
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.setForegroundColor(GxEPD_WHITE);
        u8g2Fonts.setBackgroundColor(background);
        centered(String("周") + week_str[col + 1], x + dayWidth / 2, headerTop + 18);
    }

    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    for (int col = 0; col <= 7; ++col) {
        const int x = col == 0 ? left : left + timeWidth + (col - 1) * dayWidth;
        display.drawFastVLine(x, coursesTop, 8 * rowHeight + 1, GxEPD_BLACK);
    }
    for (int row = 0; row <= 8; ++row) {
        const int y = coursesTop + row * rowHeight;
        display.drawFastHLine(left, y, gridRight - left + 1,
                              row == 4 ? GxEPD_RED : GxEPD_BLACK);
        if (row == 4)
            display.drawFastHLine(left, y + 1, gridRight - left + 1, GxEPD_RED);
    }
    for (int row = 0; row < 8; ++row) {
        const int top = coursesTop + row * rowHeight;
        u8g2Fonts.setFont(u8g2_font_helvB12_tf);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        char start[6];
        snprintf(start, sizeof(start), "%02d:%02d", clock.start[row] / 60,
                 clock.start[row] % 60);
        centered(start, left + timeWidth / 2, top + 20);
        for (int col = 0; col < 6; ++col) {
            const int x = left + timeWidth + col * dayWidth;
            if (custom[col]) {
                u8g2Fonts.setFont(FONT_TEXT);
                u8g2Fonts.setForegroundColor(GxEPD_BLACK);
                centered(fitTextEllipsis(courses[col][row].isEmpty() ? String("—") : courses[col][row], dayWidth-4),
                         x + dayWidth/2, top + 16);
                if (!courses[col][row].isEmpty()) {
                    char actual[6]; const int minute = dateClocks[col].start[row];
                    snprintf(actual,sizeof(actual),"%02d:%02d",minute/60,minute%60);
                    u8g2Fonts.setFont(u8g2_font_5x8_tf);
                    centered(actual, x + dayWidth/2, top + 26);
                }
            } else drawLargeSubject(courses[col][row], x + dayWidth / 2,
                                    top + 3, GxEPD_BLACK, dayWidth - 4);
        }
    }
    if (isSchoolDay) {
        drawTodayColumnFrame(display, left + timeWidth + todayColumn * dayWidth,
                             headerTop, dayWidth, headerHeight + 8 * rowHeight + 1,
                             GxEPD_RED);
    }
}

static const char* syncFailureLabel(const String& code) {
    if (code == "wifi") return "Wi-Fi";
    if (code == "connect") return "连接";
    if (code == "auth") return "授权";
    if (code == "service") return "服务";
    if (code == "data") return "数据";
    if (code == "storage") return "存储";
    if (code == "config") return "配置";
    if (code == "time") return "时间";
    if (code == "ack") return "回执";
    return "未知";
}

static void drawTodoPage() {
    const bool hasDate = tmInfo.tm_year + 1900 >= 2025;
    u8g2Fonts.setFontMode(1);
    u8g2Fonts.setFontDirection(0);
    u8g2Fonts.setBackgroundColor(GxEPD_WHITE);
    u8g2Fonts.setFont(FONT_TEXT);
    u8g2Fonts.setForegroundColor(GxEPD_RED);
    u8g2Fonts.drawUTF8(12, 28, "待办事项");
    u8g2Fonts.setFont(FONT_SUB);
    u8g2Fonts.setForegroundColor(GxEPD_BLACK);
    const String countText = _todo_count > 6
        ? "显示前6项 / 共" + String(_todo_count) + "项"
        : "剩余 " + String(_todo_count) + " 项";
    u8g2Fonts.drawUTF8(390 - u8g2Fonts.getUTF8Width(countText.c_str()), 16,
                       countText.c_str());
    String lastTime;
    if (_remote_sync_at >= 1735689600) {
        const time_t syncedAt = static_cast<time_t>(_remote_sync_at);
        struct tm synced = {};
        localtime_r(&syncedAt, &synced);
        char syncedTime[19];
        if (hasDate && synced.tm_year == tmInfo.tm_year &&
            synced.tm_yday == tmInfo.tm_yday)
            snprintf(syncedTime, sizeof(syncedTime), "%02d:%02d",
                     synced.tm_hour, synced.tm_min);
        else
            snprintf(syncedTime, sizeof(syncedTime), "%02d-%02d %02d:%02d",
                     synced.tm_mon + 1, synced.tm_mday, synced.tm_hour, synced.tm_min);
        lastTime = syncedTime;
    }
    String syncText = "尚未同步";
    if (_remote_sync_failed) {
        syncText = _remote_sync_code == "ack" ? "回报异常" :
                   String("同步失败 ") + syncFailureLabel(_remote_sync_code);
    } else if (!lastTime.isEmpty()) syncText = "已同步 " + lastTime;
    if (lowBattery) syncText = "电量低 · " + syncText;
    u8g2Fonts.setForegroundColor(_remote_sync_failed || lowBattery
                                    ? GxEPD_RED : GxEPD_BLACK);
    syncText = fitText(syncText, 300);
    u8g2Fonts.drawUTF8(390 - u8g2Fonts.getUTF8Width(syncText.c_str()), 32, syncText.c_str());
    display.drawFastHLine(10, 38, 380, GxEPD_RED);

    JsonDocument doc;
    if (deserializeJson(doc, _todo_json) || !doc.is<JsonArray>()) {
        u8g2Fonts.setFont(FONT_TEXT);
        centered("待办缓存暂不可用", 200, 150);
        return;
    }
    const JsonArray todos = doc.as<JsonArray>();
    if (todos.size() == 0) {
        u8g2Fonts.setFont(FONT_TEXT);
        centered("暂时没有待办事项", 200, 150);
        u8g2Fonts.setFont(FONT_SUB);
        centered("可在手机配置网页添加", 200, 177);
        return;
    }
    char today[11];
    snprintf(today, sizeof(today), "%04d-%02d-%02d", tmInfo.tm_year + 1900,
             tmInfo.tm_mon + 1, tmInfo.tm_mday);
    struct tm nextDay = tmInfo;
    ++nextDay.tm_mday;
    nextDay.tm_hour = 12;
    nextDay.tm_isdst = -1;
    mktime(&nextDay);
    char tomorrow[11];
    snprintf(tomorrow, sizeof(tomorrow), "%04d-%02d-%02d",
             nextDay.tm_year + 1900, nextDay.tm_mon + 1, nextDay.tm_mday);
    const int visibleCount = todos.size() < 6 ? todos.size() : 6;
    const bool roomy = visibleCount <= 3;
    const int rowHeight = roomy ? 77 : 240 / visibleCount;
    const int firstTop = roomy ? 55 + (3 - visibleCount) * 38 :
                         48 + (240 - visibleCount * rowHeight) / 2;
    int index = 0;
    for (JsonObject todo : todos) {
        if (index >= 6) break;
        const int top = firstTop + index * rowHeight;
        const String title = todo["title"].as<String>();
        const String due = todo["due"].as<String>();
        const bool high = todo["priority"].as<String>() == "high";
        const bool overdue = hasDate && !due.isEmpty() && due < String(today);
        const bool dueToday = hasDate && due == today;
        const bool urgent = overdue || dueToday;
        if (high || (!roomy && overdue))
            display.fillRect(11, top + (roomy ? 10 : 7), 3,
                             roomy ? 45 : rowHeight - 14, GxEPD_RED);
        u8g2Fonts.setFont(FONT_TEXT);
        u8g2Fonts.setForegroundColor(GxEPD_BLACK);
        if (roomy) {
            drawViewText(25, top + 29, fitTextEllipsis(title, high ? 295 : 355));
            if (high) {
                u8g2Fonts.setFont(FONT_SUB);
                u8g2Fonts.setForegroundColor(GxEPD_RED);
                u8g2Fonts.drawUTF8(354, top + 27, "重要");
            }
            if (!due.isEmpty()) {
                String dueText;
                if (dueToday) dueText = "今天截止";
                else {
                    if (overdue) dueText = "已逾期 · ";
                    else if (hasDate && due == tomorrow) dueText = "明天截止 · ";
                    else dueText = "截止 · ";
                    dueText += due.substring(5, 7) + "月" + due.substring(8, 10) + "日";
                }
                u8g2Fonts.setFont(FONT_TEXT);
                u8g2Fonts.setForegroundColor(urgent ? GxEPD_RED : GxEPD_BLACK);
                u8g2Fonts.drawUTF8(26, top + 55, dueText.c_str());
            }
        } else {
            drawViewText(23, top + rowHeight / 2 + 6,
                         fitTextEllipsis(title, due.isEmpty() ? 360 : 290));
            if (!due.isEmpty()) {
                u8g2Fonts.setFont(FONT_SUB);
                u8g2Fonts.setForegroundColor(urgent ? GxEPD_RED : GxEPD_BLACK);
                const String dueText = overdue ? "已逾期" :
                    (dueToday ? "今天截止" :
                     (hasDate && due == tomorrow ? "明天" : due.substring(5)));
                u8g2Fonts.drawUTF8(390 - u8g2Fonts.getUTF8Width(dueText.c_str()),
                                   top + rowHeight / 2 + 5, dueText.c_str());
            }
        }
        if (index + 1 < visibleCount)
            display.drawFastHLine(22, top + rowHeight - 1, 368, GxEPD_BLACK);
        ++index;
    }
}
