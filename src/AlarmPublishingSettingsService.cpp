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
                     ALARM_PUBLISHING_SETTINGS_FILE)
{
}

void AlarmPublishingSettingsService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS();
}
