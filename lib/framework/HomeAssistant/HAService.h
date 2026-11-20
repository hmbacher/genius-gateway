#pragma once

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

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PsychicMqttClient.h>
#include <WiFi.h>
#include <IPUtils.h>
#include <SettingValue.h>
#include <functional>
#include <memory>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <HomeAssistant/HADeviceIdentity.h>

class HADevice;

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

#ifndef HW_VERSION
#define HW_VERSION ""
#endif

#ifndef APP_NAME
#define APP_NAME "ESP32 SvelteKit"
#endif

#define HA_DEFAULT_DISCOVERY_PREFIX "homeassistant/"

/**
 * StatefulService update originId used by HA endpoints to mark updates that
 * originated from a Home Assistant command. Lets downstream update handlers
 * distinguish HA-triggered changes from local/HTTP/Event changes.
 */
#define HA_ORIGIN_ID "ha"

/**
 * @brief Home Assistant MQTT Discovery service
 *
 * Manages a single HA device and provides helpers for:
 * - Device identity and config JSON generation
 * - MQTT discovery topic formatting
 * - Entity config publishing with automatic device info attachment
 * - State publishing and command subscription helpers
 *
 * Framework services (HAUpdateService, HADiagnosticService) and project code
 * use this shared utility to register and manage HA entities.
 *
 * Usage:
 * @code
 * // In ESP32SvelteKit or project code:
 * haService->setDeviceName("My Device");
 * haService->setManufacturer("My Company");
 * haService->setModel("Model X");
 * haService->setEnabled(true);
 * haService->setDiscoveryPrefix("homeassistant/");
 *
 * // Register publish callback for custom entities
 * haService->onPublishAll([this]() {
 *     // Publish custom entity configs and states here
 * });
 * @endcode
 */
class HAService
{
public:
    static constexpr const char *TAG = "HAService";

    using PublishAllCallback = std::function<void()>;
    using CallbackId = uint32_t;
    static constexpr CallbackId INVALID_CALLBACK_ID = UINT32_MAX;

    HAService(PsychicMqttClient *mqttClient);
    ~HAService();

    /**
     * @brief Initialize the service
     *
     * Starts a persistent publish task that blocks on a task notification and
     * calls publishAll() when woken. Registers an MQTT onConnect handler that
     * notifies (wakes) this task on every (re)connect.
     *
     * Routing publishAll() through a dedicated task - instead of calling it
     * directly from the MQTT event callback - keeps the MQTT event thread free,
     * which is required for synchronous (async=false) publishes during the
     * publish-all sequence to avoid deadlocking on the same thread.
     */
    void begin();

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set the device display name for Home Assistant
     *
     * Purely cosmetic - this feeds `device.name` in the discovery payload only.
     * MQTT topics and the device identity are derived from APP_NAME, so
     * renaming is safe and never moves topics or re-registers the device.
     *
     * @param name Device name (e.g., "Genius Gateway")
     */
    void setDeviceName(const String &name);

    /**
     * @brief Set the device manufacturer
     * @param manufacturer Manufacturer name
     */
    void setManufacturer(const String &manufacturer);

    /**
     * @brief Set the device model
     * @param model Model name
     */
    void setModel(const String &model);

    /**
     * @brief Set the MQTT discovery prefix
     * @param prefix Discovery prefix (e.g., "homeassistant/")
     */
    void setDiscoveryPrefix(const String &prefix);

    /**
     * @brief Enable or disable HA integration
     * @param enabled true to enable
     */
    void setEnabled(bool enabled);

    /**
     * @brief Opt-in: let sub-device entity IDs follow the user-defined label
     *
     * This service only stores the flag. Sub-device owners read it via
     * renameEntityIds() and are responsible for the rest: naming the HA device
     * after the label (e.g. a detector's location), sending explicit entity
     * IDs (HADevice::setEntityIdPrefix) and, when the label changes, clearing
     * the retained discovery configs before publishing them again so HA adopts
     * the new entity IDs. References to the old IDs in automations, scripts
     * and dashboards have to be updated by the user.
     *
     * @param enabled true to enable
     */
    void setRenameEntityIds(bool enabled);

