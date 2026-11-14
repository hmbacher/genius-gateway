/**
 * @file GeniusDeviceRuntime.h
 * @brief RAM-only runtime state per device: last seen, live RSSI and radio-link faults.
 *
 * Pure C++ (no ESP-IDF / Arduino includes) so the state machine and the frame classifier can be unit
 * tested on a host machine (see tests/native/). The clock is injected by the caller:
 *   - nowMs: monotonic milliseconds (esp_timer), used for all expiry decisions
 *   - wall:  wall-clock seconds (time_t) for display, 0 when the system time is not valid yet
 *
 * IMPORTANT: nothing in this file (or in the service that owns an instance) may be written to flash.
 * The values deliberately live outside of GeniusDevice/GeniusDevices, whose JSON is persisted by
 * FSPersistence on every update handler call.
 *
 * Observed telegrams (see docs/reverse-engineering/protocol-analysis.md, "Radio link supervision"):
 *   0x33/0x00, 28 B  supervision series (heartbeat), not forwarded
 *   0x33/0x02, 32 B  "module missing": payload bytes 28-31 = serial number of the missing radio module (BE)
 *   0x00/0x00, 36 B  alarm family; byte 28 = 0x08 radio network fault, 0x10 fault acknowledged,
 *                    bytes 32-35 = smoke detector serial number (LE)
 *
 * @license AGPL-3.0 with Commons Clause (see LICENSE)
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace genius_rt
{

/// Faults disappear this long after the last frame that mentioned them.
constexpr uint64_t FAULT_TTL_MS = 48ULL * 60ULL * 60ULL * 1000ULL;

// ---------------------------------------------------------------------------------------------
// Frame classification (byte offsets as in GeniusGateway.h)
// ---------------------------------------------------------------------------------------------

constexpr size_t POS_FAMILY = 26;
constexpr size_t POS_TYPE = 27;
constexpr size_t POS_CODE = 28;        ///< alarm family: start/fault code
constexpr size_t POS_STOP_FLAG = 30;   ///< alarm family: silence flag
constexpr size_t POS_DETECTOR_SN = 32; ///< alarm family: smoke detector serial (little endian)
constexpr size_t POS_MISSING_SN = 28;  ///< 0x33/0x02: serial of the missing radio module (big endian)

constexpr uint32_t GATEWAY_SN = 0xFFFFFFFEu; ///< serial number the gateway itself uses as sender (never a device)

constexpr uint8_t FAMILY_ALARM = 0x00;
constexpr uint8_t FAMILY_SUPERVISION = 0x33;

constexpr uint8_t CODE_RADIO_FAULT = 0x08;
constexpr uint8_t CODE_FAULT_ACK = 0x10;

constexpr size_t LEN_SUPERVISION_BEACON = 28;
constexpr size_t LEN_SUPERVISION_MISSING = 32;
constexpr size_t LEN_ALARM = 36;

enum class SupKind : uint8_t
{
    None = 0,
    Beacon,     ///< 0x33/0x00 daily series (heartbeat of the radio link supervision)
    Missing,    ///< 0x33/0x02 names the missing radio module
    RadioFault, ///< 0x00/0x00 code 0x08
    FaultAck    ///< 0x00/0x00 code 0x10
};

inline uint32_t be32(const uint8_t *d, size_t pos)
{
    return (uint32_t(d[pos]) << 24) | (uint32_t(d[pos + 1]) << 16) | (uint32_t(d[pos + 2]) << 8) | uint32_t(d[pos + 3]);
}

inline uint32_t le32(const uint8_t *d, size_t pos)
{
    return (uint32_t(d[pos + 3]) << 24) | (uint32_t(d[pos + 2]) << 16) | (uint32_t(d[pos + 1]) << 8) | uint32_t(d[pos]);
}

/// Classify the supervision-related frames. Everything else (incl. fire alarms, code 0x01) is None.
inline SupKind classifySupervision(const uint8_t *d, size_t len)
{
    if (!d || len <= POS_TYPE)
        return SupKind::None;
    const uint8_t family = d[POS_FAMILY];
    const uint8_t type = d[POS_TYPE];
    if (family == FAMILY_SUPERVISION && type == 0x00 && len == LEN_SUPERVISION_BEACON)
        return SupKind::Beacon;
    if (family == FAMILY_SUPERVISION && type == 0x02 && len == LEN_SUPERVISION_MISSING)
        return SupKind::Missing;
    if (family == FAMILY_ALARM && type == 0x00 && len == LEN_ALARM && d[POS_STOP_FLAG] == 0)
    {
        if (d[POS_CODE] == CODE_RADIO_FAULT)
            return SupKind::RadioFault;
        if (d[POS_CODE] == CODE_FAULT_ACK)
            return SupKind::FaultAck;
    }
    return SupKind::None;
}

// ---------------------------------------------------------------------------------------------
// Runtime state
// ---------------------------------------------------------------------------------------------

enum class FaultState : uint8_t
{
    None = 0,
    Warning, ///< "module missing" (0x33/0x02) seen, the fault frame (0x08) has not arrived (yet)
    Active,  ///< radio network fault (0x08)
    Acked    ///< fault acknowledged on the detector (0x10)
};

struct Fault
{
    FaultState state = FaultState::None;
    uint32_t causeSn = 0;      ///< radio module reported as missing (0 = unknown)
    bool startUnknown = false; ///< acknowledgement seen without a preceding fault/warning (e.g. after a reboot)
    uint64_t lastMs = 0;       ///< last frame that (re-)confirmed the fault; drives the expiry
    int64_t sinceWall = 0;     ///< first frame of this episode (warning or fault), wall clock
    int64_t activeWall = 0;    ///< first 0x08 frame of this episode
    int64_t lastWall = 0;      ///< last confirming frame
    int64_t ackWall = 0;       ///< acknowledgement
};

struct DeviceRuntime
{
    uint32_t deviceId = 0;

    bool hasSeen = false; ///< any frame originated by / sent by this device was received
    int64_t seenWall = 0;
    uint64_t seenMs = 0;

    bool hasRssi = false; ///< RSSI of the last frame this device transmitted (direct link to the gateway)
    int8_t rssi = 0;
    int64_t rssiWall = 0;
    uint64_t rssiMs = 0;

    Fault fault;
};

/// Bit mask returned by the update functions: what changed in a way worth publishing.
enum Change : uint8_t
{
    CHANGE_NONE = 0,
    CHANGE_SEEN = 1,
    CHANGE_RSSI = 2,
    CHANGE_FAULT = 4
};

class RuntimeTable
{
public:
    RuntimeTable() { _items.reserve(16); }

    const DeviceRuntime *find(uint32_t deviceId) const
    {
        for (const auto &r : _items)
            if (r.deviceId == deviceId)
                return &r;
        return nullptr;
    }

    const std::vector<DeviceRuntime> &all() const { return _items; }

    void remove(uint32_t deviceId)
    {
        for (size_t i = 0; i < _items.size(); i++)
            if (_items[i].deviceId == deviceId)
            {
                _items.erase(_items.begin() + i);
                return;
            }
    }

    void clear() { _items.clear(); }

    /// A frame originated by / sent by this device was received.
    uint8_t seen(uint32_t id, uint64_t nowMs, int64_t wall)
    {
        DeviceRuntime &r = get(id);
        r.hasSeen = true;
        r.seenMs = nowMs;
        r.seenWall = wall;
        return CHANGE_SEEN;
    }

    /// RSSI of a frame this device transmitted itself (hops == 0 for the originator).
    uint8_t rssi(uint32_t id, int8_t dbm, uint64_t nowMs, int64_t wall)
    {
        DeviceRuntime &r = get(id);
        r.hasRssi = true;
        r.rssi = dbm;
        r.rssiMs = nowMs;
        r.rssiWall = wall;
        return CHANGE_RSSI;
    }

    /// 0x33/0x02: the sender reports radio module `missingSn` as missing -> warning (or refreshes a fault).
    uint8_t linkMissing(uint32_t id, uint32_t missingSn, uint64_t nowMs, int64_t wall)
    {
        DeviceRuntime &r = get(id);
        Fault &f = r.fault;
        uint8_t change = CHANGE_NONE;
        if (f.state == FaultState::None)
        {
            f = Fault{};
            f.state = FaultState::Warning;
            f.sinceWall = wall;
            change = CHANGE_FAULT;
        }
        if (f.causeSn != missingSn)
        {
            f.causeSn = missingSn;
            change = CHANGE_FAULT;
        }
        f.lastMs = nowMs;
        f.lastWall = wall;
        return change;
    }

    /// 0x00/0x00 code 0x08: radio network fault. A repeated fault after an acknowledgement is a new, active fault.
    uint8_t fault(uint32_t id, uint64_t nowMs, int64_t wall)
    {
        DeviceRuntime &r = get(id);
        Fault &f = r.fault;
        uint8_t change = CHANGE_NONE;
        if (f.state == FaultState::None)
        {
            f = Fault{};
            f.sinceWall = wall;
            f.activeWall = wall;
            f.state = FaultState::Active;
            change = CHANGE_FAULT;
        }
        else if (f.state == FaultState::Warning)
        {
            f.state = FaultState::Active;
            f.activeWall = wall;
            change = CHANGE_FAULT;
        }
        else if (f.state == FaultState::Acked)
        {
            f.state = FaultState::Active;
            f.activeWall = wall;
            f.ackWall = 0;
            f.startUnknown = false;
            change = CHANGE_FAULT;
        }
        f.lastMs = nowMs;
        f.lastWall = wall;
        return change;
    }

    /// 0x00/0x00 code 0x10: fault acknowledged. Without a known fault it is shown with an unknown start.
    uint8_t ack(uint32_t id, uint64_t nowMs, int64_t wall)
    {
        DeviceRuntime &r = get(id);
        Fault &f = r.fault;
        uint8_t change = CHANGE_NONE;
        if (f.state == FaultState::None)
        {
            f = Fault{};
            f.startUnknown = true;
            f.sinceWall = wall;
            f.state = FaultState::Acked;
            f.ackWall = wall;
            change = CHANGE_FAULT;
        }
        else if (f.state != FaultState::Acked)
        {
            f.state = FaultState::Acked;
            f.ackWall = wall;
            change = CHANGE_FAULT;
        }
        f.lastMs = nowMs;
        f.lastWall = wall;
        return change;
    }

    /// Drop faults that were not confirmed for FAULT_TTL_MS. Returns the ids that changed.
    std::vector<uint32_t> expire(uint64_t nowMs)
    {
        std::vector<uint32_t> changed;
        for (auto &r : _items)
        {
            if (r.fault.state != FaultState::None && nowMs >= r.fault.lastMs && nowMs - r.fault.lastMs > FAULT_TTL_MS)
            {
                r.fault = Fault{};
                changed.push_back(r.deviceId);
            }
        }
        return changed;
    }

private:
    std::vector<DeviceRuntime> _items;

    DeviceRuntime &get(uint32_t id)
    {
        for (auto &r : _items)
            if (r.deviceId == id)
                return r;
        _items.emplace_back();
        _items.back().deviceId = id;
        return _items.back();
    }
};

} // namespace genius_rt
