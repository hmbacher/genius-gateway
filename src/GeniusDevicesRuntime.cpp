/**
 * @file GeniusDevicesRuntime.cpp
 * @brief RAM-only runtime state of the devices: last seen, live RSSI, radio link faults.
 *
 * @copyright Copyright (c) 2024-2026 Genius Gateway Project
 * @license AGPL-3.0 with Commons Clause
 *
 * This file is part of Genius Gateway.
 * See https://github.com/hmbacher/genius-gateway/blob/main/LICENSE for details.
 *
 * NOTHING IN THIS FILE MAY WRITE TO FLASH. The state lives in GeniusDevicesService::_rt (a plain
 * RAM table) and never touches GeniusDevice / _state, whose JSON is persisted by FSPersistence on
 * every update handler call. In particular this file must not call callUpdateHandlers(), FSPersistence,
 * NVS or any file API.
 */

#include <GeniusDevicesService.h>
#include <esp_timer.h>
#include <algorithm>
#include <stdlib.h>

using namespace genius_rt;

static constexpr uint64_t RT_MQTT_SEEN_INTERVAL_MS = 60ULL * 1000ULL;       ///< last_seen is published at most once a minute per device
static constexpr uint64_t RT_MQTT_RSSI_INTERVAL_MS = 10ULL * 60ULL * 1000ULL; ///< RSSI at least every 10 min while it changes
static constexpr int RT_MQTT_RSSI_DELTA_DB = 3;                             ///< ... or as soon as it moved by this much
static constexpr uint32_t RT_PURGE_EVERY_TICKS = 60;                        ///< drop state of deleted devices (ticks are ~1 s)

// ============================================================================
// Helpers
// ============================================================================

int64_t GeniusDevicesService::_wallNow()
{
    const time_t now = time(nullptr);
    return now > 1577836800 ? (int64_t)now : 0; // 2020-01-01: anything earlier means "not synchronized yet"
}

uint32_t GeniusDevicesService::_findDeviceIdByRadioSn(uint32_t radioModuleSN)
{
    if (radioModuleSN == 0)
        return 0;
    for (const GeniusDevice &d : _state.devices)
        if (d.radioModule.sn == radioModuleSN)
            return d.id;
    return 0;
}

uint32_t GeniusDevicesService::_findDeviceIdByDetectorSn(uint32_t detectorSN)
{
    if (detectorSN == 0)
        return 0;
    for (const GeniusDevice &d : _state.devices)
        if (d.smokeDetector.sn == detectorSN)
            return d.id;
    return 0;
}

void GeniusDevicesService::_rtMarkLocked(uint32_t deviceId, uint8_t change)
{
    if (change == CHANGE_NONE || deviceId == 0)
        return;
    _treeDirty = true; // the generic value tree looks at the device again at the next tick
    for (auto &p : _rtPending)
    {
        if (p.id == deviceId)
        {
            p.event |= change;
            p.mqtt |= change;
            return;
        }
    }
    _rtPending.push_back({deviceId, change, change, 0, 0});
}

// ============================================================================
// Frame intake (called from the RX task: RAM only, short, no publishing)
// ============================================================================

