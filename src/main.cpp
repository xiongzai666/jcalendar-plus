#include <Arduino.h>
#include <ArduinoJson.h>

#include <WiFiManager.h>

#include "esp_sleep.h"
#include "esp_wifi.h"
#include "driver/rtc_io.h"

#include <wiring.h>

#include "battery.h"

#include "led.h"
#include "_sntp.h"
#include "weather.h"
#include "screen_ink.h"
#include "_preference.h"
#include "public_defaults.h"
#include "lesson_clock.h"
#include "schedule_optimization.h"
#include "effective_schedule.h"
#include "runtime_summary.h"
#include "network_budget.h"
#include "remote_sync.h"
#include "remote_retry.h"
#include "date_overrides.h"
#include "page_navigation_core.h"

#include "version.h"

#include "OneButton.h"
OneButton button(KEY_M, true);
OneButton downButton(KEY_DOWN, true);

WiFiManager wm;
WiFiManagerParameter para_qweather_host("qweather_host", "和风天气Host", "", 64); //     和风天气key
WiFiManagerParameter para_qweather_key("qweather_key", "和风天气API Key", "", 32); //     和风天气key
// const char* test_html = "<br/><label for='test'>天气模式</label><br/><input type='radio' name='test' value='0' checked> 每日天气test </input><input type='radio' name='test' value='1'> 实时天气test</input>";
// WiFiManagerParameter para_test(test_html);
WiFiManagerParameter para_qweather_type("qweather_type", "天气类型（0:每日天气，1:实时天气）", "0", 2, "pattern='\\[0-1]{1}'"); //     城市code
WiFiManagerParameter para_qweather_location("qweather_loc", "位置ID", "", 64); //     城市code
WiFiManagerParameter para_cd_day_label("cd_day_label", "倒数日（4字以内）", "", 16); //     倒数日
WiFiManagerParameter para_cd_day_date("cd_day_date", "日期（yyyyMMdd）", "", 8, "pattern='\\d{8}'"); //     城市code
WiFiManagerParameter para_tag_days("tag_days", "日期Tag（yyyyMMddx，详见README）", "", 30); //     日期Tag
WiFiManagerParameter para_si_week_1st("si_week_1st", "每周起始（0:周日，1:周一）", "0", 2, "pattern='\\[0-1]{1}'"); //     每周第一天
WiFiManagerParameter para_study_schedule("study_schedule", "课程表（周一至周六，上午4节／下午4节／晚间4节）", "", 4000);
WiFiManagerParameter para_remote_host("remote_host", "远程配置域名", "", 100);
WiFiManagerParameter para_remote_token("remote_token", "远程设备令牌", "", 129);
WiFiManagerParameter para_remote_ca("remote_ca", "远程站点根证书 PEM", "", 2200);
WiFiManagerParameter para_backup_ssid("backup_ssid", "备用 Wi-Fi 名称", "", 33);
WiFiManagerParameter para_backup_pass("backup_pass", "备用 Wi-Fi 密码（留空保持原密码）", "", 65,
                                      "type='password'");

void print_wakeup_reason() {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("Wakeup caused by external signal using RTC_IO");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
    {
        Serial.println("Wakeup caused by external signal using RTC_CNTL");
        uint64_t status = esp_sleep_get_ext1_wakeup_status();
        if (status == 0) {
            Serial.println(" *None of the configured pins woke us up");
        } else {
            Serial.print(" *Wakeup pin mask: ");
            Serial.printf("0x%016llX\r\n", status);
            for (int i = 0; i < 64; i++) {
                if ((status >> i) & 0x1) {
                    Serial.printf("  - GPIO%d\r\n", i);
                }
            }
        }
        break;
    }
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wakeup caused by timer");
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        Serial.println("Wakeup caused by touchpad");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Serial.println("Wakeup caused by ULP program");
        break;
    default:
        Serial.printf("Wakeup was not caused by deep sleep.\r\n");
    }
}

void buttonClick(void* oneButton);
void downButtonClick(void* oneButton);
void buttonDoubleClick(void* oneButton);
void buttonLongPressStop(void* oneButton);
void go_sleep();

