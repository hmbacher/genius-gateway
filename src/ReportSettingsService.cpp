/**
 * @file ReportSettingsService.cpp
 * @brief Implementation of the report settings service.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#include <ReportSettingsService.h>

ReportSettingsService::ReportSettingsService(ESP32SvelteKit *sveltekit)
    : _httpEndpoint(ReportSettings::read,
                    ReportSettings::update,
                    this,
                    sveltekit->getServer(),
                    REPORT_SETTINGS_PATH,
                    sveltekit->getSecurityManager(),
                    AuthenticationPredicates::IS_AUTHENTICATED),
      _fsPersistence(ReportSettings::read,
                     ReportSettings::update,
                     this,
                     sveltekit->getFS(),
                     REPORT_SETTINGS_FILE)
{
}

void ReportSettingsService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS();
}
