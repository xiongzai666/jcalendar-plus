#include "remote_sync.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <SPIFFS.h>
#include <FS.h>
#include <time.h>

#include "_preference.h"
#include "lesson_clock_core.h"
#include "remote_contract.h"
#include "runtime_summary.h"
#include "network_budget.h"

namespace {
constexpr const char* STAGED_CONFIG_FILE = "/remote-config-pending.json";
bool configUnavailable = false;
bool reportPending = false, reportNeedsAck = false, reportRetry = false;
String reportHost, reportToken, reportCa, initialSummary;
uint32_t reportRevision = 0;
const char* reportNetwork = "primary";
}

bool remote_config_unavailable() { return configUnavailable; }


void noteRemoteSyncFailure(const char* code) {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return;
    pref.putBool(PREF_REMOTE_SYNC_ERR, true);
    pref.putString(PREF_REMOTE_ERR_CODE, code);
    const uint8_t count = pref.getUChar(PREF_REMOTE_ERR_COUNT, 0);
    pref.putUChar(PREF_REMOTE_ERR_COUNT, count == 255 ? 255 : count + 1);
    const time_t now = time(nullptr);
    pref.putULong64(PREF_REMOTE_ERR_AT, now >= 1735689600 ? static_cast<uint64_t>(now) : 0);
    pref.end();
}

namespace {

bool validHost(const String& host) {
    if (host.length() < 4 || host.length() > 100) return false;
    for (size_t i = 0; i < host.length(); ++i) {
        const char c = host[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    }
    return host.indexOf('.') > 0;
}

bool validPayload(JsonDocument& doc) {
    return remote_contract::validPayload(doc.as<JsonVariantConst>());
}

bool savePayload(JsonDocument& doc) {
    JsonObject settings = doc["settings"];
    String todos;
    serializeJson(doc["todos"], todos);
    String overrides;
    if (doc["settings"]["dateOverrides"].isNull()) overrides = "[]";
    else serializeJson(doc["settings"]["dateOverrides"], overrides);
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return false;
    bool ok = pref.putString(PREF_CD_DAY_LABLE, settings["countdownLabel"].as<String>()) > 0;
    ok = pref.putString(PREF_CD_DAY_DATE, settings["countdownDate"].as<String>()) > 0 && ok;
    ok = pref.putString(PREF_QWEATHER_LOC, settings["weatherLocation"].as<String>()) > 0 && ok;
    ok = pref.putString(PREF_STUDY_SCHEDULE, settings["studySchedule"].as<String>()) > 0 && ok;
    ok = pref.putString(PREF_DAILY_ROUTINE, settings["dailyRoutine"].as<String>()) > 0 && ok;
    ok = pref.putString(PREF_EARLY_READING, settings["earlyReading"].as<String>()) > 0 && ok;
    ok = pref.putString(PREF_TODO_JSON, todos) > 0 && ok;
    ok = pref.putString(PREF_DATE_OVERRIDES, overrides) > 0 && ok;
    ok = pref.putUChar(PREF_TODO_COUNT, static_cast<uint8_t>(doc["pendingCount"].as<int>())) > 0 && ok;
    if (ok) {
        const int page = settings["defaultPage"].as<int>();
        const int oldPage = pref.getInt(PREF_REMOTE_PAGE, -1);
        if (page != oldPage) {
            ok = pref.putInt(PREF_SI_TYPE, page) > 0 &&
                 pref.putInt(PREF_REMOTE_PAGE, page) > 0;
        }
    }
    if (ok) {
        ok = pref.putBool(PREF_SCH_SAMPLE, false) > 0;
        ok = pref.putUInt(PREF_REMOTE_REV, doc["revision"].as<uint32_t>()) > 0 && ok;
    }
    pref.end();
    if (!ok || !pref.begin(PREF_NAMESPACE, true)) return false;
    ok = pref.getString(PREF_CD_DAY_LABLE) == settings["countdownLabel"].as<String>() &&
         pref.getString(PREF_CD_DAY_DATE) == settings["countdownDate"].as<String>() &&
         pref.getString(PREF_QWEATHER_LOC) == settings["weatherLocation"].as<String>() &&
         pref.getString(PREF_STUDY_SCHEDULE) == settings["studySchedule"].as<String>() &&
         pref.getString(PREF_DAILY_ROUTINE) == settings["dailyRoutine"].as<String>() &&
         pref.getString(PREF_EARLY_READING) == settings["earlyReading"].as<String>() &&
         pref.getString(PREF_TODO_JSON) == todos &&
         pref.getString(PREF_DATE_OVERRIDES) == overrides &&
         pref.getUChar(PREF_TODO_COUNT) == static_cast<uint8_t>(doc["pendingCount"].as<int>()) &&
         pref.getInt(PREF_REMOTE_PAGE, -1) == settings["defaultPage"].as<int>() &&
         pref.getUInt(PREF_REMOTE_REV) == doc["revision"].as<uint32_t>();
    pref.end();
    return ok;
}

bool mountJournal() {
    return SPIFFS.begin(false) || SPIFFS.begin(true);
}

bool readStagedPayload(String& body) {
    if (!mountJournal()) return false;
    File file = SPIFFS.open(STAGED_CONFIG_FILE, FILE_READ);
    if (!file || file.size() == 0 || file.size() > 12000) return false;
    body = file.readString();
    file.close();
    return !body.isEmpty() && body.length() <= 12000;
}

bool stagePayload(const String& body, uint32_t revision) {
    if (!mountJournal()) return false;
    File file = SPIFFS.open(STAGED_CONFIG_FILE, FILE_WRITE);
    if (!file || file.write(reinterpret_cast<const uint8_t*>(body.c_str()), body.length()) !=
                     body.length()) return false;
    file.close();
    String verified;
    if (!readStagedPayload(verified) || verified != body) return false;
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return false;
    const bool marked = pref.putUInt(PREF_REMOTE_PENDING, revision) > 0;
    pref.end();
    return marked;
}

bool finishStagedPayload() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return false;
    const bool cleared = pref.remove(PREF_REMOTE_PENDING);
    pref.end();
    if (cleared) SPIFFS.remove(STAGED_CONFIG_FILE);
    return cleared;
}

void syncSuccess() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return;
    pref.putBool(PREF_REMOTE_SYNC_ERR, false);
    pref.putULong64(PREF_REMOTE_SYNC_AT, static_cast<uint64_t>(time(nullptr)));
    pref.end();
}

