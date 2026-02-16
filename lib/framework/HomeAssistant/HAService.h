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
#include <vector>

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

    HAService(PsychicMqttClient *mqttClient);

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * @brief Set the device display name for Home Assistant
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
     * Format: {discoveryPrefix}{topicNamespace}/{deviceId}
     * Example: "homeassistant/genius-gateway/genius-gateway-aabbcc"
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
     * @brief Publish to a specific MQTT topic
     * @param topic Full MQTT topic
     * @param payload Message payload
     * @param qos QoS level (default 0)
     * @param retain Retain flag (default true)
     * @return true if publish succeeded
     */
    bool publish(const String &topic, const String &payload, int qos = 0, bool retain = true);

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
     * Framework services and project code register here to publish their
     * entity configs and initial states when MQTT connects.
     *
     * @param callback Function to call during publishAll()
     */
    void onPublishAll(PublishAllCallback callback);

    /**
     * @brief Publish all registered entities
     *
     * Called on MQTT connect. Invokes all registered onPublishAll callbacks.
     * Only executes if HA integration is enabled and MQTT is connected.
     */
    void publishAll();

private:
    PsychicMqttClient *_mqttClient;

    // Device identity
    String _deviceId;
    String _deviceName;
    String _manufacturer;
    String _model;
    String _topicNamespace;
    String _discoveryPrefix;
    bool _enabled;

    // Publish callbacks
    std::vector<PublishAllCallback> _publishCallbacks;

    /**
     * @brief Generate the device ID from APP_NAME and MAC address
     *
     * Creates a slug from the device name and appends the unique ID:
     * "My Device Name" → "my-device-name-aabbcc"
     */
    void _generateDeviceId();

    /**
     * @brief Generate the topic namespace from device name
     *
     * Creates a URL-safe slug: "My Device Name" → "my-device-name"
     */
    static String _slugify(const String &name);
};
