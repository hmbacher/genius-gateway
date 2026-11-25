/**
 * @file GeniusDevicesMqttTree.cpp
 * @brief MQTT value tree: snapshots of the device state, published as planned by GeniusMqttTree.h
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

/*
 * Runs on the loop task of the framework, about once a second. The planning (which topic changed,
 * throttling, clearing of values that are not known) is in GeniusMqttTree.h; this file takes the
 * snapshots under the locks of the service and publishes without them. Nothing here writes to flash.
 *
 * A full publish (after a connect, or when the tree is switched on) is many messages. They are
 * sent in batches, one batch per tick, so that the MQTT client stays free for alarms.
 */

#include <GeniusDevicesService.h>
#include <WiFi.h>

#define TREE_TICK_MS 1000     ///< The tree looks for work this often
#define TREE_RECHECK_MS 10000 ///< ... and compares everything at least this often, also without a hint
#define TREE_BATCH 25         ///< Messages sent per tick

static std::string macWithoutDelimiters()
{
    String mac = WiFi.macAddress(); // "AA:BB:CC:DD:EE:FF"
    mac.replace(":", "");
    mac.toLowerCase();
    return std::string(mac.c_str());
}

void GeniusDevicesService::_treeLoop()
{
    const uint32_t now = millis();
    if (now - _treeLastTick < TREE_TICK_MS)
        return;
    _treeLastTick = now;
    _treeTick();
}

bool GeniusDevicesService::_treeSend()
{
    for (unsigned sent = 0; sent < TREE_BATCH && !_treeQueue.empty(); sent++)
    {
        const genius_tree::Message &m = _treeQueue.front();
        // retained, QoS 0; an empty payload clears the retained topic on the broker
        if (_mqttClient->publish(m.topic.c_str(), 0, true, m.payload.c_str()) == -1)
        {
            ESP_LOGW(GeniusDevices::TAG, "MQTT value tree: publishing '%s' failed, everything is sent again.", m.topic.c_str());
            return false;
        }
        _treeQueue.pop_front();
    }
    return true;
}

void GeniusDevicesService::_treeTick()
{
    if (_mqttClient == nullptr || _alarmPublishingSettings == nullptr)
        return;

    if (!_mqttClient->connected())
    {
        // After the (re)connect everything is published again, values that are not known as clears.
        // A changed root is handled then as well, so the old topics can still be cleared.
        _treeQueue.clear();
        _treeForce = true;
        return;
    }

    const AlarmPublishingSettings settings = _alarmPublishingSettings->getSettingsCopy();
    if (_treeMac.empty())
        _treeMac = macWithoutDelimiters();

    // Switched on / off or root topic changed: clear what was published under the old root.
    const std::string root = settings.treeEnabled
                                 ? genius_tree::rootTopic(std::string(settings.treeBaseTopic.c_str()), _treeMac)
                                 : std::string();
    if (root != _treeRoot)
    {
        _treeQueue.clear();
        if (!_treeRoot.empty())
            for (const genius_tree::Message &m : _tree.clearAll())
                _treeQueue.push_back({_treeRoot + "/" + m.topic, std::string()});
        _tree.reset();
        _treeRoot = root;
        _treeForce = true;
    }

    // What is planned goes out first, a batch per tick; only then is the state compared again.
    if (!_treeQueue.empty())
    {
        if (!_treeSend())
        {
            // The bookkeeping already assumes these messages went out: start over
            _treeQueue.clear();
            _treeForce = true;
        }
        return;
    }

    if (_treeRoot.empty())
        return; // the value tree is off

    const uint32_t now = millis();
    if (!_treeDirty && !_treeForce && now - _treeLastPlan < TREE_RECHECK_MS)
        return;

    const bool force = _treeForce;
    _treeForce = false;
    _treeDirty = false; // changes that arrive while this runs set it again
    _treeLastPlan = now;

    // Names of the configured alarm lines (lock of the alarm line service, taken before ours)
    const std::vector<genius_tree::LineName> lineNames = _treeLineNames ? _treeLineNames() : std::vector<genius_tree::LineName>();

    // --- snapshot: copy under the lock, publish without it ---------------------------------------
    std::vector<genius_tree::DeviceSnapshot> devices;
    genius_tree::Globals globals;

    beginTransaction();
    devices.reserve(_state.devices.size());
    for (const GeniusDevice &d : _state.devices)
    {
        genius_tree::DeviceSnapshot s;
        s.id = d.id;
        s.location = d.location.c_str();
        s.detectorSn = d.smokeDetector.sn;
        s.radioSn = d.radioModule.sn;
        s.lineId = d.radioModule.lineId;
        s.alarming = d.isAlarming;
        s.knownFlags = d.status.knownMask();
        s.activeFlags = d.status.activeMask();

        // RAM-only values of the supervised radio modules; nothing is known until a module was heard
        ModuleRuntime runtime;
        if (_runtimeOf(d.id, runtime) && runtime.lastSeen != 0)
        {
            s.lastSeen = Utils::time_t_to_iso8601(runtime.lastSeen).c_str();
            if (runtime.rssiTime != 0)
            {
                s.hasRssi = true;
                s.rssi = runtime.rssi;
            }
        }

        if (s.alarming)
            globals.alarmingCount++;
        if (s.faulty())
            globals.faultCount++;
        devices.push_back(std::move(s));
    }
    endTransaction();
    globals.alarming = globals.alarmingCount > 0;

    // --- what is due --------------------------------------------------------------------------
    bool deferred = false;
    for (genius_tree::Message &m : _tree.plan(devices, lineNames, globals, (uint64_t)millis(), force, &deferred))
        _treeQueue.push_back({_treeRoot + "/" + m.topic, std::move(m.payload)});
    if (deferred)
        _treeDirty = true; // a changed value is only held back by its throttle: look again

    if (!_treeQueue.empty() && !_treeSend())
    {
        _treeQueue.clear();
        _treeForce = true;
    }
}
