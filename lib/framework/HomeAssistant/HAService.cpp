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
#include <HomeAssistant/HADevice.h>
#include <HomeAssistant/HAUtils.h>

HAService::HAService(PsychicMqttClient *mqttClient)
    : _mqttClient(mqttClient),
      _deviceName(APP_NAME),
      _manufacturer(""),
      _model(""),
      _discoveryPrefix(HA_DEFAULT_DISCOVERY_PREFIX),
      _enabled(false),
      _renameEntityIds(false),
      _publishTaskHandle(nullptr),
      _registryMutex(xSemaphoreCreateRecursiveMutex())
{
    // Topic identity is derived from the compile-time firmware name, never from
    // the user-editable display name. Renaming the device in the UI therefore
    // leaves both the MQTT topics and HA's device registry entry untouched.
    _deviceId = HAUtils::slugify(APP_NAME, '-', false) + "-" + SettingValue::getUniqueId();
    _updateLegacyTopicIdentity();
}

// Defined here (not =default in the header) so that the unique_ptr<HADevice>
// member can see the complete HADevice type at destruction time.
HAService::~HAService() = default;

void HAService::begin()
{
    if (_publishTaskHandle != nullptr)
    {
        ESP_LOGW(TAG, "begin() called twice - ignoring");
        return;
    }

    BaseType_t ok = xTaskCreate(
        _publishTaskImpl,
        "ha_publish",
        6144,
        this,
        1,
        &_publishTaskHandle);

    if (ok != pdPASS || _publishTaskHandle == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create HA publish task");
        return;
    }

    if (_mqttClient != nullptr)
    {
        _mqttClient->onConnect([this](bool /*sessionPresent*/)
                               {
                                   if (_publishTaskHandle != nullptr)
                                   {
                                       xTaskNotifyGive(_publishTaskHandle);
                                   } });
    }

    ESP_LOGI(TAG, "HA publish task started, listening for MQTT connect notifications");
}

void HAService::_publishTaskImpl(void *param)
{
    static_cast<HAService *>(param)->_publishTask();
}

void HAService::_publishTask()
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Publish requested (MQTT connect or settings change) - invoking publishAll()");
        publishAll();
    }
}

// ============================================================================
// Registry lock
// ============================================================================

void HAService::lock()
{
    if (_registryMutex != nullptr)
        xSemaphoreTakeRecursive(_registryMutex, portMAX_DELAY);
}

void HAService::unlock()
{
    if (_registryMutex != nullptr)
        xSemaphoreGiveRecursive(_registryMutex);
}

// ============================================================================
// Configuration
// ============================================================================

// The setters change strings that publishAll() reads in its own task.

void HAService::setDeviceName(const String &name)
{
    Lock guard(this);
    _deviceName = name;
    _updateLegacyTopicIdentity();
    _syncMainDeviceIdentity();
}

void HAService::setManufacturer(const String &manufacturer)
{
    Lock guard(this);
    _manufacturer = manufacturer;
    _syncMainDeviceIdentity();
}

void HAService::setModel(const String &model)
{
    Lock guard(this);
    _model = model;
    _syncMainDeviceIdentity();
}

void HAService::setDiscoveryPrefix(const String &prefix)
{
    Lock guard(this);
    _discoveryPrefix = prefix;
    // Ensure prefix ends with /
    if (!_discoveryPrefix.endsWith("/"))
    {
        _discoveryPrefix += "/";
    }
    // The legacy base topic embeds the prefix, so it has to follow along.
    _updateLegacyTopicIdentity();
}

void HAService::setEnabled(bool enabled)
{
    _enabled = enabled;
    ESP_LOGI(TAG, "HA integration %s", enabled ? "enabled" : "disabled");
}

