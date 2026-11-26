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
 * sent in batches, one batch per tick, so that the MQTT client stays free for alarms. The state
 * is compared also while such a queue drains: a value that changes meanwhile - an alarm - goes to
 * the front of the queue and replaces what was queued for its topic. Within everything that is
 * planned at once the alarm topics come first, and a full publish goes ahead of the clears of an
 * old root.
 *
 * The tree keeps in RAM what it published, so it can only clear what it published since the
 * gateway started. A detector deleted right after a restart, or a restart while clears were still
 * queued, would leave retained topics on the broker for good. Therefore every full publish comes
 * with a sweep: the tree subscribes to its own detector and line topics for a few seconds, notes
 * which detectors and lines the broker retains topics for, and clears those it does not know.
 * Only topics of the tree's own layout are touched; anything else under the root is left alone.
 */

#include <GeniusDevicesService.h>
#include <WiFi.h>

#define TREE_TICK_MS 1000     ///< The tree looks for work this often
#define TREE_RECHECK_MS 10000 ///< ... and compares everything at least this often, also without a hint
#define TREE_BATCH 25         ///< Messages sent per tick
#define TREE_SWEEP_MS 5000    ///< A sweep listens at least this long for what the broker retains
#define TREE_SWEEP_MAX_IDS 250 ///< ... and notes at most this many detectors / lines (bounds the memory)

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

void GeniusDevicesService::_treeOnMessage(const char *topic, const char *payload, int retain)
{
    // MQTT task. Every message the gateway receives passes here; all but the retained ones of a
    // running sweep leave at once (what the tree publishes itself comes back without the retain flag).
    if (!_treeSweepActive.load() || !retain || topic == nullptr || payload == nullptr || payload[0] == '\0')
        return;

    xSemaphoreTake(_treeSweepMutex, portMAX_DELAY);
    const size_t n = _treeSweepPrefix.size();
    if (n != 0 && strncmp(topic, _treeSweepPrefix.c_str(), n) == 0)
    {
        bool isLine = false;
        uint32_t id = 0;
        if (genius_tree::ownTopic(std::string(topic + n), isLine, id))
        {
            std::set<uint32_t> &ids = isLine ? _treeSweepLines : _treeSweepDevices;
            if (ids.size() < TREE_SWEEP_MAX_IDS)
                ids.insert(id);
        }
    }
    xSemaphoreGive(_treeSweepMutex);
}

void GeniusDevicesService::_treeSweepBegin()
{
    _treeSweepEnd(false); // a sweep that is still running starts over

    // A root with wildcards would subscribe to more than the tree's own topics
    if (_treeRoot.empty() || _treeRoot.find_first_of("#+") != std::string::npos)
        return;

    xSemaphoreTake(_treeSweepMutex, portMAX_DELAY);
    _treeSweepPrefix = _treeRoot + "/";
    _treeSweepDevices.clear();
    _treeSweepLines.clear();
    xSemaphoreGive(_treeSweepMutex);

    _treeSweepStart = millis();
    _treeSweepActive = true; // before subscribing: the retained messages follow at once
    const std::string devices = _treeRoot + "/devices/#";
    const std::string lines = _treeRoot + "/lines/#";
    if (_mqttClient->subscribe(devices.c_str(), 0) == -1 || _mqttClient->subscribe(lines.c_str(), 0) == -1)
    {
        ESP_LOGW(GeniusDevices::TAG, "MQTT value tree: could not subscribe to look for leftover topics.");
        _treeSweepEnd(false);
    }
}

void GeniusDevicesService::_treeSweepEnd(bool evaluate)
{
    if (!_treeSweepActive.exchange(false))
        return;

    std::string prefix;
    std::set<uint32_t> devices, lines;
    xSemaphoreTake(_treeSweepMutex, portMAX_DELAY);
    prefix.swap(_treeSweepPrefix);
    devices.swap(_treeSweepDevices);
    lines.swap(_treeSweepLines);
    xSemaphoreGive(_treeSweepMutex);

    if (_mqttClient->connected())
    {
        _mqttClient->unsubscribe((prefix + "devices/#").c_str());
        _mqttClient->unsubscribe((prefix + "lines/#").c_str());
    }
    if (!evaluate)
        return;

    // Detectors and lines the broker has topics for, but the tree does not know: leftovers
    size_t count = 0;
    for (const genius_tree::Message &m : _tree.sweep(devices, lines))
    {
        _treeQueue.push_back({prefix + m.topic, std::string()});
        count++;
    }
    if (count != 0)
        ESP_LOGI(GeniusDevices::TAG, "MQTT value tree: clearing %u leftover topics of smoke detectors or alarm lines that no longer exist.",
                 (unsigned)count);
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
        _treeSweepEnd(false);
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
        _treeSweepEnd(false);
        _treeQueue.clear();
        if (!_treeRoot.empty())
            for (const genius_tree::Message &m : _tree.clearAll())
                _treeQueue.push_back({_treeRoot + "/" + m.topic, std::string()});
        _tree.reset();
        _treeRoot = root;
        _treeForce = true;
    }

    // Compare the state when a change was reported, when a full publish is due, and every few
    // seconds anyway - also while a queue is still going out (the tree is off with an empty root).
    const uint32_t now = millis();
    if (!_treeRoot.empty() && (_treeDirty || _treeForce || now - _treeLastPlan >= TREE_RECHECK_MS))
        _treePlan(now);

    // A batch per tick
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

    // A sweep ends once the full publish is out (the tree knows every detector and line by then)
    // and the broker had time to deliver what it retains. Its clears go out with the next ticks.
    if (_treeSweepActive.load() && !_treeForce && now - _treeSweepStart >= TREE_SWEEP_MS)
        _treeSweepEnd(true);
}

void GeniusDevicesService::_treePlan(uint32_t now)
{
    const bool force = _treeForce;
    _treeForce = false;
    if (force)
        _treeSweepBegin(); // every full publish looks for leftovers of an earlier run
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
    std::vector<genius_tree::Message> due = _tree.plan(devices, lineNames, globals, (uint64_t)millis(), force, &deferred);
    genius_tree::alarmsFirst(due); // an alarm that is on must not wait behind hundreds of other topics
    for (genius_tree::Message &m : due)
        m.topic = _treeRoot + "/" + m.topic;
    // To the front: a change overtakes a full publish that is still going out, and a full publish
    // goes ahead of what is still clearing an old root.
    genius_tree::enqueueFirst(_treeQueue, std::move(due));
    if (deferred)
        _treeDirty = true; // a changed value is only held back by its throttle: look again
}
