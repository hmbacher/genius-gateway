/**
 * @file GeniusDeviceStatus.h
 * @brief Genius device status flags (ErrorValue bit field) and their metadata
 *
 * @copyright Copyright (c) 2024-2025 Genius Gateway Project
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

#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include <vector>

/*
 * Status flags of a Genius device - the "ErrorValue" bit field of Hekatron's FM.GW interface.
 *
 * A radio module announces every change of these flags with a status-change frame (36 bytes,
 * message family 0x00, type 0x00) carrying a mask of newly SET and a mask of newly CLEARED flags
 * plus the serial of the affected smoke detector. Changes are edge-triggered: a flag is announced
 * once when it changes, never repeated periodically. The FM.GW serial protocol forwards the same
 * triple 1:1 (IndNwkStateChange), which is where the bit names below come from.
 *
 * Bit 0 (alarm) is the special case driving the alarm logic (setAlarm/resetAlarm). Every other
 * bit - including bits not known yet - is kept as the device's status.
 */
#define GENIUS_STATUS_ALARM 0x0001UL    ///< Smoke alarm - handled by the alarm logic, never stored as status
#define GENIUS_STATUS_ERROR 0x0002UL    ///< Device fault ("Fehler")
#define GENIUS_STATUS_BATT_LOW 0x0004UL ///< Battery low ("BattLow")
#define GENIUS_STATUS_RADIO 0x0008UL    ///< Radio link fault ("Funkstreckenfehler")
#define GENIUS_STATUS_MUTE 0x0010UL     ///< A radio link or dismount fault was muted on the detector (informational)
#define GENIUS_STATUS_DISMOUNT 0x0020UL ///< Detector dismounted from its base ("Demontagefehler")

/// Gateway-defined flag, not an on-air bit: the detector's radio module was named as missing by
/// the radio link supervision of another detector (see GDE_MODULE_MISSING).
#define GENIUS_STATUS_REPORTED_MISSING 0x00010000UL

/// On-air bits that are stored as device status: all but the alarm bit, including bits not known yet.
#define GENIUS_STATUS_TRACKED_MASK 0x0000FFFEUL

/// Every flag a device status can hold.
#define GENIUS_STATUS_STORED_MASK (GENIUS_STATUS_TRACKED_MASK | GENIUS_STATUS_REPORTED_MISSING)

/// Flags that indicate a problem with the device. MUTE is informational; unknown bits are not counted.
#define GENIUS_STATUS_FAULT_MASK (GENIUS_STATUS_ERROR | GENIUS_STATUS_BATT_LOW | GENIUS_STATUS_RADIO | GENIUS_STATUS_DISMOUNT | GENIUS_STATUS_REPORTED_MISSING)

/// Flags an acoustic readout reports as well (detector fault, battery, radio fault).
#define GENIUS_STATUS_READOUT_MASK (GENIUS_STATUS_ERROR | GENIUS_STATUS_BATT_LOW | GENIUS_STATUS_RADIO)

/// Faults a MUTE can refer to. A mute is only kept while one of them is active.
#define GENIUS_STATUS_MUTABLE_MASK (GENIUS_STATUS_RADIO | GENIUS_STATUS_DISMOUNT)

/// Metadata of one known status flag.
struct GeniusStatusFlagInfo
{
    uint32_t bit;      ///< Flag (GENIUS_STATUS_*)
    const char *key;   ///< Machine-readable key (MQTT payloads, HA state JSON)
    const char *label; ///< Human-readable label (logs)
};

/// All known status flags, in display order. Bits missing here are reported as "flag_0xNNNN".
static constexpr GeniusStatusFlagInfo GENIUS_STATUS_FLAGS[] = {
    {GENIUS_STATUS_BATT_LOW, "battery_low", "Battery low"},
    {GENIUS_STATUS_ERROR, "device_fault", "Device fault"},
    {GENIUS_STATUS_RADIO, "radio_fault", "Radio link fault"},
    {GENIUS_STATUS_DISMOUNT, "dismounted", "Dismounted"},
    {GENIUS_STATUS_REPORTED_MISSING, "reported_missing", "Reported missing"},
    {GENIUS_STATUS_MUTE, "fault_muted", "Fault muted"},
};

