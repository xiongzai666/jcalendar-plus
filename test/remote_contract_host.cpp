#include <cassert>
#include <fstream>
#include <sstream>
#include <cstdio>
#include "remote_contract.h"
int main(int argc, char** argv) {
    assert(argc == 2);
    std::ifstream input(argv[1]); std::stringstream buffer; buffer << input.rdbuf();
    JsonDocument doc;
    assert(!deserializeJson(doc, buffer.str()));
    assert(remote_contract::validPayload(doc.as<JsonVariantConst>()));
    doc["settings"]["countdownDate"] = "20250229";
    assert(!remote_contract::validPayload(doc.as<JsonVariantConst>()));
    assert(!deserializeJson(doc, buffer.str()));
    auto day = doc["settings"]["dateOverrides"][0].to<JsonObject>();
    day["date"] = "2026-10-04"; day["dayOff"] = false; day["mode"] = "custom";
    auto lessons = day["lessons"].to<JsonArray>();
    auto first = lessons.add<JsonObject>();
    first["slot"] = 1; first["subject"] = "自习"; first["start"] = "08:00"; first["end"] = "09:30";
    assert(!remote_contract::validPayload(doc.as<JsonVariantConst>())); // Schema 1 cannot silently ignore custom times.
    doc["schema"] = 2;
    assert(remote_contract::validPayload(doc.as<JsonVariantConst>()));
    auto second = lessons.add<JsonObject>();
    second["slot"] = 2; second["subject"] = "自习"; second["start"] = "09:15"; second["end"] = "11:30";
    assert(!remote_contract::validPayload(doc.as<JsonVariantConst>()));
    second["start"] = "09:45";
    assert(remote_contract::validPayload(doc.as<JsonVariantConst>()));
    second.remove("end");
    assert(!remote_contract::validPayload(doc.as<JsonVariantConst>()));
    assert(!deserializeJson(doc, buffer.str()));
    doc["settings"]["countdownDate"] = "20280229";
    assert(remote_contract::validPayload(doc.as<JsonVariantConst>()));
    doc["settings"]["countdownLabel"] = "😀";
    assert(!remote_contract::validPayload(doc.as<JsonVariantConst>()));
    doc["settings"]["countdownLabel"] = "期末";
    doc["settings"]["dateOverrides"][0]["date"] = "2026-10-02";
    doc["settings"]["dateOverrides"][0]["dayOff"] = false;
    doc["settings"]["dateOverrides"][0]["lessons"][0]["slot"] = 1;
    doc["settings"]["dateOverrides"][0]["lessons"][0]["subject"] = "数学";
    assert(remote_contract::validPayload(doc.as<JsonVariantConst>()));
    doc["settings"]["dateOverrides"][0]["lessons"][1]["slot"] = 1;
    doc["settings"]["dateOverrides"][0]["lessons"][1]["subject"] = "英语";
    assert(!remote_contract::validPayload(doc.as<JsonVariantConst>()));
    puts("remote contract: fixture, dates, Unicode, duplicates, schema gate and custom chronology passed");
}
