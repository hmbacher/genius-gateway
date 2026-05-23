/**
 * @file LegacyConfigMigration.cpp
 * @brief Implementation of the shared legacy-config migration helper.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#include <LegacyConfigMigration.h>

#include <esp_log.h>
#include <Features.h>
#include <AlarmPublishingSettingsService.h>
#if FT_ENABLED(FT_HOME_ASSISTANT)
#include <HomeAssistant/HASettingsService.h>
#endif

bool LegacyConfigMigration::readLegacy(FS *fs, JsonDocument &doc)
{
    if (!fs->exists(LEGACY_FILE))
        return false;

    File legacy = fs->open(LEGACY_FILE, "r");
    if (!legacy)
        return false;

    DeserializationError error = deserializeJson(doc, legacy);
    legacy.close();
    return error == DeserializationError::Ok;
}

void LegacyConfigMigration::cleanupLegacyIfFullyMigrated(FS *fs)
{
    if (!fs->exists(LEGACY_FILE))
        return;

#if FT_ENABLED(FT_HOME_ASSISTANT)
    if (!fs->exists(HA_SETTINGS_FILE))
        return;
#endif
    if (!fs->exists(ALARM_PUBLISHING_SETTINGS_FILE))
        return;

    if (fs->remove(LEGACY_FILE))
        ESP_LOGI(TAG, "Removed legacy %s — all post-migration files present", LEGACY_FILE);
    else
        ESP_LOGW(TAG, "Failed to remove legacy %s", LEGACY_FILE);
}