void GeniusDevicesService::runtimeOnFrame(const uint8_t *data, size_t len, int8_t rssiDbm)
{
    if (!data || len < LEN_SUPERVISION_BEACON || data[0] != 0x02) // all Genius frames start with 0x02 and have >= 28 bytes
        return;

    const uint32_t origin = be32(data, 9);  // radio module that generated the frame
    const uint32_t sender = be32(data, 14); // radio module that transmitted it (differs when relayed)
    if (origin == GATEWAY_SN || sender == GATEWAY_SN)
        return;

    const SupKind kind = classifySupervision(data, len);
    const uint32_t detectorSn = (kind == SupKind::RadioFault || kind == SupKind::FaultAck) ? le32(data, POS_DETECTOR_SN) : 0;
    const uint32_t missingSn = (kind == SupKind::Missing) ? be32(data, POS_MISSING_SN) : 0;

    uint32_t idOrigin = 0, idSender = 0, idByDetector = 0;
    beginTransaction();
    idOrigin = _findDeviceIdByRadioSn(origin);
    idSender = (sender == origin) ? idOrigin : _findDeviceIdByRadioSn(sender);
    if (!idOrigin && detectorSn)
        idByDetector = _findDeviceIdByDetectorSn(detectorSn);
    endTransaction();

    const uint32_t idTarget = idOrigin ? idOrigin : idByDetector; // device the fault belongs to
    if (!idOrigin && !idSender && !idTarget)
        return; // frame of a module we do not know (e.g. a neighbour's installation)

    const uint64_t nowMs = (uint64_t)(esp_timer_get_time() / 1000);
    const int64_t wall = _wallNow();

    xSemaphoreTake(_rtMutex, portMAX_DELAY);

    if (idOrigin)
        _rtMarkLocked(idOrigin, _rt.seen(idOrigin, nowMs, wall));
    if (idSender)
    {
        if (idSender != idOrigin)
            _rtMarkLocked(idSender, _rt.seen(idSender, nowMs, wall));
        // The RSSI belongs to the module whose radio we actually heard: the sender, not the originator of a relayed frame.
        _rtMarkLocked(idSender, _rt.rssi(idSender, rssiDbm, nowMs, wall));
    }

    switch (kind)
    {
    case SupKind::Missing:
        if (idTarget)
            _rtMarkLocked(idTarget, _rt.linkMissing(idTarget, missingSn, nowMs, wall));
        break;
    case SupKind::RadioFault:
        if (idTarget)
            _rtMarkLocked(idTarget, _rt.fault(idTarget, nowMs, wall));
        break;
    case SupKind::FaultAck:
        if (idTarget)
            _rtMarkLocked(idTarget, _rt.ack(idTarget, nowMs, wall));
        break;
    default: // Beacon: "seen" above is all we record
        break;
    }

    xSemaphoreGive(_rtMutex);
}

// ============================================================================
// Housekeeping, MQTT publishing (throttled) and change list for the WebSocket event
// ============================================================================

void GeniusDevicesService::runtimeTick(std::vector<uint32_t> &changedIds)
{
    const uint64_t nowMs = (uint64_t)(esp_timer_get_time() / 1000);
    std::vector<uint32_t> toPublish;

    // Forget runtime state of deleted devices (rarely, and before taking _rtMutex: lock order state -> rt).
    std::vector<uint32_t> existing;
    const bool purge = (++_rtTicks % RT_PURGE_EVERY_TICKS) == 0;
    if (purge)
    {
        beginTransaction();
        existing.reserve(_state.devices.size());
        for (const GeniusDevice &d : _state.devices)
            existing.push_back(d.id);
        endTransaction();
    }

    xSemaphoreTake(_rtMutex, portMAX_DELAY);

    if (purge)
    {
        std::vector<uint32_t> stale;
        for (const auto &r : _rt.all())
            if (std::find(existing.begin(), existing.end(), r.deviceId) == existing.end())
                stale.push_back(r.deviceId);
        for (uint32_t id : stale)
            _rt.remove(id);
        _rtPending.erase(std::remove_if(_rtPending.begin(), _rtPending.end(),
                                        [&](const RtPending &p)
                                        { return std::find(existing.begin(), existing.end(), p.id) == existing.end(); }),
                         _rtPending.end());
    }

    for (uint32_t id : _rt.expire(nowMs))
        _rtMarkLocked(id, CHANGE_FAULT);

    for (auto &p : _rtPending)
    {
        if (p.event)
        {
            changedIds.push_back(p.id);
            p.event = 0;
        }
        if (!p.mqtt)
            continue;

        const DeviceRuntime *r = _rt.find(p.id);
        const int8_t rssi = (r && r->hasRssi) ? r->rssi : 0;
        const bool never = p.mqttMs == 0;
        const uint64_t age = nowMs - p.mqttMs;
        bool due = never || (p.mqtt & CHANGE_FAULT);
        if (!due && (p.mqtt & CHANGE_SEEN))
            due = age >= RT_MQTT_SEEN_INTERVAL_MS;
        if (!due && (p.mqtt & CHANGE_RSSI))
            due = abs((int)rssi - (int)p.mqttRssi) >= RT_MQTT_RSSI_DELTA_DB || age >= RT_MQTT_RSSI_INTERVAL_MS;
        if (due)
        {
            p.mqtt = 0;
            p.mqttMs = nowMs;
            p.mqttRssi = rssi;
            toPublish.push_back(p.id);
        }
    }

    xSemaphoreGive(_rtMutex);

    // Publish outside of _rtMutex: the state reader of the HA publisher takes the locks itself.
    for (uint32_t id : toPublish)
        _publishLinkState(id);
}