unsigned long _idle_millis;
unsigned long _screen_ready_millis = 0;
unsigned long TIME_TO_SLEEP = 180 * 1000;
const unsigned long PAGE_SELECT_WINDOW = 30000;
const unsigned long TIMER_PAGE_SELECT_WINDOW = 12000;
bool _manual_page_window = true;
constexpr uint32_t WAKE_PLAN_MAGIC = 0x4A434C31;
RTC_DATA_ATTR uint32_t _wake_plan_magic = 0;
RTC_DATA_ATTR time_t _planned_wake_at = 0;
RTC_DATA_ATTR uint8_t _planned_class_wake = 0;
RTC_DATA_ATTR uint8_t _planned_remote_retry = 0;
RTC_DATA_ATTR int64_t _planned_regular_slot = 0;
RTC_DATA_ATTR int64_t _served_regular_slot = 0;
RTC_DATA_ATTR int64_t _planned_weather_slot = 0;
RTC_DATA_ATTR int64_t _served_weather_slot = 0;
RTC_DATA_ATTR RemoteRetryState _remote_retry_state = {};
bool _retry_wake_this_boot = false;
bool _weather_due_this_boot = false;
bool _remote_sync_retry = false;

bool _wifi_flag = false;
unsigned long _wifi_failed_millis;
bool _restart_after_param_save = false;
unsigned long _param_saved_millis = 0;
bool _portal_screen_visible = false;

String devicePortalPassword() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return "";
    String password = pref.getString(PREF_AP_PASSWORD);
    if (password.length() != 12) {
        // Enable the RF source before drawing a new per-device secret.
        WiFi.mode(WIFI_AP);
        delay(20);
        static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        password = "";
        for (int i = 0; i < 12; ++i)
            password += alphabet[esp_random() % (sizeof(alphabet) - 1)];
        pref.putString(PREF_AP_PASSWORD, password);
    }
    pref.end();
    return password;
}

bool connectBackupWiFi() {
    Preferences pref;
    pref.begin(PREF_NAMESPACE, true);
    const String ssid = pref.getString(PREF_BACKUP_SSID);
    const String password = pref.getString(PREF_BACKUP_PASS);
    pref.end();
    if (ssid.isEmpty() || password.length() < 8) return false;

    WiFi.mode(WIFI_STA);
    // WiFiManager has already initialized the driver. Switching Arduino's
    // persistence flag now has no effect on its storage mode, so change the
    // live driver before applying the backup station configuration.
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
        Serial.println("Backup Wi-Fi RAM storage failed.");
        return false;
    }
    Serial.println("Trying backup Wi-Fi...");
    WiFi.begin(ssid.c_str(), password.c_str());
    const unsigned long started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < 12000)
        delay(100);
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Backup Wi-Fi connected.");
        return true;
    }
    Serial.println("Backup Wi-Fi unavailable.");
    return false;
}

bool connectPreferredWiFi() {
    Preferences pref;
    pref.begin(PREF_NAMESPACE, true);
    const uint8_t last = pref.getUChar(PREF_WIFI_LAST, 1);
    pref.end();

    bool connected = false;
    uint8_t winner = 0;
    if (last == 2) {
        // Snapshot the stored primary credentials before using RAM-only
        // station storage for the backup connection.
        WiFi.mode(WIFI_STA);
        wifi_config_t primaryConfig = {};
        const bool hasPrimary = esp_wifi_get_config(WIFI_IF_STA, &primaryConfig) == ESP_OK &&
                                primaryConfig.sta.ssid[0] != 0;
        connected = connectBackupWiFi();
        if (connected) winner = 2;
        else if (hasPrimary) {
            Serial.println("Trying saved primary Wi-Fi...");
            WiFi.begin(reinterpret_cast<const char*>(primaryConfig.sta.ssid),
                       reinterpret_cast<const char*>(primaryConfig.sta.password));
            const unsigned long started = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - started < 10000)
                delay(100);
            connected = WiFi.status() == WL_CONNECTED;
            if (connected) winner = 1;
        }
    } else {
        connected = wm.autoConnect();
        if (connected) winner = 1;
        else {
            connected = connectBackupWiFi();
            if (connected) winner = 2;
        }
    }
    if (connected && winner != last) {
        if (pref.begin(PREF_NAMESPACE)) {
            pref.putUChar(PREF_WIFI_LAST, winner);
            pref.end();
        }
    }
    if (connected) Serial.printf("Preferred Wi-Fi path: %s\n", winner == 2 ? "backup" : "primary");
    return connected;
}

// A manual wake keeps the MCU awake for page selection. If an activity changes
// during that window, sleeping afterwards would skip its scheduled wake.
bool timetableBoundaryPassed(time_t renderedAt, time_t current) {
    if (renderedAt <= 0 || current <= renderedAt) return false;
    struct tm rendered = {}, latest = {};
    localtime_r(&renderedAt, &rendered);
    localtime_r(&current, &latest);
    if (rendered.tm_year != latest.tm_year || rendered.tm_yday != latest.tm_yday)
        return true;
    const DateOverrideInfo override = readDateOverride(latest);
    if (override.dayOff || (latest.tm_wday == 0 && !override.hasLessons)) return false;
    const int from = rendered.tm_hour * 3600 + rendered.tm_min * 60 + rendered.tm_sec;
    const int to = latest.tm_hour * 3600 + latest.tm_min * 60 + latest.tm_sec;
    if (from / 60 == to / 60) return false;
    const EffectiveDayContext context = effectiveDayForDate(latest);
    return !sameEffectiveView(context.day, context.routine.c_str(), from / 60,
                              to / 60, latest.tm_wday);
}

