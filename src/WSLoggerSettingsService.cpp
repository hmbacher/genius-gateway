/// @file WSLoggerSettingsService.cpp
/// @brief Implementation of WebSocket logger settings service
/// 
/// @copyright Copyright (c) 2024-2025 Genius Gateway Project
/// @license AGPL-3.0 with Commons Clause
/// 
/// This file is part of Genius Gateway.
/// 
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU Affero General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version, with the Commons Clause restriction.
/// 
/// "Commons Clause" License Condition v1.0
/// The Software is provided to you by the Licensor under the License,
/// as defined below, subject to the following condition:
/// Without limiting other conditions in the License, the grant of rights
/// under the License will not include, and the License does not grant to you,
/// the right to Sell the Software.
/// 
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU Affero General Public License for more details.
/// 
/// See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.

#include <WSLoggerSettingsService.h>

WSLoggerSettingsService::WSLoggerSettingsService(ESP32SvelteKit *sveltekit) : _httpEndpoint(WSLoggerSettings::read,
                                                                                            WSLoggerSettings::update,
                                                                                            this,
                                                                                            sveltekit->getServer(),
                                                                                            WSLOGGER_SETTINGS_PATH,
                                                                                            sveltekit->getSecurityManager(),
                                                                                            AuthenticationPredicates::IS_AUTHENTICATED),
                                                                              _fsPersistence(WSLoggerSettings::read,
                                                                                             WSLoggerSettings::update,
                                                                                             this,
                                                                                             sveltekit->getFS(),
                                                                                             WSLOGGER_SETTINGS_FILE)
{
}

void WSLoggerSettingsService::begin()
{
    _httpEndpoint.begin();
    _fsPersistence.readFromFS();
}