void HAService::setRenameEntityIds(bool enabled)
{
    _renameEntityIds = enabled;
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
    return _discoveryPrefix + _deviceId;
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

    // Retire the pre-v1.5.1 config first so HA drops the old entity before the
    // new one arrives - otherwise it would keep both and suffix the new entity
    // ID with "_2".
    purgeLegacyConfig(component, objectId, _deviceId);

    int result = _mqttClient->publish(configTopic.c_str(), 0, true, payload.c_str(), 0, false);

    if (result != -1)
    {
        ESP_LOGI(TAG, "Published %s/%s config (%d bytes)", component.c_str(), objectId.c_str(), payload.length());
        return true;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish %s/%s config", component.c_str(), objectId.c_str());
        return false;
    }
}

void HAService::purgeLegacyConfig(const String &component, const String &objectId, const String &deviceId)
{
    // Sub-devices always used their own (unchanged) ID as node_id, so only the
    // main device's discovery topics moved in v1.5.1.
    if (!_legacyPurgeArmed || _legacyDeviceId.isEmpty() || deviceId != _deviceId)
        return;

    if (_mqttClient == nullptr || !_mqttClient->connected())
        return;

    String legacyTopic = _discoveryPrefix + component + "/" + _legacyDeviceId +
                         "/" + objectId + "/config";
    _mqttClient->publish(legacyTopic.c_str(), 0, true, "", 0, false);
    ESP_LOGD(TAG, "Cleared legacy config topic %s", legacyTopic.c_str());
}

bool HAService::publish(const String &topic, const String &payload, int qos, bool retain, bool async)
{
    if (_mqttClient == nullptr || !_mqttClient->connected())
        return false;

    _purgeLegacyState(topic);

    return _mqttClient->publish(topic.c_str(), qos, retain, payload.c_str(), payload.length(), async) != -1;
}