struct PendingFailure {
    String code;
    uint8_t count = 0;
    uint32_t at = 0;
};

PendingFailure readPendingFailure() {
    PendingFailure failure;
    Preferences pref;
    if (pref.begin(PREF_NAMESPACE, true)) {
        if (pref.isKey(PREF_REMOTE_ERR_CODE)) {
            failure.code = pref.getString(PREF_REMOTE_ERR_CODE);
            failure.count = pref.getUChar(PREF_REMOTE_ERR_COUNT, 0);
            failure.at = static_cast<uint32_t>(pref.getULong64(PREF_REMOTE_ERR_AT, 0));
        }
        pref.end();
    }
    return failure;
}

void clearPendingFailure() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE)) return;
    pref.remove(PREF_REMOTE_ERR_CODE);
    pref.remove(PREF_REMOTE_ERR_COUNT);
    pref.remove(PREF_REMOTE_ERR_AT);
    pref.end();
}

bool acknowledge(const String& host, const String& token, const String& ca,
                 uint32_t revision, bool retryAttempt, const char* network) {
    const PendingFailure failure = readPendingFailure();
    String payload = "{\"revision\":" + String(revision);
    payload += ",\"network\":\"" + String(network) + "\"";
    if (retryAttempt) payload += ",\"retry\":true";
    if (!failure.code.isEmpty() && failure.count > 0) {
        payload += ",\"lastFailure\":{\"code\":\"" + failure.code +
                   "\",\"count\":" + String(failure.count) +
                   ",\"at\":" + String(failure.at) + "}";
    }
    payload += "}";
    JsonDocument report;
    runtimeSummary(report.to<JsonObject>());
    String runtime; serializeJson(report, runtime);
    payload.remove(payload.length() - 1);
    payload += ",\"runtime\":" + runtime + "}";
    for (int attempt = 0; attempt < 2; ++attempt) {
        BudgetSecureClient client;
        client.enableDnsCandidates();
        client.setCACert(ca.c_str());
        HTTPClient http;
        if (!configureNetworkTimeouts(http, client)) return false;
        if (!http.begin(client, "https://" + host + "/api/device/ack")) continue;
        http.addHeader("Authorization", "Bearer " + token);
        http.addHeader("Content-Type", "application/json");
        const uint32_t ackStarted = millis();
        const uint32_t freeHeap = ESP.getFreeHeap();
        const uint32_t maxBlock = ESP.getMaxAllocHeap();
        const int status = http.POST(payload);
        Serial.printf("Remote ack status: %d\n", status);
        if (status < 0) {
            char detail[128] = {};
            const int code = client.lastError(detail, sizeof(detail));
            Serial.printf("Remote ack transport: %d %s, %lu ms, heap %lu/%lu, Wi-Fi %d\n",
                code, detail, static_cast<unsigned long>(millis() - ackStarted),
                static_cast<unsigned long>(freeHeap), static_cast<unsigned long>(maxBlock), int(WiFi.status()));
        }
        http.end();
        if (status == 204) {
            if (!failure.code.isEmpty()) clearPendingFailure();
            return true;
        }
        if (status > 0) return false;
        delay(300);
    }
    return false;
}

} // namespace

