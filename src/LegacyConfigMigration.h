/**
 * @file LegacyConfigMigration.h
 * @brief One-shot migrations away from pre-v1.3.0 persisted config files.
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

#include <FS.h>
#include <ArduinoJson.h>

/**
 * @brief Coordinates migrations away from /config/mqtt-settings.json.
 *
 * Pre-v1.3.0 the gateway stored two unrelated concerns inside one file:
 *   1. Home Assistant integration flags (enable, discovery prefix).
 *   2. Simple alarm publishing (enable, alarm topic).
 *
 * As of v1.3.x those are split into two dedicated files
 * (`/config/haSettings.json` and `/config/alarm-publishing.json`) and the
 * legacy file is removed entirely. Because the two consuming services start
 * at different points (HA inside the framework's `begin()`, alarm-publishing
 * inside `GeniusGateway::begin()`), the cleanup of the shared legacy file
 * cannot be owned by either service alone. This helper centralises the
 * deletion: each service calls `cleanupLegacyIfFullyMigrated()` after writing
 * its own post-migration file; whichever call observes that all expected
 * post-migration files now exist performs the legacy delete.
 */
class LegacyConfigMigration
{
public:
    /// Pre-v1.3.0 combined settings file. Removed once all migrations finish.
    static constexpr const char *LEGACY_FILE = "/config/mqtt-settings.json";

    /// Parse the legacy file into `doc`. Returns false if the file is absent
    /// or fails to deserialize. The caller inspects `doc` for the keys it
    /// cares about — services pick out their own subset.
    static bool readLegacy(FS *fs, JsonDocument &doc);

    /// Try to remove the legacy file. The deletion happens only when every
    /// post-migration file the build is expected to produce already exists,
    /// so the call is order-independent and safe to invoke after each
    /// service-level migration.
    static void cleanupLegacyIfFullyMigrated(FS *fs);

private:
    static constexpr const char *TAG = "LegacyMigration";
};