void HAService::_purgeLegacyState(const String &topic)
{
    if (!_legacyPurgeArmed || _legacyBaseTopic.isEmpty())
        return;

    // Only topics under our own base topic have a legacy counterpart; discovery
    // config topics live under {prefix}{component}/... and are handled by
    // purgeLegacyConfig() instead.
    const String base = getBaseTopic();
    if (!topic.startsWith(base))
        return;

    String legacyTopic = _legacyBaseTopic + topic.substring(base.length());
    _mqttClient->publish(legacyTopic.c_str(), 0, true, "", 0, false);
    ESP_LOGD(TAG, "Cleared legacy state topic %s", legacyTopic.c_str());
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

HAService::CallbackId HAService::onPublishAll(PublishAllCallback callback)
{
    Lock guard(this);
    CallbackId id = _nextCallbackId++;
    _publishCallbacks.emplace_back(id, std::move(callback));
    return id;
}

HAService::CallbackId HAService::onUnpublishAll(PublishAllCallback callback)
{
    Lock guard(this);
    CallbackId id = _nextCallbackId++;
    _unpublishCallbacks.emplace_back(id, std::move(callback));
    return id;
}

void HAService::removePublishAllCallback(CallbackId id)
{
    Lock guard(this);
    auto it = std::find_if(_publishCallbacks.begin(), _publishCallbacks.end(),
                           [id](const CallbackEntry &e) { return e.first == id; });
    if (it != _publishCallbacks.end())
        _publishCallbacks.erase(it);
}

void HAService::removeUnpublishAllCallback(CallbackId id)
{
    Lock guard(this);
    auto it = std::find_if(_unpublishCallbacks.begin(), _unpublishCallbacks.end(),
                           [id](const CallbackEntry &e) { return e.first == id; });
    if (it != _unpublishCallbacks.end())
        _unpublishCallbacks.erase(it);
}

void HAService::publishAll()
{
    if (!_enabled)
        return;

    if (_mqttClient == nullptr || !_mqttClient->connected())
    {
        ESP_LOGW(TAG, "publishAll() called but MQTT is not connected");
        return;
    }

    // Remember what is registered right now, then publish it piece by piece. The
    // registry lock is held per callback and per device only: another task that adds
    // or removes a device waits for one of them at most, never for the whole run.
    std::vector<CallbackId> callbackIds;
    std::vector<const HADevice *> subDevices;
    {
        Lock guard(this);
        callbackIds.reserve(_publishCallbacks.size());
        for (const CallbackEntry &entry : _publishCallbacks)
            callbackIds.push_back(entry.first);
        subDevices.reserve(_subDevices.size());
        for (const auto &dev : _subDevices)
            subDevices.push_back(dev.get());
    }

    ESP_LOGI(TAG, "Publishing all HA entities (%d callbacks, main+%d sub-devices)",
             (int)callbackIds.size(), (int)subDevices.size());

    // The integration can be switched off while this runs (the caller holds the registry
    // lock around unpublishAll() and setEnabled(false)). Checking under the same lock
    // makes this run stop there: what it sent before is removed by that unpublishAll(),
    // and nothing is sent after it.
    for (CallbackId id : callbackIds)
    {
        {
            Lock guard(this);
            if (!_enabled)
                return;
            _invokeCallback(_publishCallbacks, id);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    {
        Lock guard(this);
        if (!_enabled)
            return;
        if (_mainDevice)
        {
            _syncMainDeviceIdentity();
            _mainDevice->publishAll();
        }
    }

    for (const HADevice *candidate : subDevices)
    {
        {
            Lock guard(this);
            if (!_enabled)
                return;
            // Publish the device only if it is still registered - it may have been
            // removed, and destroyed, since the list was taken. The pointer is merely
            // compared here, never dereferenced.
            for (auto &dev : _subDevices)
            {
                if (dev.get() == candidate)
                {
                    dev->publishAll();
                    break;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // Everything has now been (re)published under the current topic tree, and
    // each publish cleared its pre-v1.5.1 counterpart on the way. Nothing left
    // to purge - stop paying for the check on every subsequent publish.
    if (_legacyPurgeArmed)
    {
        ESP_LOGI(TAG, "Legacy HA topic purge complete");
        _legacyPurgeArmed = false;
    }
}

void HAService::requestPublishAll()
{
    if (_publishTaskHandle != nullptr)
        xTaskNotifyGive(_publishTaskHandle);
    else
        publishAll();
}

void HAService::unpublishAll()
{
    if (!_enabled)
        return;

    if (_mqttClient == nullptr || !_mqttClient->connected())
    {
        ESP_LOGW(TAG, "unpublishAll() called but MQTT is not connected");
        return;
    }

    // Same pattern as publishAll(): take the lists, then work through them with the
    // registry lock held per callback and per device.
    std::vector<CallbackId> callbackIds;
    std::vector<const HADevice *> subDevices;
    {
        Lock guard(this);
        callbackIds.reserve(_unpublishCallbacks.size());
        for (const CallbackEntry &entry : _unpublishCallbacks)
            callbackIds.push_back(entry.first);
        subDevices.reserve(_subDevices.size());
        for (const auto &dev : _subDevices)
            subDevices.push_back(dev.get());
    }

    ESP_LOGI(TAG, "Unpublishing all HA entities (%d callbacks, main+%d sub-devices)",
             (int)callbackIds.size(), (int)subDevices.size());

    for (CallbackId id : callbackIds)
        _invokeCallback(_unpublishCallbacks, id);

    {
        Lock guard(this);
        if (_mainDevice)
            _mainDevice->unpublishAll();
    }

    for (const HADevice *candidate : subDevices)
    {
        Lock guard(this);
        for (auto &dev : _subDevices)
        {
            if (dev.get() == candidate)
            {
                dev->unpublishAll();
                break;
            }
        }
    }
}

void HAService::_invokeCallback(const std::vector<CallbackEntry> &callbacks, CallbackId id)
{
    Lock guard(this);

    auto it = std::find_if(callbacks.begin(), callbacks.end(),
                           [id](const CallbackEntry &e) { return e.first == id; });
    if (it == callbacks.end() || !it->second)
        return; // deregistered since the list was taken

    // Run a copy: the callback may register or remove callbacks itself, which would
    // move or destroy the vector element it is running from.
    PublishAllCallback callback = it->second;
    callback();
}

// ============================================================================
// Devices
// ============================================================================

HADevice &HAService::mainDevice()
{
    Lock guard(this);
    _ensureMainDevice();
    return *_mainDevice;
}

HADevice *HAService::addSubDevice(std::unique_ptr<HADevice> device)
{
    if (!device)
        return nullptr;

    Lock guard(this);

    // Wire in the main device relationship if not already set by the caller.
    HADeviceIdentity &id = device->identity();
    if (id.viaDevice.isEmpty())
        id.viaDevice = _deviceId;
    if (id.topicNamespace.isEmpty())
        id.topicNamespace = _deviceId;

    HADevice *raw = device.get();
    _subDevices.push_back(std::move(device));

    // Publish immediately when MQTT is already connected.
    if (isReady())
    {
        ESP_LOGI(TAG, "Adding sub-device '%s' - publishing immediately", raw->getDeviceId().c_str());
        raw->publishAll();
    }
    else
    {
        ESP_LOGI(TAG, "Adding sub-device '%s' - will publish on next connect", raw->getDeviceId().c_str());
    }

    return raw;
}

bool HAService::removeSubDevice(const String &deviceId)
{
    // Holding the lock makes this wait for a publishAll() that is busy with the
    // device, so the device is never destroyed underneath it.
    Lock guard(this);

    for (auto it = _subDevices.begin(); it != _subDevices.end(); ++it)
    {
        if (*it && (*it)->getDeviceId() == deviceId)
        {
            ESP_LOGI(TAG, "Removing sub-device '%s'", deviceId.c_str());
            (*it)->unpublishAll();
            _subDevices.erase(it);
            return true;
        }
    }

    ESP_LOGW(TAG, "removeSubDevice('%s'): device not found", deviceId.c_str());
    return false;
}

void HAService::_ensureMainDevice()
{
    if (_mainDevice)
        return;

    HADeviceIdentity identity;
    identity.id = _deviceId;
    identity.name = _deviceName;
    identity.manufacturer = _manufacturer;
    identity.model = _model;
    identity.swVersion = String(APP_VERSION);
    identity.hwVersion = String(HW_VERSION);
    // Empty namespace - the main device sits directly under the discovery
    // prefix at {prefix}{deviceId}.
    identity.topicNamespace = String();

    _mainDevice = std::unique_ptr<HADevice>(new HADevice(this, std::move(identity)));
}

void HAService::_syncMainDeviceIdentity()
{
    if (!_mainDevice)
        return;

    HADeviceIdentity &id = _mainDevice->identity();
    id.id = _deviceId;
    id.name = _deviceName;
    id.manufacturer = _manufacturer;
    id.model = _model;
    id.swVersion = String(APP_VERSION);
    id.hwVersion = String(HW_VERSION);
    id.topicNamespace = String();

    IPAddress localIP = WiFi.localIP();
    if (IPUtils::isSet(localIP))
    {
        id.configurationUrl = "http://" + localIP.toString() + "/";
    }
}

// ============================================================================
// Private methods
// ============================================================================

void HAService::_updateLegacyTopicIdentity()
{
    const String legacyNamespace = HAUtils::slugify(_deviceName, '-', false);
    const String legacyDeviceId  = legacyNamespace + "-" + SettingValue::getUniqueId();
    const String legacyBaseTopic = _discoveryPrefix + legacyNamespace + "/" + legacyDeviceId;

    // Store each value only if it actually differs - an identical value means
    // there is nothing stale at that address to clear. The two are independent:
    // a display name of exactly APP_NAME yields the same device ID but still a
    // different base topic, because the old scheme carried a namespace level.
    _legacyDeviceId  = (legacyDeviceId == _deviceId) ? String() : legacyDeviceId;
    _legacyBaseTopic = (legacyBaseTopic == getBaseTopic()) ? String() : legacyBaseTopic;
}
