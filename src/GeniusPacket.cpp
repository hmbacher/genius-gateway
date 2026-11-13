/**
 * @file GeniusPacket.cpp
 * @brief Genius radio protocol: frame classification
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

#include "GeniusPacket.h"

#include <string.h>

// Byte-order helpers - explicit shifts instead of ntohl() keep this file host-compilable.
static inline uint32_t readU32BE(const uint8_t *data, size_t pos)
{
    return ((uint32_t)data[pos] << 24) | ((uint32_t)data[pos + 1] << 16) |
           ((uint32_t)data[pos + 2] << 8) | (uint32_t)data[pos + 3];
}

static inline uint32_t readU32LE(const uint8_t *data, size_t pos)
{
    return (uint32_t)data[pos] | ((uint32_t)data[pos + 1] << 8) |
           ((uint32_t)data[pos + 2] << 16) | ((uint32_t)data[pos + 3] << 24);
}

static inline uint16_t readU16LE(const uint8_t *data, size_t pos)
{
    return (uint16_t)(data[pos] | (data[pos + 1] << 8));
}

bool genius_analyze_packet(const uint8_t *data, size_t length, genius_packet_t *out)
{
    if (!data || !out)
        return false;

    memset(out, 0, sizeof(genius_packet_t));
    out->type = HPT_UNKNOWN;

    /* A frame must be long enough to carry the message-type byte before we can classify it. */
    if (length <= DATAPOS_MSG_TYPE)
        return true;

    /* Determine the Genius packet type from its on-air message-type byte (offset 27), using
     * length as a validator. This mirrors how the genuine radio module routes a received
     * frame: on the type-specific tail byte plus an exact per-type length. Length ALONE is
     * ambiguous - message-type 0x00 is both a status change (36 B) and the CommissioningProbe
     * Request (28 B), and a 36-byte frame is either a status change (0x00) or a ConfigCheckProbe
     * response (0x08). */
    switch (data[DATAPOS_MSG_TYPE])
    {
    case MSGTYPE_COMMISSIONING: // 0x03  (alarm-line commissioning)
        if (length == LEN_COMMISSIONING_PACKET)
            out->type = HPT_COMMISSIONING;
        break;

    case MSGTYPE_COMMISSIONING_PROBE_RESPONSE: // 0x01  (commissioning-context, carries Req-SN)
        if (length == LEN_COMMISSIONING_PROBE_RESPONSE_PACKET)
            out->type = HPT_COMMISSIONING_PROBE_RESPONSE;
        break;

    case MSGTYPE_LINE_TEST: // 0x04  (firmware: line_state_processor, event 3)
        if (length != LEN_LINE_TEST_PACKET)
            break;
        if (data[DATAPOS_LINE_TEST_START_STOP_FLAG] == 0) // 0x00 = END/STOP of line test
            out->type = HPT_LINE_TEST_STOP;
        else if ((data[DATAPOS_LINE_TEST_START_STOP_FLAG] & 0x04) > 0) // 0x04/0x06 = START
            out->type = HPT_LINE_TEST_START;
        break;

    case MSGTYPE_CONFIG_CHECK_PROBE_REQUEST: // 0x06  (ConfigCheckProbe, wildcard direct-range probe)
        if (length == LEN_CONFIG_CHECK_PROBE_REQUEST_PACKET)
            out->type = HPT_CONFIG_CHECK_PROBE_REQUEST;
        break;

    case MSGTYPE_CONFIG_CHECK_PROBE_RESPONSE: // 0x08  (36 B - kept apart from status changes by the type byte)
        if (length == LEN_CONFIG_CHECK_PROBE_RESPONSE_PACKET)
            out->type = HPT_CONFIG_CHECK_PROBE_RESPONSE;
        break;

    case MSGTYPE_STATUS_CHANGE_OR_COMMISSIONING_PROBE_REQ: // 0x00  - shared value, split by length
        if (length == LEN_STATUS_CHANGE_PACKET)
        {
            /* Tail: [set mask u16 LE][cleared mask u16 LE][detector serial u32 LE]. Alarm start/stop
             * is the GENIUS_STATUS_ALARM bit of these masks - interpretation is up to the consumer. */
            out->type = HPT_STATUS_CHANGE;
            out->status_set = readU16LE(data, DATAPOS_STATUS_SET_MASK);
            out->status_cleared = readU16LE(data, DATAPOS_STATUS_CLEARED_MASK);
            out->detector_sn = readU32LE(data, DATAPOS_STATUS_DETECTOR_SN);
        }
        else if (length == LEN_COMMISSIONING_PROBE_REQUEST_PACKET)
            out->type = HPT_COMMISSIONING_PROBE_REQUEST;
        break;

    default:
        break;
    }

    if (out->type != HPT_UNKNOWN)
    {
        out->origin_id = readU32BE(data, DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID);
        out->sender_id = readU32BE(data, DATAPOS_GENERAL_SENDER_RADIO_MODULE_ID);
        out->line_id = readU32BE(data, DATAPOS_GENERAL_LINE_ID);
        out->hops = HOPS_FIRST - data[DATAPOS_GENERAL_HOPS];
    }

    return true;
}
