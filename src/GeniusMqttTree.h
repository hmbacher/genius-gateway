/**
 * @file GeniusMqttTree.h
 * @brief MQTT value tree: one retained topic per value, plain true / false / number / text
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

#pragma once

/*
 * Independent of Home Assistant and of JSON. Plain C++ without framework dependencies, so the
 * planning logic can be tested on a host (tests/native/test_tree.cpp). The ESP side
 * (GeniusDevicesMqttTree.cpp) takes snapshots of the device state, asks Tree::plan() which
 * messages are due and publishes them (retained, QoS 0).
 *
 * Contributed by danielrdt (#27); here fed from the device status flags.
 *
 * Topic layout, relative to the configurable root (default "genius-gateway/<mac>"). A detector is
 * addressed by its stable internal id:
 *
 *   alarm, alarming_count, fault_count                          global
 *   devices/<id>/alarm                                          true | false
 *   devices/<id>/location                                       text
 *   devices/<id>/serial/detector, serial/radio, line_id         integers
 *   devices/<id>/battery_low, device_fault, radio_fault,
 *                dismounted                                     true | false, empty while not known
 *   devices/<id>/reported_missing                               true | false
 *   devices/<id>/last_seen                                      ISO 8601, UTC   (FM Pro X, RAM only)
 *   devices/<id>/rssi                                           dBm, integer    (FM Pro X, RAM only)
 *
 *   lines/<lineId>/name                                         text
 *   lines/<lineId>/alarm                                        true | false (a detector of the line is alarming)
 *   lines/<lineId>/alarming_count, fault_count, device_count    integers
 *
 * <lineId> is the alarm line ID as a decimal number, the same value as devices/<id>/line_id.
 * A value that is not known has no payload: its retained topic is cleared (empty payload).
 */

#include <GeniusDeviceStatus.h>

#include <array>
#include <map>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <vector>

namespace genius_tree
{

/// Topics of one smoke detector
enum Topic : uint8_t
{
    T_ALARM = 0,
    T_LOCATION,
    T_SERIAL_DETECTOR,
    T_SERIAL_RADIO,
    T_LINE_ID,
    T_BATTERY_LOW,
    T_DEVICE_FAULT,
    T_RADIO_FAULT,
    T_DISMOUNTED,
    T_REPORTED_MISSING,
    T_LAST_SEEN,
    T_RSSI,
    T_COUNT
};

inline const char *suffix(Topic t)
{
    static const char *const names[T_COUNT] = {
        "alarm", "location", "serial/detector", "serial/radio", "line_id",
        "battery_low", "device_fault", "radio_fault", "dismounted", "reported_missing",
        "last_seen", "rssi"};
    return names[t];
}

/// How often a value may be published (0 = every change at once).
struct Policy
{
    uint64_t minIntervalMs = 0; ///< A changed value is held back until this long after the last publish
    int32_t minDelta = 0;       ///< ... unless the number moved by at least this much
};

inline Policy policyOf(Topic t)
{
    if (t == T_LAST_SEEN)
        return {60ULL * 1000ULL, 0}; // a frame every few seconds must not cause a message every few seconds
    if (t == T_RSSI)
        return {10ULL * 60ULL * 1000ULL, 3};
    return {};
}

/// Everything the tree needs to know about one device, copied under the locks of the service.
struct DeviceSnapshot
{
    uint32_t id = 0;
    std::string location;
    uint32_t detectorSn = 0;
    uint32_t radioSn = 0;
    uint32_t lineId = 0;
    bool alarming = false;

    uint32_t knownFlags = 0;  ///< Status flags whose state is known (GENIUS_STATUS_*)
    uint32_t activeFlags = 0; ///< Status flags that are active

    std::string lastSeen; ///< ISO 8601; empty = not heard since the gateway started
    bool hasRssi = false;
    int8_t rssi = 0;