void runtimeSummary(JsonObject report) {
    const time_t now = time(nullptr);
    tm local = {}; localtime_r(&now, &local);
    int64_t next = nextRegularNetworkAt(now, _served_regular_slot);
    const EffectiveDayContext context = effectiveDayForDate(local);
    const int seconds = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
    for (int group = 0; group < 3; ++group) {
        const int finish = effectiveDismissal(context.day, group);
        const int64_t at = int64_t(now) - seconds + (finish - 10) * 60 + 10;
        if (finish >= 0 && at > now && at > _served_weather_slot && at < next) next = at;
    }
    if (_remote_retry_state.phase == RemoteRetryPhase::Pending &&
        _remote_retry_state.dueAt > now && _remote_retry_state.dueAt < next)
        next = _remote_retry_state.dueAt;
    Preferences pref; pref.begin(PREF_NAMESPACE, true);
    report["activePage"] = pref.getInt(PREF_SI_TYPE, 2); pref.end();
    report["firmware"] = J_VERSION;
    report["nowReadAt"] = weather_now_read_at();
    report["dailyReadAt"] = weather_daily_read_at();
    report["nowFailed"] = weather_now_failed();
    report["dailyFailed"] = weather_daily_failed();
    report["nextNetworkAt"] = local.tm_year + 1900 >= 2025 ? next : 0;
}

NetworkBudget& wakeNetworkBudget() { static NetworkBudget budget; return budget; }

void setup() {
    wakeNetworkBudget().begin(millis());
    delay(10);
    Serial.begin(115200);
    Serial.println(".");
    print_wakeup_reason();
    const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
    _manual_page_window = wakeCause != ESP_SLEEP_WAKEUP_TIMER;
    const bool plannedTimerWake = wakeCause == ESP_SLEEP_WAKEUP_TIMER &&
                                  _wake_plan_magic == WAKE_PLAN_MAGIC;
    if (plannedTimerWake) {
        _weather_due_this_boot = _planned_regular_slot || _planned_weather_slot;
        if (_planned_regular_slot) _served_regular_slot = _planned_regular_slot;
        if (_planned_weather_slot) _served_weather_slot = _planned_weather_slot;
    }
    _retry_wake_this_boot = plannedTimerWake && _planned_remote_retry;
    if (plannedTimerWake && !_planned_class_wake && !_planned_remote_retry)
        remoteRetryBeginRegularCycle(_remote_retry_state);
    setenv("TZ", "CST-8", 1);
    tzset();
    Serial.println("\r\n\r\n");
    delay(10);


    Serial.printf("***********************\r\n");
    Serial.printf("      J-Calendar\r\n");
    Serial.printf("    version: %s\r\n", J_VERSION);
    Serial.printf("***********************\r\n\r\n");
    Serial.printf("Copyright © 2022-2025 JADE Software Co., Ltd. All Rights Reserved.\r\n\r\n");

    Preferences defaults;
    defaults.begin(PREF_NAMESPACE);
    initializePublicPreferences(defaults);
    defaults.end();

    remote_recover_pending();

    Preferences startupPref;
    if (startupPref.begin(PREF_NAMESPACE)) {
        const int retained = startupPref.getInt(PREF_SI_TYPE, 2);
        const int configured = startupPref.getInt(PREF_REMOTE_PAGE, 2);
        const int page = pageOnStartup(retained, configured,
                                      wakeCause != ESP_SLEEP_WAKEUP_UNDEFINED);
        if (page != retained) startupPref.putInt(PREF_SI_TYPE, page);
        startupPref.end();
        Serial.printf("Startup page: %d (%s).\n", page,
                      wakeCause == ESP_SLEEP_WAKEUP_UNDEFINED ? "configured home" : "retained after sleep");
    }

    led_init();
    led_on();
    delay(100);
    int voltage = readBatteryVoltage();
    Serial.printf("Battery: %d mV\r\n", voltage);
    if(voltage < 2500) {
        Serial.println("[INFO]电池损坏或无ADC电路。");
    } else if(voltage < 3000) {
        Serial.println("[WARN]电量低于3v，系统休眠。");
        go_sleep();
    } else if (voltage < 3300) {
        // 低于3.3v，电池电量用尽，屏幕给警告，然后关机。
        Serial.println("[WARN]电量低于3.3v，警告并系统休眠。");
        si_warning("电量不足，请充电！");
        go_sleep();
    } else if (voltage > 4400) {
        Serial.println("[INFO]未接电池。");
    }

    button.setClickMs(300);
    button.setPressMs(5000); // 长按上键进入配置
    button.attachClick(buttonClick, &button);
    button.attachDoubleClick(buttonDoubleClick, &button);
    // button.attachMultiClick()
    button.attachLongPressStop(buttonLongPressStop, &button);
    downButton.setClickMs(300);
    downButton.attachClick(downButtonClick, &downButton);

    // The RTC clock survives deep sleep. Lesson-only wakes can redraw using
    // today's cached forecast without powering up Wi-Fi or contacting SNTP.
    time_t localNow = time(nullptr);
    struct tm localDate = {};
    localtime_r(&localNow, &localDate);
    const bool plannedClassWake = wakeCause == ESP_SLEEP_WAKEUP_TIMER &&
        _wake_plan_magic == WAKE_PLAN_MAGIC && _planned_class_wake &&
        localDate.tm_year + 1900 >= 2025 &&
        localNow >= _planned_wake_at - 60 && localNow <= _planned_wake_at + 300;
    if (plannedClassWake) {
        if (!weather_restore_cached()) weather_exec(2);
        Serial.println("Class timer wake: local clock; available weather cache; Wi-Fi skipped.");
        _sntp_exec(SYNC_STATUS_OK);
        led_on();
        return;
    }

    Serial.println("Wm begin...");
    led_fast();
    wm.setHostname("J-Calendar");
    wm.setEnableConfigPortal(false);
    wm.setConnectTimeout(10);
    bool connected = connectPreferredWiFi();
    if (connected) {
        Serial.println("Connect OK.");
        led_on();
        _wifi_flag = true;
    } else {
        Serial.println("Connect failed.");
        noteRemoteSyncFailure("wifi");
        Preferences retryPref;
        if (retryPref.begin(PREF_NAMESPACE, true)) {
            const bool remoteEnabled = retryPref.getString(PREF_REMOTE_TOKEN).length() >= 32;
            retryPref.end();
            if (remoteEnabled) {
                const time_t now = time(nullptr);
                const bool due = remoteRetryIsDue(_remote_retry_state, now,
                                                  _retry_wake_this_boot);
                remoteRetryRecord(_remote_retry_state, RemoteSyncResult::Failed, due, now);
            }
        }
        _wifi_flag = false;
        _wifi_failed_millis = millis();
        led_slow();
        _sntp_exec(2);
        weather_exec(2);
        WiFi.mode(WIFI_OFF); // 提前关闭WIFI，省电
        Serial.println("Wifi closed.");
    }
}