// ============================================================================
// Serialization (REST, WebSocket event and MQTT state share one format)
// ============================================================================

static const char *faultStateName(FaultState s)
{
    switch (s)
    {
    case FaultState::Warning: return "warning";
    case FaultState::Active:  return "active";
    case FaultState::Acked:   return "acked";
    default:                  return "none";
    }
}

void GeniusDevicesService::_runtimeFillDevice(const GeniusDevice &device, JsonObject o, uint64_t nowMs, int64_t wall)
{
    (void)wall;
    o["id"] = device.id;
    const DeviceRuntime *r = _rt.find(device.id);

    if (r && r->hasSeen)
    {
        if (r->seenWall > 0)
            o["lastSeen"] = Utils::time_t_to_iso8601((time_t)r->seenWall);
        o["lastSeenAgeS"] = (uint32_t)((nowMs - r->seenMs) / 1000);
    }

    // Effective RSSI: the live value (RAM) wins; without one the last measured value of the range test (flash).
    if (r && r->hasRssi)
    {
        o["rssi"] = r->rssi;
        o["rssiSource"] = "live";
        if (r->rssiWall > 0)
            o["rssiTime"] = Utils::time_t_to_iso8601((time_t)r->rssiWall);
    }
    else if (device.radioModule.lastRangeTest > 0)
    {
        o["rssi"] = device.radioModule.rssi; // 0 = range-tested without an answer
        o["rssiSource"] = "probe";
        o["rssiTime"] = Utils::time_t_to_iso8601(device.radioModule.lastRangeTest);
    }

    if (r && r->fault.state != FaultState::None)
    {
        const Fault &f = r->fault;
        JsonObject fo = o["fault"].to<JsonObject>();
        fo["state"] = faultStateName(f.state);
        fo["acknowledged"] = (f.state == FaultState::Acked);
        if (f.startUnknown)
            fo["startUnknown"] = true;
        if (f.sinceWall > 0 && !f.startUnknown)
            fo["since"] = Utils::time_t_to_iso8601((time_t)f.sinceWall);
        if (f.activeWall > 0)
            fo["activeSince"] = Utils::time_t_to_iso8601((time_t)f.activeWall);
        if (f.ackWall > 0)
            fo["acknowledgedAt"] = Utils::time_t_to_iso8601((time_t)f.ackWall);
        if (f.lastWall > 0)
            fo["lastReceived"] = Utils::time_t_to_iso8601((time_t)f.lastWall);
        const uint64_t faultAge = nowMs >= f.lastMs ? nowMs - f.lastMs : 0;
        fo["expiresInS"] = (uint32_t)((faultAge >= FAULT_TTL_MS ? 0 : FAULT_TTL_MS - faultAge) / 1000);
        if (f.causeSn)
        {
            fo["causeSn"] = f.causeSn; // radio module reported as missing
            for (const GeniusDevice &d : _state.devices)
                if (d.radioModule.sn == f.causeSn)
                {
                    fo["causeDeviceId"] = d.id;
                    fo["causeLocation"] = d.location;
                    break;
                }
        }
    }
}

