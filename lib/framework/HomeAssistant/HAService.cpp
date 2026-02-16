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

#include <HomeAssistant/HAService.h>

HAService::HAService(PsychicMqttClient *mqttClient)
    : _mqttClient(mqttClient),
      _deviceName(APP_NAME),
      _manufacturer(""),
      _model(""),
      _discoveryPrefix(HA_DEFAULT_DISCOVERY_PREFIX),
      _enabled(false)
{
    _generateDeviceId();
    _topicNamespace = _slugify(_deviceName);
}

// ============================================================================
// Configuration
// ============================================================================

void HAService::setDeviceName(const String &name)
{
    _deviceName = name;
    _topicNamespace = _slugify(name);
    _generateDeviceId();
}

void HAService::setManufacturer(const String &manufacturer)
{
    _manufacturer = manufacturer;
}

void HAService::setModel(const String &model)
{
    _model = model;
}

void HAService::setDiscoveryPrefix(const String &prefix)
{
    _discoveryPrefix = prefix;
    // Ensure prefix ends with /
    if (!_discoveryPrefix.endsWith("/"))
    {
        _discoveryPrefix += "/";
    }
}

void HAService::setEnabled(bool enabled)
{
    _enabled = enabled;
    ESP_LOGI(TAG, "HA integration %s", enabled ? "enabled" : "disabled");
}

// ============================================================================
// State queries
// ============================================================================

bool HAService::isReady() const
{
    return _enabled && _mqttClient != nullptr && _mqttClient->connected();
}

String HAService::getBaseTopic() const
{
    return _discoveryPrefix + _topicNamespace + "/" + _deviceId;
}

// ============================================================================
// MQTT Discovery helpers
// ============================================================================

void HAService::addDeviceInfo(JsonDocument &doc) const
{
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = _deviceId;
    device["name"] = _deviceName;

    if (!_manufacturer.isEmpty())
    {
        device["manufacturer"] = _manufacturer;
    }

    if (!_model.isEmpty())
    {
        device["model"] = _model;
    }

    String swVersion = String(APP_VERSION);
    if (!swVersion.isEmpty())
    {
        device["sw_version"] = swVersion;
    }

    String hwVersion = String(HW_VERSION);
    if (!hwVersion.isEmpty())
    {
        device["hw_version"] = hwVersion;
    }

    // Add configuration URL if we have a valid IP
    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
    {
        device["configuration_url"] = "http://" + localIP.toString() + "/";
    }
}

bool HAService::publishConfig(const String &component, const String &objectId, JsonDocument &config)
{
    if (!isReady())
        return false;

    // Set "~" abbreviation base
    config["~"] = getBaseTopic();

    // Add device info if not already present
    if (!config["device"].is<JsonObject>())
    {
        addDeviceInfo(config);
    }

    // Build discovery topic: {prefix}{component}/{deviceId}/{objectId}/config
    String configTopic = _discoveryPrefix + component + "/" + _deviceId + "/" + objectId + "/config";

    String payload;
    serializeJson(config, payload);

    int result = _mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str());

    if (result != -1)
    {
        ESP_LOGV(TAG, "Published %s/%s config", component.c_str(), objectId.c_str());
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish %s/%s config", component.c_str(), objectId.c_str());
        return false;
    }
}

bool HAService::publish(const String &topic, const String &payload, int qos, bool retain)
{
    if (_mqttClient == nullptr || !_mqttClient->connected())
        return false;

    return _mqttClient->publish(topic.c_str(), qos, retain, payload.c_str()) != -1;
}

void HAService::subscribe(const String &topic,
                          std::function<void(char *, char *, int, int, bool)> callback,
                          int qos)
{
    if (_mqttClient == nullptr)
        return;

    _mqttClient->onTopic(topic.c_str(), qos, callback);
}

// ============================================================================
// Entity lifecycle management
// ============================================================================

void HAService::onPublishAll(PublishAllCallback callback)
{
    _publishCallbacks.push_back(callback);
}

void HAService::publishAll()
{
    if (!isReady())
    {
        ESP_LOGW(TAG, "publishAll() called but HA integration is not ready");
        return;
    }

    ESP_LOGI(TAG, "Publishing all HA entities (%d registered callbacks)", _publishCallbacks.size());

    for (auto &callback : _publishCallbacks)
    {
        callback();
    }
}

// ============================================================================
// Private methods
// ============================================================================

void HAService::_generateDeviceId()
{
    _deviceId = _slugify(_deviceName) + "-" + SettingValue::getUniqueId();
}

String HAService::_slugify(const String &name)
{
    String slug = name;
    slug.toLowerCase();

    // Replace spaces and underscores with dashes
    slug.replace(" ", "-");
    slug.replace("_", "-");

    // Remove any characters that aren't alphanumeric or dash
    String clean;
    clean.reserve(slug.length());
    for (unsigned int i = 0; i < slug.length(); i++)
    {
        char c = slug.charAt(i);
        if (isalnum(c) || c == '-')
        {
            clean += c;
        }
    }

    // Remove consecutive dashes
    while (clean.indexOf("--") >= 0)
    {
        clean.replace("--", "-");
    }

    // Remove leading/trailing dashes
    while (clean.startsWith("-"))
        clean = clean.substring(1);
    while (clean.endsWith("-"))
        clean = clean.substring(0, clean.length() - 1);

    return clean;
}