/**
 * 处理各个任务
 * 1. sntp同步
 *      前置条件：Wifi已连接
 * 2. 刷新日历
 *      前置条件：sntp同步完成（无论成功或失败）
 * 3. 刷新天气信息
 *      前置条件：wifi已连接
 * 4. 系统配置
 *      前置条件：无
 * 5. 休眠
 *      前置条件：所有任务都完成或失败，
 */
void loop() {
    static bool remoteChecked = false;
    button.tick(); // 上键：上一页；双击或长按进入配置
    downButton.tick();
    wm.process();
    if (_portal_screen_visible && !wm.getConfigPortalActive() &&
        !_restart_after_param_save && si_screen_status() != 0) {
        _portal_screen_visible = false;
        _screen_ready_millis = 0;
        si_screen();
    }
    // WiFiManager sends its success page after invoking the save callback.
    // Restart only after that HTTP response has had time to reach the phone.
    if (_restart_after_param_save && millis() - _param_saved_millis >= 1500) {
        ESP.restart();
    }
    // 前置任务：wifi已连接
    // sntp同步
    if (_sntp_status() == -1) {
        _sntp_exec();
    }
    // 如果是定时器唤醒，并且接近午夜（23:50之后），则直接休眠
    if (_sntp_status() == SYNC_STATUS_TOO_LATE) {
        go_sleep();
    }
    if (!remoteChecked && _sntp_status() > 0) {
        remoteChecked = true;
        // Course-only timer wakes have Wi-Fi disabled, so this only polls on
        // manual and two-hour network wakes. Cache remains usable offline.
        const time_t now = time(nullptr);
        const bool retryAttempt = remoteRetryIsDue(_remote_retry_state, now,
                                                    _retry_wake_this_boot);
        _remote_sync_retry = retryAttempt;
        const RemoteSyncResult result = remote_sync(retryAttempt);
        remoteRetryRecord(_remote_retry_state, result, retryAttempt, time(nullptr));
        if (result != RemoteSyncResult::Skipped)
            Serial.printf("Remote %s: %s.\n", retryAttempt ? "retry" : "check",
                          result == RemoteSyncResult::Success ? "success" : "failed");
        // Validate the cache after applying remote settings: a changed
        // weather location must fetch new data rather than show the old city.
        if (_retry_wake_this_boot && !_weather_due_this_boot &&
            weather_status() == -1 && weather_restore_cached())
            Serial.println("Remote retry wake: using cached weather.");
    }
    // 前置任务：wifi已连接
    // 获取Weather信息
    // Today's cache needs a valid date. A hard reset can leave the clock at
    // 1970 until SNTP finishes, so do not reject a good cache too early.
    if (weather_status() == -1 && _sntp_status() > 0 &&
        (!_retry_wake_this_boot || remoteChecked)) {
        weather_exec();
    }

    // 刷新日历
    // 前置任务：sntp、weather
    // 执行条件：屏幕状态为待处理
    if (!wm.getConfigPortalActive() && !_portal_screen_visible &&
        _sntp_status() > 0 && weather_status() > 0 && si_screen_status() == -1) {
        const RemoteSyncResult report = remote_finish_report();
        remoteRetryRecord(_remote_retry_state, report, _remote_sync_retry, time(nullptr));
        if (report != RemoteSyncResult::Skipped)
            Serial.printf("Runtime report: %s.\n", report == RemoteSyncResult::Success ? "success" : "failed");
        // 数据获取完毕后，关闭Wifi，省电
        if (!wm.getConfigPortalActive()) {
            WiFi.mode(WIFI_OFF);
        }
        Serial.println("Wifi closed after data fetch.");

        si_screen();
    }

    // 休眠
    // 前置条件：屏幕刷新完成（或成功）

    // 未在配置状态，且屏幕刷新完成，进入休眠
    if (!wm.getConfigPortalActive() && si_screen_status() > 0) {
        static time_t lastBoundaryCheck = 0;
        const time_t current = time(nullptr);
        if (si_screen_status() == 1 && current != lastBoundaryCheck) {
            lastBoundaryCheck = current;
            Preferences pref;
            pref.begin(PREF_NAMESPACE);
            const bool timetablePage = pref.getInt(PREF_SI_TYPE, 2) == 2;
            pref.end();
            if (timetablePage && timetableBoundaryPassed(si_screen_snapshot_time(), current)) {
                Serial.println("Activity changed while awake; refreshing timetable.");
                _screen_ready_millis = 0;
                si_screen();
                return;
            }
        }
        if (_screen_ready_millis == 0) _screen_ready_millis = millis();
        const unsigned long window = _manual_page_window ? PAGE_SELECT_WINDOW : TIMER_PAGE_SELECT_WINDOW;
        if (millis() - _screen_ready_millis < window) {
            delay(10);
            return;
        }
        if (_wifi_flag) {
            go_sleep();
        }
        if (!_wifi_flag && millis() - _wifi_failed_millis > 10 * 1000) { // 如果wifi连接不成功，等待10秒休眠
            go_sleep();
        }
    }
    // 配置状态下，
    if (wm.getConfigPortalActive() && millis() - _idle_millis > TIME_TO_SLEEP) {
        wm.stopConfigPortal();
    }

    delay(10);
}