void remote_recover_pending() {
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE, true)) return;
    const uint32_t revision = pref.getUInt(PREF_REMOTE_PENDING, 0);
    pref.end();
    if (!revision) return;
    String body;
    JsonDocument doc;
    if (readStagedPayload(body) && !deserializeJson(doc, body) &&
        validPayload(doc) && doc["revision"].as<uint32_t>() == revision &&
        savePayload(doc) && finishStagedPayload()) {
        Serial.printf("Interrupted remote revision %lu recovered.\n",
                      static_cast<unsigned long>(revision));
        return;
    }
    // A damaged journal cannot be trusted. Request the complete cloud state
    // again, and keep the regular pages hidden until a valid version arrives.
    configUnavailable = true;
    if (pref.begin(PREF_NAMESPACE)) {
        pref.putUInt(PREF_REMOTE_REV, 0);
        pref.end();
    }
    noteRemoteSyncFailure("storage");
    Serial.println("Remote config recovery needs a fresh cloud copy.");
}

RemoteSyncResult remote_sync(bool retryAttempt) {
    reportPending = false;
    if (WiFi.status() != WL_CONNECTED) return RemoteSyncResult::Skipped;
    Preferences pref;
    if (!pref.begin(PREF_NAMESPACE, true)) {
        noteRemoteSyncFailure("storage");
        return RemoteSyncResult::Failed;
    }
    const String host = pref.getString(PREF_REMOTE_HOST, "");
    const String token = pref.getString(PREF_REMOTE_TOKEN);
    const String ca = pref.getString(PREF_REMOTE_CA);
    const uint32_t revision = pref.getUInt(PREF_REMOTE_REV, 0);
    const char* network = pref.getUChar(PREF_WIFI_LAST, 1) == 2 ? "backup" : "primary";
    pref.end();
    if (token.isEmpty()) return RemoteSyncResult::Skipped;
    if (time(nullptr) < 1735689600) {
        noteRemoteSyncFailure("time");
        return RemoteSyncResult::Failed;
    }
    if (!validHost(host) || token.length() < 32 || token.length() > 128 ||
        ca.indexOf("-----BEGIN CERTIFICATE-----") < 0) {
        noteRemoteSyncFailure("config");
        return RemoteSyncResult::Failed;
    }

    const PendingFailure pending = readPendingFailure();
    String url = "https://" + host + "/api/device/state?revision=" + String(revision);
    url += "&maxSchema=2";
    url += "&network=" + String(network);
    JsonDocument report;
    runtimeSummary(report.to<JsonObject>());
    String summary; serializeJson(report, summary);
    url += "&runtime=";
    for (const unsigned char c : summary) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') url += char(c);
        else { char escape[4]; snprintf(escape,sizeof(escape),"%%%02X",c); url += escape; }
    }
    if (retryAttempt) url += "&retry=1";
    if (!pending.code.isEmpty() && pending.count > 0)
        url += "&failureCode=" + pending.code + "&failureCount=" + String(pending.count) +
               "&failureAt=" + String(pending.at);
    String body;
    for (int attempt = 0; attempt < 2; ++attempt) {
        BudgetSecureClient client;
        client.enableDnsCandidates();
        client.setCACert(ca.c_str());
        HTTPClient http;
        if (!configureNetworkTimeouts(http, client)) {
            noteRemoteSyncFailure("connect"); return RemoteSyncResult::Failed;
        }
        if (!http.begin(client, url)) {
            Serial.println("Remote HTTPS setup failed.");
            if (attempt == 0) { delay(300); continue; }
            noteRemoteSyncFailure("connect");
            return RemoteSyncResult::Failed;
        }
        http.addHeader("Authorization", "Bearer " + token);
        const int status = http.GET();
        if (status == 204) {
            http.end();
            if (!pending.code.isEmpty()) clearPendingFailure();
            Serial.println("Remote config unchanged.");
            syncSuccess();
            reportPending = true; reportNeedsAck = false; reportRetry = retryAttempt;
            reportHost = host; reportToken = token; reportCa = ca; reportRevision = revision;
            reportNetwork = network; initialSummary = summary;
            return RemoteSyncResult::Success;
        }
        if (status == 200) {
            const bool complete = readBoundedBody(http, body, 11500);
            http.end();
            if (!complete) { noteRemoteSyncFailure("data"); return RemoteSyncResult::Failed; }
            if (!pending.code.isEmpty()) clearPendingFailure();
            break;
        }
        Serial.printf("Remote sync status: %d\n", status);
        if (status < 0) {
            char tlsError[128] = {};
            const int tlsCode = client.lastError(tlsError, sizeof(tlsError));
            Serial.printf("Remote TLS error: %d %s\n", tlsCode, tlsError);
        }
        http.end();
        if (status >= 0 || attempt == 1) {
            noteRemoteSyncFailure(status == 401 || status == 403 ? "auth" :
                                  status < 0 ? "connect" : "service");
            return RemoteSyncResult::Failed;
        }
        delay(300);
    }
    if (body.length() > 12000) {
        Serial.println("Remote config oversized.");
        noteRemoteSyncFailure("data");
        return RemoteSyncResult::Failed;
    }
    JsonDocument doc;
    if (deserializeJson(doc, body) || !validPayload(doc)) {
        Serial.println("Remote config invalid; cached settings retained.");
        noteRemoteSyncFailure("data");
        return RemoteSyncResult::Failed;
    }
    if (!stagePayload(body, doc["revision"].as<uint32_t>())) {
        Serial.println("Remote config staging failed; current settings retained.");
        noteRemoteSyncFailure("storage");
        return RemoteSyncResult::Failed;
    }
    if (!savePayload(doc) || !finishStagedPayload()) {
        configUnavailable = true;
        Serial.println("Remote config apply interrupted; staged copy retained.");
        noteRemoteSyncFailure("storage");
        return RemoteSyncResult::Failed;
    }
    configUnavailable = false;
    const uint32_t applied = doc["revision"].as<uint32_t>();
    Serial.printf("Remote config applied: revision %lu\n", static_cast<unsigned long>(applied));
    reportPending = true; reportNeedsAck = true; reportRetry = retryAttempt;
    reportHost = host; reportToken = token; reportCa = ca; reportRevision = applied;
    reportNetwork = network; initialSummary = summary;
    return RemoteSyncResult::Success;
}

RemoteSyncResult remote_finish_report() {
    if (!reportPending || WiFi.status() != WL_CONNECTED) return RemoteSyncResult::Skipped;
    reportPending = false;
    JsonDocument report;
    runtimeSummary(report.to<JsonObject>());
    String currentSummary; serializeJson(report,currentSummary);
    const bool needed = reportNeedsAck || currentSummary != initialSummary;
    const bool ok = !needed || acknowledge(reportHost, reportToken, reportCa,
        reportRevision, reportRetry, reportNetwork);
    reportHost = reportToken = reportCa = initialSummary = "";
    if (!ok) {
        noteRemoteSyncFailure("ack");
        return RemoteSyncResult::Failed;
    }
    syncSuccess();
    return RemoteSyncResult::Success;
}
