/**
 * @file GeniusDevicesMqttTree.cpp
 * @brief Generic MQTT value tree (one topic per value, plain true/false/number/text).
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 *
 * Independent of Home Assistant. The planning (which topic changed, throttling, clearing of unknown
 * values) is in GeniusMqttTree.h and unit tested on a host; this file only takes snapshots of the device
 * state and publishes the messages. Everything runs on the "gd-runtime" task (treeTick), so no
 * MQTT publishing happens in the radio RX task, and the state locks are held only while copying.
 *
 * The tree publishes values, it never writes to flash.
 */

#include <GeniusDevicesService.h>
#include <WiFi.h>
#include <esp_timer.h>

using namespace genius_rt;

static std::string macWithoutDelimiters()
{
    String mac = WiFi.macAddress(); // "AA:BB:CC:DD:EE:FF"
    mac.replace(":", "");
    mac.toLowerCase();
    return std::string(mac.c_str());
}

void GeniusDevicesService::treeRequestFullPublish()
{
    _treeForce = true;
    _treeDirty = true;
}

bool GeniusDevicesService::_treePublish(const std::string &root, const std::vector<genius_tree::Message> &messages)
{
    unsigned sent = 0;
    for (const auto &m : messages)
    {
        const std::string topic = root + "/" + m.topic;
        // retained, QoS 0; an empty payload clears the retained topic on the broker
        if (_mqttClient->publish(topic.c_str(), 0, true, m.payload.c_str(), m.payload.length(), false) == -1)
        {
            ESP_LOGW(TAG, "MQTT value tree: publish of '%s' failed, everything is sent again later.", topic.c_str());
            return false;
        }
        if (++sent % 10 == 0)
            vTaskDelay(1); // let the MQTT task and the rest of the system breathe during a full publish
    }
    return true;
}

void GeniusDevicesService::treeTick()
{
    const AlarmPublishingSettings settings =
        _alarmPublishingSettings ? _alarmPublishingSettings->getSettingsCopy() : AlarmPublishingSettings();
    const bool connected = _mqttClient != nullptr && _mqttClient->connected();

    if (_treeMac.empty())
        _treeMac = macWithoutDelimiters();

    // Switched on / off or root topic changed: wipe what was published under the old root.
    const std::string root = settings.treeEnabled
                                 ? genius_tree::rootTopic(std::string(settings.treeBaseTopic.c_str()), _treeMac)
                                 : std::string();
    if (root != _treeRoot)
    {
        if (!_treeRoot.empty() && connected)
            _treePublish(_treeRoot, _tree.clearAll());
        _tree.reset();
        _treeRoot = root;
        _treeForce = true;
    }
    if (_treeRoot.empty())
        return; // value tree is off

    if (!connected)
    {
        _treeForce = true; // after the (re)connect everything is published again, unknown values as clears
        return;
    }
    if (!_treeDirty && !_treeForce)
        return;

    const bool force = _treeForce;
    _treeForce = false;
    _treeDirty = false; // changes that arrive while we work set it again

    // names of the configured alarm lines (own lock of the alarm line service, taken before ours)
    const std::vector<genius_tree::LineName> lineNames = _treeLineNames ? _treeLineNames() : std::vector<genius_tree::LineName>();

    // --- snapshot: copy under the locks, publish without them -------------------------------------
    std::vector<genius_tree::DeviceSnapshot> devices;
    genius_tree::Globals globals;

    beginTransaction();
    xSemaphoreTake(_rtMutex, portMAX_DELAY);

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

        s.hasReadout = d.readoutTime > 0;
        s.readoutWall = (int64_t)d.readoutTime;
        s.batteryLow = d.smokeDetector.batteryLowFault;
        s.deviceFault = d.smokeDetector.deviceFault;
        s.radioFault = d.radioModule.radioNetworkFault;

        s.hasProbe = d.radioModule.lastRangeTest > 0;
        s.probeRssi = d.radioModule.rssi;

        if (const DeviceRuntime *r = _rt.find(d.id))
        {
            s.hasSeen = r->hasSeen;
            s.seenWall = r->seenWall;
            s.hasLiveRssi = r->hasRssi;
            s.liveRssi = r->rssi;
            s.faultState = r->fault.state;
            s.faultStartUnknown = r->fault.startUnknown;
            s.faultSinceWall = r->fault.sinceWall;
            s.causeSn = r->fault.causeSn;
        }
        if (s.faultState != FaultState::None)
            globals.faultCount++;
        devices.push_back(std::move(s));
    }
    globals.alarming = _isAlarming;
    globals.alarmingCount = _numAlarming;

    xSemaphoreGive(_rtMutex);
    endTransaction();

    // --- what is due, and send it -----------------------------------------------------------------
    bool deferred = false;
    const uint64_t nowMs = (uint64_t)(esp_timer_get_time() / 1000);
    const std::vector<genius_tree::Message> messages = _tree.plan(devices, lineNames, globals, nowMs, force, &deferred);

    if (!_treePublish(_treeRoot, messages))
        _treeForce = true; // retry everything, the bookkeeping already assumed these messages went out
    if (deferred)
        _treeDirty = true; // a changed value is only held back by its throttle: look again next second
}