// 上键回到前一页面；下键进入后一页面。
void switchPage(bool forward) {
    if (wm.getConfigPortalActive() || si_screen_status() != 1) return;
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    int mode = pref.getInt(PREF_SI_TYPE, 2);
    // 顺序：课表+倒计时、日间周课表、月历、待办。
    mode = nextPageInOrder(mode, forward);
    pref.putInt(PREF_SI_TYPE, mode);
    pref.end();
    _screen_ready_millis = 0;
    _manual_page_window = true;
    Serial.printf("Switch to page %d\n", mode);
    si_screen();
}

void buttonClick(void* oneButton) {
    Serial.println("Button click.");
    if (wm.getConfigPortalActive()) {
        Serial.println("In config status.");
    } else {
        switchPage(false);
    }
}

void downButtonClick(void* oneButton) {
    Serial.println("Down button click.");
    switchPage(true);
}

String normalizeRemoteCertificate(const String& pasted) {
    const String header = "-----BEGIN CERTIFICATE-----";
    const String footer = "-----END CERTIFICATE-----";
    const int start = pasted.indexOf(header);
    const int finish = pasted.indexOf(footer);
    if (start < 0 || finish <= start) return "";
    String compact;
    for (int i = start + header.length(); i < finish; ++i) {
        const char c = pasted[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=') compact += c;
    }
    if (compact.length() < 500 || compact.length() > 1800) return "";
    String pem = header + "\n";
    for (size_t i = 0; i < compact.length(); i += 64) {
        pem += compact.substring(i, i + 64) + "\n";
    }
    return pem + footer + "\n";
}

void saveParamsCallback() {
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    pref.putString(PREF_QWEATHER_HOST, para_qweather_host.getValue());
    pref.putString(PREF_QWEATHER_KEY, para_qweather_key.getValue());
    pref.putString(PREF_QWEATHER_TYPE, strcmp(para_qweather_type.getValue(), "1") == 0 ? "1" : "0");
    pref.putString(PREF_QWEATHER_LOC, para_qweather_location.getValue());
    pref.putString(PREF_CD_DAY_LABLE, para_cd_day_label.getValue());
    pref.putString(PREF_CD_DAY_DATE, para_cd_day_date.getValue());
    pref.putString(PREF_TAG_DAYS, para_tag_days.getValue());
    pref.putString(PREF_SI_WEEK_1ST, strcmp(para_si_week_1st.getValue(), "1") == 0 ? "1" : "0");
    pref.putString(PREF_STUDY_SCHEDULE, para_study_schedule.getValue());
    pref.putString(PREF_REMOTE_HOST, para_remote_host.getValue());
    pref.putString(PREF_REMOTE_TOKEN, para_remote_token.getValue());
    const String certificate = normalizeRemoteCertificate(para_remote_ca.getValue());
    if (!certificate.isEmpty()) pref.putString(PREF_REMOTE_CA, certificate);
    const String backupSsid = para_backup_ssid.getValue();
    const String backupPass = para_backup_pass.getValue();
    if (backupSsid.isEmpty()) {
        pref.remove(PREF_BACKUP_SSID);
        pref.remove(PREF_BACKUP_PASS);
    } else {
        pref.putString(PREF_BACKUP_SSID, backupSsid);
        if (!backupPass.isEmpty()) pref.putString(PREF_BACKUP_PASS, backupPass);
    }
    pref.putBool(PREF_SCH_SAMPLE, false);
    pref.end();

    Serial.println("Params saved.");

    _idle_millis = millis(); // 刷新无操作时间点

    _param_saved_millis = millis();
    _restart_after_param_save = true;
}

void preSaveParamsCallback() {
}

// 双击打开配置页面
void buttonDoubleClick(void* oneButton) {
    Serial.println("Button double click.");
    if (wm.getConfigPortalActive()) {
        ESP.restart();
        return;
    }

    if (weather_status() == 0) {
        weather_stop();
    }
    const unsigned long waitStarted = millis();
    while (si_screen_status() == 0 && millis() - waitStarted < 30000) delay(50);
    if (si_screen_status() == 0) {
        Serial.println("Screen busy; config portal opening deferred.");
        return;
    }

    // 设置配置页面
    // 根据配置信息设置默认值
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    String qHost = pref.getString(PREF_QWEATHER_HOST);
    String qToken = pref.getString(PREF_QWEATHER_KEY);
    String qType = pref.getString(PREF_QWEATHER_TYPE, "0");
    String qLoc = pref.getString(PREF_QWEATHER_LOC);
    String cddLabel = pref.getString(PREF_CD_DAY_LABLE);
    String cddDate = pref.getString(PREF_CD_DAY_DATE);
    String tagDays = pref.getString(PREF_TAG_DAYS);
    String week1st = pref.getString(PREF_SI_WEEK_1ST, "0");
    String studySchedule = pref.getString(PREF_STUDY_SCHEDULE);
    String remoteHost = pref.getString(PREF_REMOTE_HOST, "");
    String remoteToken = pref.getString(PREF_REMOTE_TOKEN);
    String remoteCA = pref.getString(PREF_REMOTE_CA);
    String backupSsid = pref.getString(PREF_BACKUP_SSID);
    pref.end();

    para_qweather_host.setValue(qHost.c_str(), 64);
    para_qweather_key.setValue(qToken.c_str(), 32);
    para_qweather_location.setValue(qLoc.c_str(), 64);
    para_qweather_type.setValue(qType.c_str(), 1);
    para_cd_day_label.setValue(cddLabel.c_str(), 16);
    para_cd_day_date.setValue(cddDate.c_str(), 8);
    para_tag_days.setValue(tagDays.c_str(), 30);
    para_si_week_1st.setValue(week1st.c_str(), 1);
    para_study_schedule.setValue(studySchedule.c_str(), 4000);
    para_remote_host.setValue(remoteHost.c_str(), 100);
    para_remote_token.setValue(remoteToken.c_str(), 129);
    para_remote_ca.setValue(remoteCA.c_str(), 2200);
    para_backup_ssid.setValue(backupSsid.c_str(), 33);
    para_backup_pass.setValue("", 65);

    wm.setTitle("J-Calendar");
    wm.addParameter(&para_si_week_1st);
    wm.addParameter(&para_qweather_host);
    wm.addParameter(&para_qweather_key);
    wm.addParameter(&para_qweather_type);
    wm.addParameter(&para_qweather_location);
    wm.addParameter(&para_cd_day_label);
    wm.addParameter(&para_cd_day_date);
    wm.addParameter(&para_tag_days);
    wm.addParameter(&para_study_schedule);
    wm.addParameter(&para_remote_host);
    wm.addParameter(&para_remote_token);
    wm.addParameter(&para_remote_ca);
    wm.addParameter(&para_backup_ssid);
    wm.addParameter(&para_backup_pass);
    // std::vector<const char *> menu = {"wifi","wifinoscan","info","param","custom","close","sep","erase","update","restart","exit"};
    std::vector<const char*> menu = { "wifi","param","update","sep","info","restart","exit" };
    wm.setMenu(menu); // custom menu, pass vector
    wm.setConfigPortalBlocking(false);
    wm.setBreakAfterConfig(true);
    wm.setPreSaveParamsCallback(preSaveParamsCallback);
    wm.setSaveParamsCallback(saveParamsCallback);
    wm.setSaveConnect(false); // 保存完wifi信息后是否自动连接，设置为否，以便于用户继续配置param。
    const String portalPassword = devicePortalPassword();
    if (portalPassword.length() != 12) {
        Serial.println("Config portal password unavailable.");
        return;
    }
    si_portal_screen(portalPassword);
    _portal_screen_visible = true;
    wm.startConfigPortal("J-Calendar", portalPassword.c_str());

    led_config(); // LED 进入三快闪状态

    // 控制配置超时180秒后休眠
    _idle_millis = millis();
}

// 长按上键进入配置页面，保留已有的 Wi-Fi、课表和倒计时设置。
void buttonLongPressStop(void* oneButton) {
    Serial.println("Button long press.");
    buttonDoubleClick(oneButton);
}

#define uS_TO_S_FACTOR 1000000
#define TIMEOUT_TO_SLEEP  10 // seconds
time_t blankTime = 0;
void go_sleep() {
    uint64_t p;
    bool classWake = false;
    bool retryWake = false;
    // 根据配置情况来刷新，如果未配置qweather信息，则24小时刷新，否则每2小时刷新
    Preferences pref;
    pref.begin(PREF_NAMESPACE);
    String _qweather_key = pref.getString(PREF_QWEATHER_KEY, "");
    const bool remoteEnabled = pref.getString(PREF_REMOTE_TOKEN).length() >= 32;
    const int pageMode = pref.getInt(PREF_SI_TYPE, 2);
    const String dailyRoutine = pageMode == 2 ?
        pref.getString(PREF_DAILY_ROUTINE, DEFAULT_DAILY_ROUTINE) : String();
    pref.end();

    time_t now;
    time(&now);
    struct tm local;
    localtime_r(&now, &local);
    if (!remoteEnabled && (_qweather_key.length() == 0 || weather_type() == 0)) { // 无需联网则次日刷新。
        // Sleep to next day
        int secondsToNextDay = (24 - local.tm_hour) * 3600 - local.tm_min * 60 - local.tm_sec;
        Serial.printf("Seconds to next day: %d seconds.\n", secondsToNextDay);
        p = (uint64_t)(secondsToNextDay);
        p = p < 0 ? 3600 * 24 : (p + 30); // 额外增加30秒，避免过早唤醒
    } else {
        p = static_cast<uint64_t>(nextRegularNetworkAt(now, _served_regular_slot) - now);
    }
    const int64_t regularSlot = now + p;
    int64_t weatherSlot = 0;
    int activitySeconds = -1;
    const auto addActivity = [&activitySeconds, now](int64_t at) {
        const int seconds = static_cast<int>(at - now);
        if (seconds > 0 && (activitySeconds < 0 || seconds < activitySeconds))
            activitySeconds = seconds;
    };

    const EffectiveDayContext context = effectiveDayForDate(local);
    const int secondsToday = local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec;
    if (pageMode == 2 && context.day.mask) {
        const int boundary = nextEffectiveBoundary(context.day, context.routine.c_str(),
                                                    secondsToday, local.tm_wday);
        if (boundary >= 0) addActivity(now + boundary - secondsToday + 10);
    }
    if (context.day.mask && !_qweather_key.isEmpty()) {
        const int64_t midnight = now - secondsToday;
        for (int section = 0; section < 3; ++section) {
            const int finish = effectiveDismissal(context.day, section);
            if (finish < 0) continue;
            const int64_t at = midnight + (finish - 10) * 60 + 10;
            if (at > now && at > _served_weather_slot && (!weatherSlot || at < weatherSlot))
                weatherSlot = at;
        }
        if (weatherSlot && weatherSlot - now < static_cast<int64_t>(p))
            p = static_cast<uint64_t>(weatherSlot - now);
    }

    // Preserve lesson-boundary refreshes. If one happens first, the pending
    // retry remains scheduled across that local-only wake.
    int retrySeconds = -1;
    if (_remote_retry_state.phase == RemoteRetryPhase::Pending &&
        !(local.tm_hour == 23 && local.tm_min >= 50)) {
        const time_t remaining = _remote_retry_state.dueAt - now;
        const uint64_t untilRetry = static_cast<uint64_t>(remaining > 0 ? remaining : 0) + 10;
        retrySeconds = static_cast<int>(untilRetry);
    }
    const bool beforeDismissal = weatherSlot && now + static_cast<int64_t>(p) == weatherSlot;
    const RefreshWakePlan plan = chooseRefreshWake(static_cast<int>(p), activitySeconds,
        retrySeconds, beforeDismissal ? REFRESH_MERGE_SECONDS : REGULAR_NETWORK_MERGE_SECONDS);
    p = plan.seconds;
    classWake = !plan.network;
    retryWake = plan.retry;
    const auto closeToWake = [now, p](int64_t slot, int tolerance) {
        const int64_t distance = slot - (now + static_cast<int64_t>(p));
        return slot && distance >= -tolerance && distance <= tolerance;
    };
    _planned_regular_slot = plan.network && closeToWake(regularSlot, REGULAR_NETWORK_MERGE_SECONDS)
        ? regularSlot : 0;
    _planned_weather_slot = plan.network && closeToWake(weatherSlot, REFRESH_MERGE_SECONDS)
        ? weatherSlot : 0;

    // A reboot can lose wall-clock time. Retry Wi-Fi soon, while cached pages
    // remain usable and show no misleading date or active-lesson highlight.
    if (local.tm_year + 1900 < 2025 && p > 15 * 60) {
        p = 15 * 60;
        classWake = false;
    }
    _wake_plan_magic = WAKE_PLAN_MAGIC;
    _planned_class_wake = classWake ? 1 : 0;
    _planned_remote_retry = retryWake ? 1 : 0;
    _planned_wake_at = now + static_cast<time_t>(p);
    Serial.printf("Next wake: %llu s, %s.\n", static_cast<unsigned long long>(p),
                  classWake ? "local class" : retryWake ? "remote retry" : "network/weather");
    esp_sleep_enable_timer_wakeup(p * (uint64_t)uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup(KEY_M, LOW);
    // EXT0 samples the RTC GPIO while the main GPIO controller is asleep.
    // Keep GPIO14 biased high so the upper key reliably creates a falling edge.
    rtc_gpio_init(KEY_M);
    rtc_gpio_set_direction(KEY_M, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(KEY_M);
    rtc_gpio_pullup_en(KEY_M);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

    // 省电考虑，关闭RTC外设和存储器
    // esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF); // RTC IO, sensors and ULP, 注意：由于需要按键唤醒，所以不能关闭，否则会导致RTC_IO唤醒(ext0)失败
    // Keep a few bytes of RTC slow memory for the next wake's purpose.
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_XTAL, ESP_PD_OPTION_OFF);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC8M, ESP_PD_OPTION_OFF);

    gpio_deep_sleep_hold_dis(); // 解除所有引脚的保持状态

    // 省电考虑，重置gpio，平均每针脚能省8ua。
    // gpio_reset_pin(PIN_LED_R); // 减小deep-sleep电流
    gpio_reset_pin(SPI_CS); // 减小deep-sleep电流
    gpio_reset_pin(SPI_DC); // 减小deep-sleep电流
    gpio_reset_pin(SPI_RST); // 减小deep-sleep电流
    gpio_reset_pin(SPI_BUSY); // 减小deep-sleep电流`
    gpio_reset_pin(SPI_MOSI); // 减小deep-sleep电流
    gpio_reset_pin(SPI_MISO); // 减小deep-sleep电流
    gpio_reset_pin(SPI_SCK); // 减小deep-sleep电流
    gpio_reset_pin(KEY_DOWN); // GPIO12 启动配置脚，不能带内部上拉进入下次复位
    // gpio_reset_pin(PIN_ADC); // 减小deep-sleep电流
    // gpio_reset_pin(I2C_SDA); // 减小deep-sleep电流
    // gpio_reset_pin(I2C_SCL); // 减小deep-sleep电流

    delay(10);
    Serial.println("Deep sleep...");
    Serial.flush();
    esp_deep_sleep_start();
}