    /** @brief Whether sub-device entity IDs follow the user-defined label (see setRenameEntityIds) */
    bool renameEntityIds() const { return _renameEntityIds; }

    // ========================================================================
    // State queries
    // ========================================================================

    /** @brief Check if HA integration is enabled */
    bool isEnabled() const { return _enabled; }

    /** @brief Check if HA integration is enabled and MQTT is connected */
    bool isReady() const;

    /** @brief Get the unique device identifier (e.g., "genius-gateway-aabbcc") */
    String getDeviceId() const { return _deviceId; }

    /** @brief Get the MQTT discovery prefix (e.g., "homeassistant/") */
    String getDiscoveryPrefix() const { return _discoveryPrefix; }

    /**
     * @brief Get the base topic for state/command topics
     *
     * Used as MQTT "~" abbreviation base in discovery configs.
     * Format: {discoveryPrefix}{deviceId}
     * Example: "homeassistant/genius-gateway-aabbcc"
     */
    String getBaseTopic() const;

    /** @brief Get the MQTT client */
    PsychicMqttClient *getMqttClient() const { return _mqttClient; }

    // ========================================================================
    // MQTT Discovery helpers
    // ========================================================================

    /**
     * @brief Add device info block to a discovery config document
     *
     * Adds the "device" object with identifiers, name, manufacturer, model,
     * sw_version, hw_version, and configuration_url.
     *
     * @param doc JsonDocument to modify (adds "device" key)
     */
    void addDeviceInfo(JsonDocument &doc) const;

    /**
     * @brief Publish a discovery config for an entity
     *
     * Automatically sets the "~" abbreviation to baseTopic and adds device info
     * if not already present. The config topic follows HA convention:
     * {discoveryPrefix}{component}/{deviceId}/{objectId}/config
     *
     * @param component HA component type ("sensor", "switch", "button", "update", "binary_sensor")
     * @param objectId Unique object suffix (e.g., "free_heap", "restart")
     * @param config Pre-filled JsonDocument with entity config
     * @return true if publish succeeded
     */
    bool publishConfig(const String &component, const String &objectId, JsonDocument &config);

    /**
     * @brief Clear the pre-v1.5.1 discovery config topic for an entity.
     *
     * Up to v1.5.0 the discovery node_id was derived from the user-editable
     * display name, so upgrading moves every main-device config topic. Called
     * right after a successful config publish, this sends an empty retained
     * payload to the topic the entity used to live at, so Home Assistant drops
     * the stale duplicate instead of showing it as an unavailable ghost.
     *
     * No-op for sub-devices (their node_id never contained the display name)
     * and once the first full publishAll() after boot has completed.
     *
     * @param component HA component type the config was published under
     * @param objectId  Entity object ID
     * @param deviceId  node_id the config was published under
     */
    void purgeLegacyConfig(const String &component, const String &objectId, const String &deviceId);

    /**
     * @brief Publish to a specific MQTT topic
     * @param topic Full MQTT topic
     * @param payload Message payload
     * @param qos QoS level (default 0)
     * @param retain Retain flag (default true)
     * @param async Asynchronous publish (default true). Set to false for synchronous publish.
     * @return true if publish succeeded
     */
    bool publish(const String &topic, const String &payload, int qos = 0, bool retain = true, bool async = true);

    /**
     * @brief Subscribe to a MQTT topic with callback
     * @param topic Full MQTT topic
     * @param callback Message handler
     * @param qos QoS level (default 0)
     */
    void subscribe(const String &topic,
                   std::function<void(char *, char *, int, int, bool)> callback,
                   int qos = 0);

    // ========================================================================
    // Entity lifecycle management
    // ========================================================================

    /**
     * @brief Register a callback to be invoked during publishAll()
     *
     * Used by helpers that publish their own configs without going through
     * an HADevice (e.g., HAGroupedSensorPublisher / HAGroupedSwitchPublisher).
     * For per-entity registration, prefer HADevice::registerControl/Config/
     * Diagnostic, which routes through this service automatically.
     *
     * @param callback Function to call during publishAll()
     */
    CallbackId onPublishAll(PublishAllCallback callback);

