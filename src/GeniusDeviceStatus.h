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

/*
 * Status flags of a Genius device - the "ErrorValue" bit field of Hekatron's FM.GW interface.
 *
 * A radio module announces every change of these flags with a status-change frame (36 bytes,
 * message type 0x00) carrying a mask of newly SET and a mask of newly CLEARED flags plus the
 * serial of the affected smoke detector. Changes are edge-triggered: a flag is announced once
 * when it changes, never repeated periodically. The FM.GW serial protocol forwards the same
 * triple 1:1 (IndNwkStateChange), which is where the bit names below come from.
 *
 * Bit 0 (alarm) is the special case driving the alarm logic (setAlarm/resetAlarm). Every other
 * bit - including bits not known yet - is kept as the device's persistent status.
 */
#define GENIUS_STATUS_ALARM 0x0001    ///< Smoke alarm - handled by the alarm logic, never stored as status
#define GENIUS_STATUS_ERROR 0x0002    ///< Device fault ("Fehler")
#define GENIUS_STATUS_BATT_LOW 0x0004 ///< Battery low ("BattLow")
#define GENIUS_STATUS_RADIO 0x0008    ///< Radio link fault ("Funkstreckenfehler")
#define GENIUS_STATUS_MUTE 0x0010     ///< A radio link or dismount fault was muted (informational)
#define GENIUS_STATUS_DISMOUNT 0x0020 ///< Detector dismounted from its base ("Demontagefehler")

/// Every bit that is stored as device status: all but the alarm bit, including bits not known yet.
#define GENIUS_STATUS_TRACKED_MASK ((uint16_t)~GENIUS_STATUS_ALARM)

/// Bits that indicate a problem with the device. MUTE is informational; unknown bits are not counted.
#define GENIUS_STATUS_FAULT_MASK (GENIUS_STATUS_ERROR | GENIUS_STATUS_BATT_LOW | GENIUS_STATUS_RADIO | GENIUS_STATUS_DISMOUNT)

/// Bits an acoustic readout reports as well (detector fault, battery, radio fault).
#define GENIUS_STATUS_READOUT_MASK (GENIUS_STATUS_ERROR | GENIUS_STATUS_BATT_LOW | GENIUS_STATUS_RADIO)

/// Metadata of one known status flag.
struct GeniusStatusFlagInfo
{
    uint16_t bit;      ///< Flag bit (GENIUS_STATUS_*)
    const char *key;   ///< Machine-readable key (MQTT payloads, HA state JSON)
    const char *label; ///< Human-readable label (logs)
};

/// All known status flags, in display order. Bits missing here are reported as "flag_0xNNNN".
static constexpr GeniusStatusFlagInfo GENIUS_STATUS_FLAGS[] = {
    {GENIUS_STATUS_BATT_LOW, "battery_low", "Battery low"},
    {GENIUS_STATUS_ERROR, "device_fault", "Device fault"},
    {GENIUS_STATUS_RADIO, "radio_fault", "Radio link fault"},
    {GENIUS_STATUS_DISMOUNT, "dismounted", "Dismounted"},
    {GENIUS_STATUS_MUTE, "fault_muted", "Fault muted"},
};

/// Metadata for @p bit, or nullptr if the bit is not known (yet).
static inline const GeniusStatusFlagInfo *geniusStatusFlagInfo(uint16_t bit)
{
    for (const GeniusStatusFlagInfo &info : GENIUS_STATUS_FLAGS)
        if (info.bit == bit)
            return &info;
    return nullptr;
}