/// Metadata for @p bit, or nullptr if the bit is not known (yet).
static inline const GeniusStatusFlagInfo *geniusStatusFlagInfo(uint32_t bit)
{
    for (const GeniusStatusFlagInfo &info : GENIUS_STATUS_FLAGS)
        if (info.bit == bit)
            return &info;
    return nullptr;
}

/// Timestamps before this (2020-01-01T00:00:00Z) mean "time unknown": the clock was not set yet.
#define GENIUS_TIME_VALID_MIN 1577836800L

static inline bool geniusTimeValid(time_t t)
{
    return t >= (time_t)GENIUS_TIME_VALID_MIN;
}

#ifndef GENIUS_MAX_DEVICE_EVENTS
#define GENIUS_MAX_DEVICE_EVENTS 50 ///< Entries kept in a device's activity log (ring buffer)
#endif

/// Kinds of activity log entries. The values are persisted and also serve as the source of a
/// flag state (readout, status packet), so they must not change.
enum genius_device_event_t : uint8_t
{
    GDE_CREATED = 0,        ///< Device was added to the gateway (value = registration type)
    GDE_READOUT = 1,        ///< Acoustic readout (set = flags reported active, cleared = flags reported OK)
    GDE_STATUS_PACKET = 2,  ///< Status change received over the air (set / cleared = flags that changed)
    GDE_STATUS_RESET = 3,   ///< Status reset in the gateway: the active flags became unknown
    GDE_MODULE_MISSING = 4, ///< Radio link supervision of this device names a missing radio module (value = its serial)
    GDE_ALARM = 5,          ///< Alarm (reserved)
    GDE_MAX
};

/// Current state of one status flag. A flag without a state is unknown: the gateway has not
/// heard about it, so it is neither OK nor active.
struct GeniusFlagState
{
    uint32_t flag;  ///< The flag (a single GENIUS_STATUS_* bit)
    bool active;    ///< true = fault / condition present, false = reported OK
    time_t since;   ///< Since when this state holds (not valid = time unknown)
    uint8_t source; ///< Where the state comes from: GDE_READOUT or GDE_STATUS_PACKET
};

/// One activity log entry.
struct GeniusDeviceEvent
{
    time_t time;      ///< When it happened (not valid = time unknown)
    uint8_t type;     ///< genius_device_event_t
    uint32_t set;     ///< GDE_READOUT / GDE_STATUS_PACKET: flags reported active
    uint32_t cleared; ///< GDE_READOUT / GDE_STATUS_PACKET: flags reported OK
    uint32_t value;   ///< GDE_CREATED: registration type; GDE_MODULE_MISSING: serial of the missing radio module
};

/// What a report changed.
struct GeniusStatusDelta
{
    uint32_t set = 0;     ///< Flags that became active
    uint32_t cleared = 0; ///< Flags that became OK
    uint32_t removed = 0; ///< Flags that became unknown again

    bool any() const { return (set | cleared | removed) != 0; }
};

/**
 * @brief Status of one Genius device: the current state of each known flag plus an activity log.
 *
 * The state is fed by status changes received over the air, by acoustic readouts and by manual
 * resets. The newest information wins per flag. Plain C++ without framework dependencies, so the
 * rules can be checked on a host build.
 */
class GeniusDeviceStatus
{
public:
    std::vector<GeniusFlagState> flags;    ///< Flags whose state is known, in the order they became known
    std::vector<GeniusDeviceEvent> events; ///< Activity log, oldest first, at most GENIUS_MAX_DEVICE_EVENTS

    /// State of @p flag, or nullptr while it is unknown.
    const GeniusFlagState *find(uint32_t flag) const
    {
        for (const GeniusFlagState &state : flags)
            if (state.flag == flag)
                return &state;
        return nullptr;
    }

