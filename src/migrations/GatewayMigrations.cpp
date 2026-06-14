/**
 * @file GatewayMigrations.cpp
 * @brief Genius Gateway config migrations.
 *
 * Pre-v1.3.0 the gateway stored two unrelated concerns in /config/mqtt-settings.json:
 *   1. Home Assistant integration flags (HAIntegrationEnabled, HAMQTTDiscoveryPrefix)
 *   2. Simple alarm publishing (alarmEnabled, alarmTopic)
 * As of v1.3.0 these split into /config/haSettings.json and
 * /config/alarm-publishing.json. The legacy file is dropped post-load once
 * both successor files exist.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#include <migrations/GatewayMigrations.h>
#include <migrations/MigrationService.h>

#include <ArduinoJson.h>
#include <FS.h>
#include <Features.h>
#include <SettingValue.h>
#include <esp_log.h>

#include <AlarmPublishingSettingsService.h>
#if FT_ENABLED(FT_HOME_ASSISTANT)
#include <HomeAssistant/HASettingsService.h>
#endif

namespace
{
constexpr const char *TAG = "GwMigrations";
constexpr const char *LEGACY_FILE = "/config/mqtt-settings.json";

bool readLegacy(FS *fs, JsonDocument &doc)
{
    if (!fs->exists(LEGACY_FILE))
        return false;
    File legacy = fs->open(LEGACY_FILE, "r");
    if (!legacy)
        return false;
    DeserializationError err = deserializeJson(doc, legacy);
    legacy.close();
    return err == DeserializationError::Ok;
}

#if FT_ENABLED(FT_HOME_ASSISTANT)
bool writeHASettingsFromLegacy(FS *fs)
{
    JsonDocument legacy;
    if (!readLegacy(fs, legacy))
        return false;

    bool hasLegacy = legacy["HAIntegrationEnabled"].is<bool>() ||
                     legacy["HAMQTTDiscoveryPrefix"].is<String>();
    if (!hasLegacy)
    {
        ESP_LOGI(TAG, "legacy file lacks HA keys — nothing to migrate");
        return true; // mark applied to avoid re-checking forever
    }

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["enabled"] = legacy["HAIntegrationEnabled"] | FACTORY_HA_ENABLED;
    String prefix = legacy["HAMQTTDiscoveryPrefix"] | FACTORY_HA_DISCOVERY_PREFIX;
    if (!prefix.endsWith("/"))
        prefix += "/";
    root["discovery_prefix"] = prefix;
    root["device_name"] = SettingValue::format(FACTORY_HA_DEVICE_NAME);
    root["manufacturer"] = FACTORY_HA_MANUFACTURER;
    root["model"] = FACTORY_HA_MODEL;

    File out = fs->open(HA_SETTINGS_FILE, "w");
    if (!out)
        return false;
    size_t n = serializeJson(doc, out);
    out.close();
    if (n == 0)
        return false;

    ESP_LOGI(TAG, "wrote %s from legacy %s (enabled=%d, prefix=%s)",
             HA_SETTINGS_FILE, LEGACY_FILE,
             (bool)root["enabled"], prefix.c_str());
    return true;
}
#endif

bool writeAlarmPublishingFromLegacy(FS *fs)
{
    JsonDocument legacy;
    if (!readLegacy(fs, legacy))
        return false;

    bool hasLegacy = legacy["alarmEnabled"].is<bool>() ||
                    legacy["alarmTopic"].is<String>();
    if (!hasLegacy)
    {
        ESP_LOGI(TAG, "legacy file lacks alarm-publishing keys — nothing to migrate");
        return true;
    }

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["alarmEnabled"] = legacy["alarmEnabled"] | false;
    String topic = legacy["alarmTopic"] | String(DEFAULT_ALARM_PUBLISHING_TOPIC);
    root["alarmTopic"] = topic;

    File out = fs->open(ALARM_PUBLISHING_SETTINGS_FILE, "w");
    if (!out)
        return false;
    size_t n = serializeJson(doc, out);
    out.close();
    if (n == 0)
        return false;

    ESP_LOGI(TAG, "wrote %s from legacy %s (enabled=%d, topic=%s)",
             ALARM_PUBLISHING_SETTINGS_FILE, LEGACY_FILE,
             (bool)root["alarmEnabled"], topic.c_str());
    return true;
}
} // namespace

void registerGatewayMigrations(MigrationService &service)
{
#if FT_ENABLED(FT_HOME_ASSISTANT)
    {
        Migration m;
        m.id = "v1.3-split-mqtt-settings-ha";
        m.phase = MigrationPhase::PreServiceBegin;
        m.order = 10;
        m.shouldRun = [](FS *fs) {
            return fs->exists(LEGACY_FILE) && !fs->exists(HA_SETTINGS_FILE);
        };
        m.apply = writeHASettingsFromLegacy;
        service.add(std::move(m));
    }
#endif

    {
        Migration m;
        m.id = "v1.3-split-mqtt-settings-alarm";
        m.phase = MigrationPhase::PreServiceBegin;
        m.order = 11;
        m.shouldRun = [](FS *fs) {
            return fs->exists(LEGACY_FILE) && !fs->exists(ALARM_PUBLISHING_SETTINGS_FILE);
        };
        m.apply = writeAlarmPublishingFromLegacy;
        service.add(std::move(m));
    }

    {
        Migration m;
        m.id = "v1.3-drop-legacy-mqtt-settings";
        m.phase = MigrationPhase::PostServiceBegin;
        m.order = 100;
        m.shouldRun = [](FS *fs) {
            if (!fs->exists(LEGACY_FILE))
                return false;
#if FT_ENABLED(FT_HOME_ASSISTANT)
            if (!fs->exists(HA_SETTINGS_FILE))
                return false;
#endif
            return fs->exists(ALARM_PUBLISHING_SETTINGS_FILE);
        };
        m.apply = [](FS *fs) {
            bool ok = fs->remove(LEGACY_FILE);
            if (ok)
                ESP_LOGI(TAG, "removed legacy %s", LEGACY_FILE);
            return ok;
        };
        service.add(std::move(m));
    }
}
