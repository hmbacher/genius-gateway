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

#include <HomeAssistant/HADevice.h>
#include <HomeAssistant/HAService.h>
#include <HomeAssistant/HAUtils.h>
#include <IPUtils.h>

HADevice::HADevice(HAService *haService, HADeviceIdentity identity)
    : _haService(haService),
      _identity(std::move(identity)),
      _configPublishFailures(0)
{
}

// ============================================================================
// Identity / topic helpers
// ============================================================================

void HADevice::setEntityIdPrefix(const String &prefix)
{
    // Slugifying is idempotent, so a ready-made prefix passes through unchanged.
    _entityIdPrefix = HAUtils::slugify(prefix);
}

String HADevice::getBaseTopic() const
{
    // topicNamespace is empty for the main device and the main device's ID for
    // a sub-device, giving:
    //   main device: {prefix}{id}
    //   sub-device:  {prefix}{mainDeviceId}/{id}
    if (_identity.topicNamespace.isEmpty())
        return _haService->getDiscoveryPrefix() + _identity.id;

    return _haService->getDiscoveryPrefix() + _identity.topicNamespace + "/" + _identity.id;
}

// ============================================================================
// Lifecycle
// ============================================================================

void HADevice::publishAll()
{
    if (_haService == nullptr || !_haService->isReady())
        return;

    ESP_LOGI("HADevice", "Publishing %u entities for device '%s'",
             (unsigned)_entities.size(), _identity.id.c_str());

    for (auto &entity : _entities)
    {
        if (entity)
            entity->publishAll();
    }
}

void HADevice::unpublishAll(int qos)
{
    if (_haService == nullptr || !_haService->isReady())
        return;

    ESP_LOGI("HADevice", "Unpublishing %u entities for device '%s'",
             (unsigned)_entities.size(), _identity.id.c_str());

    for (auto &entity : _entities)
    {
        if (!entity)
            continue;

        // Send empty retained payload → HA removes the device.
        // Topic: {prefix}{component}/{id}/{objectId}/config
        // Note: use only device ID as node_id (HA requires a single-level node_id,
        // no slashes allowed per MQTT discovery spec).
        String configTopic = _haService->getDiscoveryPrefix() + entity->component() +
                             "/" + _identity.id + "/" + entity->objectId() + "/config";
        _haService->publish(configTopic, String(), qos, true, false);
    }
}

// ============================================================================
// MQTT helpers used by entities of this device
// ============================================================================

bool HADevice::publishConfig(const String &component,
                             const String &objectId,
                             JsonDocument &config)
{
    if (_haService == nullptr || !_haService->isReady())
    {
        _configPublishFailures++;
        return false;
    }

    // "~" resolves to this device's base topic inside discovery payloads.
    config["~"] = getBaseTopic();

    // Explicit entity ID "<component>.<prefix>_<slug(entity name)>" (see setEntityIdPrefix).
    if (!_entityIdPrefix.isEmpty() && !config["default_entity_id"].is<const char *>())
    {
        String entityId = _entityIdPrefix;
        const String entitySlug = HAUtils::slugify(String(config["name"] | ""));
        if (!entitySlug.isEmpty())
            entityId += "_" + entitySlug;
        config["default_entity_id"] = component + "." + entityId;
    }

    // Attach device identity block if not already present.
    if (!config["device"].is<JsonObject>())
    {
        _identity.writeDeviceBlock(config);

        // If configurationUrl wasn't set explicitly, fall back to current IP
        // so the device links to the gateway's web UI.
        if (_identity.configurationUrl.isEmpty())
        {
            IPAddress localIP = WiFi.localIP();
            if (IPUtils::isSet(localIP))
                config["device"]["configuration_url"] = "http://" + localIP.toString() + "/";
        }
    }

    // Use only device ID as node_id (HA discovery requires a single-level node_id,
    // no slashes allowed per spec). The ~ base topic uses the full namespace path.
    String configTopic = _haService->getDiscoveryPrefix() + component +
                         "/" + _identity.id + "/" + objectId + "/config";

    String payload;
    serializeJson(config, payload);

    // Retire the pre-v1.5.1 config first so HA drops the old entity before the
    // new one arrives - otherwise it would keep both and suffix the new entity
    // ID with "_2". No-op for sub-devices, whose node_id never changed.
    _haService->purgeLegacyConfig(component, objectId, _identity.id);

    int result = _haService->getMqttClient()->publish(
        configTopic.c_str(), 0, true, payload.c_str(), 0, false);

    if (result != -1)
    {
        ESP_LOGI("HADevice", "Published %s/%s config (%u bytes) for device '%s'",
                 component.c_str(), objectId.c_str(), payload.length(), _identity.id.c_str());
        return true;
    }
    else
    {
        ESP_LOGE("HADevice", "Failed to publish %s/%s config for device '%s'",
                 component.c_str(), objectId.c_str(), _identity.id.c_str());
        _configPublishFailures++;
        return false;
    }
}

bool HADevice::publish(const String &topic,
                       const String &payload,
                       int qos,
                       bool retain,
                       bool async)
{
    return _haService->publish(topic, payload, qos, retain, async);
}