    /**
     * @brief Register a callback to be invoked during unpublishAll()
     *
     * Used by HAGroupedSensorPublisher and HAGroupedSwitchPublisher to hook
     * into the service-level unpublish triggered when HA integration is disabled.
     *
     * @param callback Function to call during unpublishAll()
     */
    CallbackId onUnpublishAll(PublishAllCallback callback);

    /**
     * @brief Deregister a callback
     *
     * Returns only after a run of that callback that is in progress in another
     * task has finished, so the object behind it can be destroyed right after.
     */
    void removePublishAllCallback(CallbackId id);
    void removeUnpublishAllCallback(CallbackId id);

    /**
     * @brief Publish all registered entities
     *
     * Called on MQTT connect. Invokes all registered onPublishAll callbacks
     * and asks the main HADevice (and any sub-devices) to publish their
     * entities. Only executes if HA integration is enabled and MQTT is
     * connected.
     *
     * The registry lock (see Lock) is taken per callback and per device, not
     * for the whole sequence: other tasks can add or remove devices while a
     * publish-all runs and wait for one device at most. A device or callback
     * that was removed in the meantime is skipped.
     */
    void publishAll();

    /**
     * @brief Remove all registered entities from Home Assistant
     *
     * Sends empty retained payloads to every discovery topic so HA deletes
     * the device and all its entities. Must be called while isReady() is true
     * (i.e. before setEnabled(false)).
     */
    void unpublishAll();

    // ========================================================================
    // Registry lock
    // ========================================================================

    /**
     * @brief Scoped lock on the registry of HA objects
     *
     * The registry is everything publishAll() walks: the publish/unpublish
     * callbacks, the main device and the sub-devices with their entities.
     * publishAll() runs in its own task, so adding, removing or modifying
     * those objects from another task has to be serialised with it.
     *
     * HAService takes the lock itself in addSubDevice(), removeSubDevice(),
     * onPublishAll(), onUnpublishAll(), the remove...Callback() functions and
     * the identity setters. Project code takes it explicitly whenever it
     *  - changes a registered device in place (e.g. HADevice::identity()), or
     *  - destroys an object whose callback publishAll() may be running.
     *
     * Lock order: take this lock BEFORE any mutex that a state reader or a
     * publish callback takes (typically the mutex of a StatefulService).
     * publishAll() holds the lock while it calls them; a task that already
     * holds such a mutex and then asks for this lock can deadlock with it.
     * So lock first, then enter the transaction:
     *
     * @code
     * HAService::Lock haLock(_haService);
     * beginTransaction();
     * // add / remove sub-devices, update identities ...
     * endTransaction();
     * @endcode
     *
     * The lock is recursive. publish(), publishConfig() and the state
     * publishers do not need it.
     *
     * Never take it in an MQTT event callback (onConnect, a command or message
     * handler): publishAll() publishes synchronously while holding it, and a
     * synchronous publish cannot complete while the MQTT event task is blocked.
     */
    class Lock
    {
    public:
        explicit Lock(HAService *service) : _service(service)
        {
            if (_service != nullptr)
                _service->lock();
        }
        ~Lock()
        {
            if (_service != nullptr)
                _service->unlock();
        }
        Lock(const Lock &) = delete;
        Lock &operator=(const Lock &) = delete;

    private:
        HAService *_service;
    };

    /** Take the registry lock (recursive). Prefer the scoped Lock. */
    void lock();
    /** Release the registry lock. */
    void unlock();

    // ========================================================================
    // Devices
    // ========================================================================

    /**
     * @brief Access the main (firmware-owning) HADevice.
     *
     * Lazy-created on first access from the current identity fields. Project
     * code registers entities via mainDevice().registerControl/Config/
     * Diagnostic. The HADevice's identity is kept in sync when setDeviceName/
     * setManufacturer/setModel are called subsequently.
     */
    HADevice &mainDevice();

