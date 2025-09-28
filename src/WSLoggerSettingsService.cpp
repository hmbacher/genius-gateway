/// @file WSLoggerSettingsService.cpp
/// @brief Implementation of WebSocket logger settings service

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