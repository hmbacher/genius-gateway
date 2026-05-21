/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2025 theelims
 *   Copyright (C) 2026 hmbacher
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <HomeAssistant/HASettingsService.h>

HASettingsService::HASettingsService(PsychicHttpServer *server,
                                     FS *fs,
                                     SecurityManager *securityManager,
                                     HAService *haService)
    : _httpEndpoint(HASettings::read,
                    HASettings::update,
                    this,
                    server,
                    HA_SETTINGS_SERVICE_PATH,
                    securityManager,
                    AuthenticationPredicates::IS_ADMIN),
      _fsPersistence(HASettings::read,
                     HASettings::update,
                     this,
                     fs,
                     HA_SETTINGS_FILE),
      _fs(fs),
      _haService(haService)
{
    addUpdateHandler([this](const String &originId)
                     { _applyToHAService(); },
                     false);
}

void HASettingsService::begin()
{
    _httpEndpoint.begin();
    _migrateLegacyHASettings();
    _fsPersistence.readFromFS();
    _applyToHAService();
}

// Pre-v1.3.0 the HA enable flag and discovery prefix lived in the gateway-MQTT
// settings file under different key names. On first boot after the upgrade,
// copy those values into the new haSettings.json so the user doesn't have to
// re-enable HA and re-enter the discovery prefix.
void HASettingsService::_migrateLegacyHASettings()
{
    constexpr const char *LEGACY_FILE = "/config/mqtt-settings.json";
    constexpr const char *LEGACY_ENABLED_KEY = "HAIntegrationEnabled";
    constexpr const char *LEGACY_PREFIX_KEY = "HAMQTTDiscoveryPrefix";

    // New file already present → nothing to migrate.
    if (_fs->exists(HA_SETTINGS_FILE))
        return;

    if (!_fs->exists(LEGACY_FILE))
        return;

    File legacy = _fs->open(LEGACY_FILE, "r");
    if (!legacy)
        return;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, legacy);
    legacy.close();
    if (error != DeserializationError::Ok)
        return;

    bool hasLegacyFields = doc[LEGACY_ENABLED_KEY].is<bool>() || doc[LEGACY_PREFIX_KEY].is<String>();
    if (!hasLegacyFields)
        return;

    _state.enabled = doc[LEGACY_ENABLED_KEY] | FACTORY_HA_ENABLED;
    _state.discoveryPrefix = doc[LEGACY_PREFIX_KEY] | FACTORY_HA_DISCOVERY_PREFIX;
    _state.deviceName = SettingValue::format(FACTORY_HA_DEVICE_NAME);
    _state.manufacturer = FACTORY_HA_MANUFACTURER;
    _state.model = FACTORY_HA_MODEL;

    if (!_state.discoveryPrefix.endsWith("/"))
        _state.discoveryPrefix += "/";

    _fsPersistence.writeToFS();

    ESP_LOGI(TAG, "Migrated legacy HA settings from %s (enabled=%d, prefix=%s)",
             LEGACY_FILE, _state.enabled, _state.discoveryPrefix.c_str());
}

void HASettingsService::_applyToHAService()
{
    if (_haService == nullptr)
        return;

    bool wasReady = _haService->isReady();

    // Empty device_name falls back to APP_NAME (compile-time firmware name)
    String name = _state.deviceName.isEmpty() ? String(APP_NAME) : _state.deviceName;

    _haService->setDeviceName(name);
    _haService->setManufacturer(_state.manufacturer);
    _haService->setModel(_state.model);
    _haService->setDiscoveryPrefix(_state.discoveryPrefix);

    // Remove from HA before disabling — must happen while MQTT is still connected
    if (wasReady && !_state.enabled)
    {
        _haService->unpublishAll();
    }

    _haService->setEnabled(_state.enabled);

    ESP_LOGI(TAG, "Applied HA settings (enabled=%d, prefix=%s, device=%s)",
             _state.enabled,
             _state.discoveryPrefix.c_str(),
             name.c_str());

    // Republish on settings change if MQTT is connected
    if (_haService->isReady())
    {
        _haService->publishAll();
    }
}