    /**
     * @brief Add a sub-device (e.g., a discovered remote node in a gateway).
     *
     * The HAService takes ownership of the device. If MQTT is already
     * connected, the device's entities are published immediately. Otherwise
     * they are published on the next MQTT (re)connect.
     *
     * The following identity fields are set automatically if not already
     * filled by the caller:
     *   - viaDevice     → set to the main device's ID
     *   - topicNamespace → set to the main device's ID, so the base topic nests
     *                      under it: {prefix}{mainDeviceId}/{subDeviceId}
     *
     * @param device  HADevice to add (ownership transferred)
     * @return Raw pointer valid for the lifetime of this HAService (or until
     *         removeSubDevice() is called). Never null.
     */
    HADevice *addSubDevice(std::unique_ptr<HADevice> device);

    /**
     * @brief Remove a sub-device by its device ID.
     *
     * Calls unpublishAll() on the device (sends empty retained payloads so HA
     * removes the device from its registry) before destroying it.
     *
     * @param deviceId  HADeviceIdentity::id of the device to remove
     * @return true if found and removed, false if no device with that ID exists
     */
    bool removeSubDevice(const String &deviceId);

private:
    PsychicMqttClient *_mqttClient;

    // Device identity
    String _deviceId;
    String _deviceName;
    String _manufacturer;
    String _model;
    String _discoveryPrefix;
    bool _enabled;
    bool _renameEntityIds;

    // Pre-v1.5.1 topic identity, remembered only while it differs from the
    // current one. Empty means "nothing stale to clear". See _purgeLegacyState().
    String _legacyDeviceId;
    String _legacyBaseTopic;
    bool _legacyPurgeArmed{true};

    // Publish / unpublish callbacks - keyed by ID for O(n) removal
    using CallbackEntry = std::pair<CallbackId, PublishAllCallback>;
    std::vector<CallbackEntry> _publishCallbacks;
    std::vector<CallbackEntry> _unpublishCallbacks;
    CallbackId _nextCallbackId{0};

    // Main device (lazy-created) and sub-devices
    std::unique_ptr<HADevice> _mainDevice;
    std::vector<std::unique_ptr<HADevice>> _subDevices;

    /** Ensure _mainDevice exists and its identity is synced with current state. */
    void _ensureMainDevice();
    /** Refresh the main device's identity to mirror current HAService state. */
    void _syncMainDeviceIdentity();

    /**
     * Run the callback registered under `id`, if it still is. Holds the registry
     * lock for the duration, so a concurrent deregistration waits for it.
     */
    void _invokeCallback(const std::vector<CallbackEntry> &callbacks, CallbackId id);

    // Persistent publish task (started by begin())
    TaskHandle_t _publishTaskHandle;
    static void _publishTaskImpl(void *param);
    void _publishTask();

    // Serialises the registry (callbacks, main device, sub-devices) between the
    // publish task and the tasks that add, remove or modify HA objects. See Lock.
    SemaphoreHandle_t _registryMutex;

    /**
     * @brief Recompute the pre-v1.5.1 topic identity from the display name.
     *
     * Up to v1.5.0 both the namespace and the device ID were slugs of the
     * user-editable display name:
     *   namespace = slugify(deviceName)
     *   deviceId  = slugify(deviceName) + "-" + uniqueId
     *   baseTopic = {prefix}{namespace}/{deviceId}
     *
     * The display name still persists across the upgrade, so the old topics
     * are exactly reproducible - which is what lets the first publish after
     * the upgrade clean them up. Each of the two values is stored only if it
     * actually differs from its current counterpart.
     */
    void _updateLegacyTopicIdentity();

    /**
     * @brief Clear the pre-v1.5.1 counterpart of a state/command topic.
     *
     * Called for every publish that targets the current base topic while the
     * purge is armed; rewrites the topic onto the old base and sends an empty
     * retained payload there so the stale retained tree drains itself.
     */
    void _purgeLegacyState(const String &topic);
};