    /// At least one fault is active (the flags that count as a fault everywhere else, too)
    bool faulty() const { return (activeFlags & GENIUS_STATUS_FAULT_MASK) != 0; }
};

struct Globals
{
    bool alarming = false;
    uint32_t alarmingCount = 0;
    uint32_t faultCount = 0;
};

/// Topics of one alarm line (summary of its detectors)
enum LineTopic : uint8_t
{
    L_NAME = 0,
    L_ALARM,
    L_ALARMING_COUNT,
    L_FAULT_COUNT,
    L_DEVICE_COUNT,
    L_COUNT
};

inline const char *lineSuffix(LineTopic t)
{
    static const char *const names[L_COUNT] = {"name", "alarm", "alarming_count", "fault_count", "device_count"};
    return names[t];
}

/// A configured alarm line (id and name)
struct LineName
{
    uint32_t id = 0;
    std::string name;
};

/// The values of all topics of one alarm line, summed up from the snapshots of its detectors.
struct LineSummary
{
    uint32_t id = 0;
    std::string name;
    uint32_t devices = 0;
    uint32_t alarming = 0;
    uint32_t faults = 0;
};

/// One value; present = false means "not known": the retained topic is cleared.
struct Value
{
    bool present = false;
    std::string text;
    int32_t num = 0; ///< Numeric value for the change threshold (RSSI)
};

inline Value text(const std::string &s) { return {true, s, 0}; }
inline Value boolean(bool b) { return {true, b ? "true" : "false", 0}; }
inline Value number(int64_t n) { return {true, std::to_string(n), (int32_t)n}; }
inline Value none() { return {}; }

/// A status flag the detector reports: known only once it was reported (or read out).
inline Value flagValue(const DeviceSnapshot &d, uint32_t flag)
{
    return (d.knownFlags & flag) ? boolean((d.activeFlags & flag) != 0) : none();
}

/// The values of all topics of one device.
inline std::array<Value, T_COUNT> deviceValues(const DeviceSnapshot &d)
{
    std::array<Value, T_COUNT> v;
    v[T_ALARM] = boolean(d.alarming);
    v[T_LOCATION] = d.location.empty() ? none() : text(d.location);
    v[T_SERIAL_DETECTOR] = d.detectorSn ? number(d.detectorSn) : none();
    v[T_SERIAL_RADIO] = d.radioSn ? number(d.radioSn) : none();
    v[T_LINE_ID] = d.lineId ? number(d.lineId) : none();

    v[T_BATTERY_LOW] = flagValue(d, GENIUS_STATUS_BATT_LOW);
    v[T_DEVICE_FAULT] = flagValue(d, GENIUS_STATUS_ERROR);
    v[T_RADIO_FAULT] = flagValue(d, GENIUS_STATUS_RADIO);
    v[T_DISMOUNTED] = flagValue(d, GENIUS_STATUS_DISMOUNT);
    // The gateway's own state, so it is always known
    v[T_REPORTED_MISSING] = boolean((d.activeFlags & GENIUS_STATUS_REPORTED_MISSING) != 0);

    v[T_LAST_SEEN] = d.lastSeen.empty() ? none() : text(d.lastSeen);
    v[T_RSSI] = d.hasRssi ? number(d.rssi) : none();
    return v;
}

/// The values of all topics of one alarm line.
inline std::array<Value, L_COUNT> lineValues(const LineSummary &l)
{
    std::array<Value, L_COUNT> v;
    v[L_NAME] = l.name.empty() ? none() : text(l.name);
    v[L_ALARM] = boolean(l.alarming > 0);
    v[L_ALARMING_COUNT] = number(l.alarming);
    v[L_FAULT_COUNT] = number(l.faults);
    v[L_DEVICE_COUNT] = number(l.devices);
    return v;
}

/**
 * Sum the detectors up per alarm line. A line exists if it is configured (`names`) or if a detector
 * is assigned to it; detectors without a line (id 0) and the broadcast line (0xFFFFFFFF) are no lines.
 * Ordered by line id.
 */
inline std::vector<LineSummary> summarizeLines(const std::vector<DeviceSnapshot> &devices, const std::vector<LineName> &names)
{
    std::map<uint32_t, LineSummary> lines;
    auto valid = [](uint32_t id) { return id != 0 && id != 0xFFFFFFFFu; };
    for (const LineName &n : names)
    {
        if (!valid(n.id))
            continue;
        LineSummary &l = lines[n.id];
        l.id = n.id;
        l.name = n.name;
    }
    for (const DeviceSnapshot &d : devices)
    {
        if (!valid(d.lineId))
            continue;
        LineSummary &l = lines[d.lineId];
        l.id = d.lineId;
        l.devices++;
        if (d.alarming)
            l.alarming++;
        if (d.faulty())
            l.faults++;
    }
    std::vector<LineSummary> out;
    for (auto &kv : lines)
        out.push_back(kv.second);
    return out;
}

/// FNV-1a; two values that hash the same are treated as equal (change detection only).
inline uint32_t hashOf(const std::string &s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s)
    {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

/// What was last published for one topic (a hash, not the text, to keep this small).
struct Slot
{
    bool valid = false;
    uint32_t hash = 0;
    uint64_t ms = 0;
    int32_t num = 0;
};

enum class Act : uint8_t
{
    Skip,
    Publish,
    Defer ///< Changed, but held back by the policy: ask again later
};

inline Act decide(const Slot &slot, uint32_t newHash, int32_t newNum, uint64_t nowMs, const Policy &p, bool force)
{
    if (force || !slot.valid)
        return Act::Publish;
    if (newHash == slot.hash)
        return Act::Skip;
    if (p.minIntervalMs == 0 && p.minDelta == 0)
        return Act::Publish;
    if (p.minDelta > 0 && abs(newNum - slot.num) >= p.minDelta)
        return Act::Publish;
    const uint64_t age = nowMs >= slot.ms ? nowMs - slot.ms : 0;
    if (p.minIntervalMs > 0 && age >= p.minIntervalMs)
        return Act::Publish;
    return Act::Defer;
}

/// Root topic: the configured one, or "genius-gateway/<mac>". A trailing slash is dropped.
inline std::string rootTopic(const std::string &configured, const std::string &mac)
{
    std::string r = configured.empty() ? "genius-gateway/" + mac : configured;
    while (!r.empty() && r.back() == '/')
        r.pop_back();
    return r;
}

/// A message to publish: `topic` is relative to the root, an empty payload clears the retained topic.
struct Message
{
    std::string topic;
    std::string payload;
};

class Tree
{
public:
    static constexpr size_t GLOBAL_COUNT = 3;

    /**
     * Which messages are due now. `force` (MQTT connect, tree switched on, root changed) republishes
     * every topic, also the unknown ones as clears, so no stale retained value survives a restart.
     * `deferred` is set when a changed value is only held back by its policy (call again later).
     */
    std::vector<Message> plan(const std::vector<DeviceSnapshot> &devices, const std::vector<LineName> &lineNames,
                              const Globals &g, uint64_t nowMs, bool force, bool *deferred = nullptr)
    {
        std::vector<Message> out;
        bool defer = false;

        std::vector<uint32_t> live;
        for (const DeviceSnapshot &d : devices)
        {
            live.push_back(d.id);
            auto &slots = _devices[d.id];
            const auto values = deviceValues(d);
            for (size_t i = 0; i < T_COUNT; i++)
                step(slots[i], values[i], "devices/" + std::to_string(d.id) + "/" + suffix((Topic)i), policyOf((Topic)i),
                     nowMs, force, out, defer);
        }

        // devices that were deleted: wipe what was published for them
        for (auto it = _devices.begin(); it != _devices.end();)
        {
            bool alive = false;
            for (uint32_t id : live)
                if (id == it->first)
                    alive = true;
            if (alive)
            {
                ++it;
                continue;
            }
            for (size_t i = 0; i < T_COUNT; i++)
                clearSlot(it->second[i], "devices/" + std::to_string(it->first) + "/" + suffix((Topic)i), out);
            it = _devices.erase(it);
        }

        // alarm lines: summary of their detectors
        const std::vector<LineSummary> lines = summarizeLines(devices, lineNames);
        for (const LineSummary &l : lines)
        {
            auto &slots = _lines[l.id];
            const auto values = lineValues(l);
            for (size_t i = 0; i < L_COUNT; i++)
                step(slots[i], values[i], "lines/" + std::to_string(l.id) + "/" + lineSuffix((LineTopic)i), Policy{}, nowMs,
                     force, out, defer);
        }
        for (auto it = _lines.begin(); it != _lines.end();)
        {
            bool alive = false;
            for (const LineSummary &l : lines)
                if (l.id == it->first)
                    alive = true;
            if (alive)
            {
                ++it;
                continue;
            }
            for (size_t i = 0; i < L_COUNT; i++)
                clearSlot(it->second[i], "lines/" + std::to_string(it->first) + "/" + lineSuffix((LineTopic)i), out);
            it = _lines.erase(it);
        }

        const Value gv[GLOBAL_COUNT] = {boolean(g.alarming), number(g.alarmingCount), number(g.faultCount)};
        for (size_t i = 0; i < GLOBAL_COUNT; i++)
            step(_global[i], gv[i], globalName(i), Policy{}, nowMs, force, out, defer);

        if (deferred)
            *deferred = defer;
        return out;
    }

    /// Messages that clear everything published so far (tree switched off or root changed), then forget it all.
    std::vector<Message> clearAll()
    {
        std::vector<Message> out;
        for (auto &kv : _devices)
            for (size_t i = 0; i < T_COUNT; i++)
                clearSlot(kv.second[i], "devices/" + std::to_string(kv.first) + "/" + suffix((Topic)i), out);
        for (auto &kv : _lines)
            for (size_t i = 0; i < L_COUNT; i++)
                clearSlot(kv.second[i], "lines/" + std::to_string(kv.first) + "/" + lineSuffix((LineTopic)i), out);
        for (size_t i = 0; i < GLOBAL_COUNT; i++)
            clearSlot(_global[i], globalName(i), out);
        reset();
        return out;
    }

    void reset()
    {
        _devices.clear();
        _lines.clear();
        _global = {};
    }

    bool empty() const { return _devices.empty() && _lines.empty(); }

private:
    std::map<uint32_t, std::array<Slot, T_COUNT>> _devices;
    std::map<uint32_t, std::array<Slot, L_COUNT>> _lines;
    std::array<Slot, GLOBAL_COUNT> _global{};

    static const char *globalName(size_t i)
    {
        static const char *const names[GLOBAL_COUNT] = {"alarm", "alarming_count", "fault_count"};
        return names[i];
    }

    static void step(Slot &slot, const Value &v, const std::string &topic, const Policy &p, uint64_t nowMs, bool force,
                     std::vector<Message> &out, bool &defer)
    {
        const std::string payload = v.present ? v.text : std::string();
        // A value that is not known and was never published needs no clear (unless forced after a restart).
        if (!v.present && !slot.valid && !force)
        {
            slot = {true, hashOf(""), nowMs, 0};
            return;
        }
        // The throttle spaces out the values that follow each other. A value that appears (the topic
        // was empty until now) or disappears is not held back.
        const bool edge = !v.present || slot.hash == hashOf("");
        switch (decide(slot, hashOf(payload), v.num, nowMs, edge ? Policy{} : p, force))
        {
        case Act::Publish:
            out.push_back({topic, payload});
            slot = {true, hashOf(payload), nowMs, v.num};
            break;
        case Act::Defer:
            defer = true;
            break;
        default:
            break;
        }
    }

    static void clearSlot(Slot &slot, const std::string &topic, std::vector<Message> &out)
    {
        if (slot.valid && slot.hash != hashOf(""))
            out.push_back({topic, std::string()});
        slot = {};
    }
};

} // namespace genius_tree
