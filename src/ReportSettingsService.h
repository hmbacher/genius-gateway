/**
 * @file ReportSettingsService.h
 * @brief Settings service for the PDF report header (property, address, customer).
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version, with the Commons Clause restriction.
 *
 * "Commons Clause" License Condition v1.0
 * The Software is provided to you by the Licensor under the License,
 * as defined below, subject to the following condition:
 * Without limiting other conditions in the License, the grant of rights
 * under the License will not include, and the License does not grant to you,
 * the right to Sell the Software.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 */

#pragma once

#include <HttpEndpoint.h>
#include <FSPersistence.h>
#include <ESP32SvelteKit.h>

#define REPORT_SETTINGS_FILE "/config/report-settings.json" ///< Configuration file path
#define REPORT_SETTINGS_PATH "/rest/report-settings"        ///< REST API service endpoint path

#define REPORT_SETTINGS_MAX_FIELD_LEN 256 ///< Maximum length for any single report field

/// Report header settings data model
class ReportSettings
{
public:
    ReportSettings()
        : propertyName(""),
          propertyAddress(""),
          customerName("")
    {
    }

    String propertyName;    ///< Optional property/site name shown in report header
    String propertyAddress; ///< Optional property address (may be multi-line)
    String customerName;    ///< Optional customer/owner name

    static void read(ReportSettings &settings, JsonObject &root)
    {
        root["propertyName"] = settings.propertyName;
        root["propertyAddress"] = settings.propertyAddress;
        root["customerName"] = settings.customerName;
    }

    static StateUpdateResult update(JsonObject &root, ReportSettings &settings, const String &originId)
    {
        bool changed = false;

        auto updateField = [&](const char *key, String &field) {
            if (root[key].is<String>())
            {
                String newVal = root[key].as<String>();
                newVal = newVal.substring(0, REPORT_SETTINGS_MAX_FIELD_LEN);
                if (field != newVal)
                {
                    field = newVal;
                    changed = true;
                }
            }
        };

        updateField("propertyName", settings.propertyName);
        updateField("propertyAddress", settings.propertyAddress);
        updateField("customerName", settings.customerName);

        return changed ? StateUpdateResult::CHANGED : StateUpdateResult::UNCHANGED;
    }

private:
    static constexpr const char *TAG = "ReportSettings"; ///< Logging tag
};

/// Service for PDF report header settings
class ReportSettingsService : public StatefulService<ReportSettings>
{
public:
    ReportSettingsService(ESP32SvelteKit *sveltekit);

    /// Initialize the report settings service
    void begin();

private:
    static constexpr const char *TAG = "ReportSettingsSvc"; ///< Logging tag

    HttpEndpoint<ReportSettings> _httpEndpoint;   ///< REST API endpoint handler
    FSPersistence<ReportSettings> _fsPersistence; ///< File system persistence handler
};