void GeniusDevicesService::runtimeToJson(JsonArray arr, const std::vector<uint32_t> *onlyIds)
{
    const uint64_t nowMs = (uint64_t)(esp_timer_get_time() / 1000);
    const int64_t wall = _wallNow();

    beginTransaction();
    xSemaphoreTake(_rtMutex, portMAX_DELAY);
    for (const GeniusDevice &d : _state.devices)
    {
        if (onlyIds)
        {
            if (std::find(onlyIds->begin(), onlyIds->end(), d.id) == onlyIds->end())
                continue;
        }
        else if (!_rt.find(d.id) && d.radioModule.lastRangeTest == 0)
            continue; // nothing to tell about this device

        _runtimeFillDevice(d, arr.add<JsonObject>(), nowMs, wall);
    }
    xSemaphoreGive(_rtMutex);
    endTransaction();
}

// ============================================================================
// Home Assistant entities (separate state topic "link"; independent of the acoustic readout)
// ============================================================================

std::unique_ptr<HAGroupedSensorPublisher> GeniusDevicesService::_makeLinkPublisher(uint32_t devId, HADevice *owner)
{
    auto link = std::make_unique<HAGroupedSensorPublisher>(
        _haService, "link",
        [this, devId](JsonObject &state) {
            const uint64_t nowMs = (uint64_t)(esp_timer_get_time() / 1000);
            const int64_t wall = _wallNow();
            beginTransaction();
            xSemaphoreTake(_rtMutex, portMAX_DELAY);
            for (const GeniusDevice &d : _state.devices)
                if (d.id == devId)
                {
                    _runtimeFillDevice(d, state, nowMs, wall);
                    break;
                }
            xSemaphoreGive(_rtMutex);
            endTransaction();
        },
        owner);

    // These entities do not depend on a radio module readout. Each one is available as soon as its
    // own value exists, so HA shows "unavailable" (not a stale or invented value) after a gateway reboot.
    link->addSensor("sensor", "last_seen", "Last Seen", "mdi:eye-check-outline", HACategory::Diagnostic,
        [](JsonObject &c) {
            c["value_template"]        = "{{ value_json.lastSeen }}";
            c["device_class"]          = "timestamp";
            c["availability_topic"]    = "~/link/state";
            c["availability_template"] = "{{ 'online' if value_json.lastSeen is defined else 'offline' }}";
        })
    .addSensor("sensor", "signal_strength", "Signal Strength", "", HACategory::Diagnostic,
        [](JsonObject &c) {
            c["value_template"]        = "{{ value_json.rssi }}";
            c["device_class"]          = "signal_strength";
            c["unit_of_measurement"]   = "dBm";
            c["state_class"]           = "measurement";
            c["availability_topic"]    = "~/link/state";
            c["availability_template"] = "{{ 'online' if value_json.rssi is defined and value_json.rssi < 0 else 'offline' }}";
            c["json_attributes_topic"]    = "~/link/state";
            c["json_attributes_template"] = "{{ {'source': value_json.rssiSource | default(none), 'time': value_json.rssiTime | default(none)} | tojson }}";
        })
    .addSensor("binary_sensor", "radio_link_fault", "Radio Link Fault", "", HACategory::Diagnostic,
        [](JsonObject &c) {
            c["value_template"]        = "{{ 'ON' if value_json.fault is defined else 'OFF' }}";
            c["device_class"]          = "problem";
            c["json_attributes_topic"]    = "~/link/state";
            c["json_attributes_template"] = "{{ value_json.fault | tojson if value_json.fault is defined else '{}' }}";
        });

    link->begin();
    return link;
}

esp_err_t GeniusDevicesService::_publishLinkState(uint32_t deviceId)
{
    if (!_haService->isReady())
        return ESP_ERR_INVALID_STATE;

    HAGroupedSensorPublisher *pub = nullptr;
    beginTransaction();
    auto it = _haDevices.find(deviceId);
    if (it != _haDevices.end() && it->second.link)
        pub = it->second.link.get();
    if (pub)
        pub->publishState();
    endTransaction();
    return pub ? ESP_OK : ESP_ERR_NOT_FOUND;
}