    /// Flags that are currently active.
    uint32_t activeMask() const
    {
        uint32_t mask = 0;
        for (const GeniusFlagState &state : flags)
            if (state.active)
                mask |= state.flag;
        return mask;
    }

    /// Flags whose state is known (active or OK).
    uint32_t knownMask() const
    {
        uint32_t mask = 0;
        for (const GeniusFlagState &state : flags)
            mask |= state.flag;
        return mask;
    }

    /**
     * @brief Apply a report: the flags in @p setMask are active, those in @p clearedMask are OK.
     *
     * - A flag that already has the reported state is left alone (its "since" stays).
     * - A report that is older than what is known about a flag does not change that flag.
     * - A mute is only taken over while a radio link or dismount fault is active, and it goes
     *   away together with that fault. A mute without such a fault is ignored.
     *
     * @return the flags whose state changed
     */
    GeniusStatusDelta apply(uint32_t setMask, uint32_t clearedMask, time_t when, uint8_t source)
    {
        GeniusStatusDelta delta;

        setMask &= GENIUS_STATUS_STORED_MASK;
        clearedMask &= GENIUS_STATUS_STORED_MASK & ~setMask; // named in both masks: set wins

        const bool muteSet = (setMask & GENIUS_STATUS_MUTE) != 0;
        const bool muteCleared = (clearedMask & GENIUS_STATUS_MUTE) != 0;
        setMask &= ~GENIUS_STATUS_MUTE;
        clearedMask &= ~GENIUS_STATUS_MUTE;

        for (uint32_t bit = 1; bit != 0; bit <<= 1)
        {
            const bool set = (setMask & bit) != 0;
            if (!set && !(clearedMask & bit))
                continue;

            GeniusFlagState *state = _find(bit);
            if (state)
            {
                if (geniusTimeValid(when) && geniusTimeValid(state->since) && when < state->since)
                    continue; // older than what is known
                if (state->active == set)
                    continue; // confirmation only
                state->active = set;
                state->since = when;
                state->source = source;
            }
            else
            {
                flags.push_back({bit, set, when, source});
            }
            (set ? delta.set : delta.cleared) |= bit;
        }

        const bool mutableFault = (activeMask() & GENIUS_STATUS_MUTABLE_MASK) != 0;
        const bool muted = _find(GENIUS_STATUS_MUTE) != nullptr;
        if (muted && (!mutableFault || muteCleared))
        {
            _remove(GENIUS_STATUS_MUTE);
            delta.removed |= GENIUS_STATUS_MUTE;
        }
        else if (!muted && mutableFault && muteSet)
        {
            flags.push_back({GENIUS_STATUS_MUTE, true, when, source});
            delta.set |= GENIUS_STATUS_MUTE;
        }

        return delta;
    }

    /// Manual reset: every active flag becomes unknown. Flags reported OK are kept.
    GeniusStatusDelta reset()
    {
        GeniusStatusDelta delta;
        delta.removed = activeMask();
        for (size_t i = flags.size(); i-- > 0;)
            if (flags[i].active)
                flags.erase(flags.begin() + i);
        return delta;
    }

    /// Forget every flag state (the smoke detector was replaced). The activity log is kept.
    GeniusStatusDelta clear()
    {
        GeniusStatusDelta delta;
        delta.removed = knownMask();
        flags.clear();
        return delta;
    }

    /// Append to the activity log; the oldest entries make room.
    void addEvent(const GeniusDeviceEvent &event)
    {
        events.push_back(event);
        while (events.size() > GENIUS_MAX_DEVICE_EVENTS)
            events.erase(events.begin());
    }

private:
    GeniusFlagState *_find(uint32_t flag)
    {
        for (GeniusFlagState &state : flags)
            if (state.flag == flag)
                return &state;
        return nullptr;
    }

    void _remove(uint32_t flag)
    {
        for (size_t i = 0; i < flags.size(); i++)
            if (flags[i].flag == flag)
            {
                flags.erase(flags.begin() + i);
                return;
            }
    }
};
