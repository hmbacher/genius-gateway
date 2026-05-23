/**
 * @file AlarmPublishingSettingsService.cpp
 * @brief Implementation of the alarm-publishing settings service.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#include <AlarmPublishingSettingsService.h>

#include <LegacyConfigMigration.h>

AlarmPublishingSettingsService::AlarmPublishingSettingsService(ESP32SvelteKit *sveltekit)
    : _httpEndpoint(AlarmPublishingSettings::read,
                    AlarmPublishingSettings::update,
                    this,
                    sveltekit->getServer(),
                    ALARM_PUBLISHING_SETTINGS_PATH,
                    sveltekit->getSecurityManager(),
                    AuthenticationPredicates::IS_AUTHENTICATED),
      _fsPersistence(AlarmPublishingSettings::read,
                     AlarmPublishingSettings::update,
                     this,
                     sveltekit->getFS(),
                     ALARM_PUBLISHING_SETTINGS_FILE),
      _fs(sveltekit->getFS())
{
}

void AlarmPublishingSettingsService::begin()
{
    _httpEndpoint.begin();
    _migrateLegacySettings();
    _fsPersistence.readFromFS();
    LegacyConfigMigration::cleanupLegacyIfFullyMigrated(_fs);
}

void AlarmPublishingSettingsService::_migrateLegacySettings()
{
    if (_fs->exists(ALARM_PUBLISHING_SETTINGS_FILE))
        return;

    JsonDocument doc;
    if (!LegacyConfigMigration::readLegacy(_fs, doc))
        return;

    bool hasLegacyFields = doc["alarmEnabled"].is<bool>() || doc["alarmTopic"].is<String>();
    if (!hasLegacyFields)
        return;

    _state.alarmEnabled = doc["alarmEnabled"] | false;
    _state.alarmTopic = doc["alarmTopic"] | String(DEFAULT_ALARM_PUBLISHING_TOPIC);

    _fsPersistence.writeToFS();

    ESP_LOGI(TAG, "Migrated legacy alarm-publishing settings from %s (enabled=%d, topic=%s)",
             LegacyConfigMigration::LEGACY_FILE, _state.alarmEnabled, _state.alarmTopic.c_str());
}
