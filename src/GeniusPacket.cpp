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

    /* Determine the Genius packet type from its on-air message-family byte (offset 26) and
     * message-type byte (offset 27), using the length as a validator. This mirrors how the
     * genuine radio module routes a received frame. Neither the type byte nor the length
     * is enough on its own: type 0x00 is a status change in family 0x00, the
     * CommissioningProbe Request in family 0x66 and the daily supervision series in family
     * 0x33 - the latter two even share their length (28 B) - and a 36-byte frame is either
     * a status change (0x00/0x00) or a ConfigCheckProbe response (0x55/0x08). */
    const uint8_t msgType = data[DATAPOS_MSG_TYPE];

    switch (data[DATAPOS_MSG_FAMILY])
    {
    case MSGFAMILY_STATUS: // 0x00
        if (msgType == MSGTYPE_STATUS_CHANGE && length == LEN_STATUS_CHANGE_PACKET)
        {
            /* Tail: [set mask u16 LE][cleared mask u16 LE][detector serial u32 LE]. Alarm start/stop
             * is the GENIUS_STATUS_ALARM bit of these masks - interpretation is up to the consumer. */
            out->type = HPT_STATUS_CHANGE;
            out->status_set = readU16LE(data, DATAPOS_STATUS_SET_MASK);
            out->status_cleared = readU16LE(data, DATAPOS_STATUS_CLEARED_MASK);
            out->detector_sn = readU32LE(data, DATAPOS_STATUS_DETECTOR_SN);
        }
        break;

    case MSGFAMILY_SUPERVISION: // 0x33  (FM Pro X with the DIP switch "radio link supervision")
        if (msgType == MSGTYPE_SUPERVISION_SERIES && length == LEN_SUPERVISION_SERIES_PACKET)
        {
            out->type = HPT_SUPERVISION_SERIES;
        }
        else if (msgType == MSGTYPE_SUPERVISION_ROLL_CALL && length == LEN_SUPERVISION_ROLL_CALL_PACKET)
        {
            out->type = HPT_SUPERVISION_ROLL_CALL;
        }
        else if (msgType == MSGTYPE_MODULE_MISSING && length == LEN_MODULE_MISSING_PACKET)
        {
            out->type = HPT_MODULE_MISSING;
            out->missing_module_sn = readU32BE(data, DATAPOS_MODULE_MISSING_SN);
        }
        else if (msgType == MSGTYPE_MODULE_PRESENT && length == LEN_MODULE_PRESENT_PACKET)
        {
            // Not captured on air yet (same layout as "module missing")
            out->type = HPT_MODULE_PRESENT;
            out->present_module_sn = readU32BE(data, DATAPOS_MODULE_PRESENT_SN);
        }
        break;

    case MSGFAMILY_LINE_CLOCK: // 0x22  (sent in a supervision round, before the roll call)
        if (msgType == MSGTYPE_LINE_CLOCK && length == LEN_LINE_CLOCK_PACKET)
            out->type = HPT_LINE_CLOCK;
        break;

    case MSGFAMILY_COMMISSIONING: // 0x66
        switch (msgType)
        {
        case MSGTYPE_COMMISSIONING: // 0x03  (alarm-line commissioning)
            if (length == LEN_COMMISSIONING_PACKET)
                out->type = HPT_COMMISSIONING;
            break;

        case MSGTYPE_COMMISSIONING_PROBE_REQUEST: // 0x00  (commissioning context, direct range only)
            if (length == LEN_COMMISSIONING_PROBE_REQUEST_PACKET)
                out->type = HPT_COMMISSIONING_PROBE_REQUEST;
            break;

        case MSGTYPE_COMMISSIONING_PROBE_RESPONSE: // 0x01  (commissioning context, carries Req-SN)
            if (length == LEN_COMMISSIONING_PROBE_RESPONSE_PACKET)
                out->type = HPT_COMMISSIONING_PROBE_RESPONSE;
            break;

        case MSGTYPE_LINE_TEST: // 0x04
            if (length != LEN_LINE_TEST_PACKET)
                break;
            // The test mode byte takes exactly three values. Each is named here rather than
            // tested by a bit, so a value that is not known stays unclassified.
            switch (data[DATAPOS_LINE_TEST_START_STOP_FLAG])
            {
            case GENIUS_TEST_MODE_LINE_TEST: // 0x04
                out->type = HPT_LINE_TEST_START;
                break;
            case GENIUS_TEST_MODE_NETWORK_TEST: // 0x06
                out->type = HPT_NETWORK_TEST_START;
                break;
            case GENIUS_TEST_MODE_STOP: // 0x00 - ends a line test as well as a network test
                out->type = HPT_LINE_TEST_STOP;
                break;
            default:
                break;
            }
            break;

        default:
            break;
        }
        break;

    case MSGFAMILY_CONFIG_CHECK_PROBE: // 0x55
        if (msgType == MSGTYPE_CONFIG_CHECK_PROBE_REQUEST) // 0x06  (wildcard direct-range probe)
        {
            if (length == LEN_CONFIG_CHECK_PROBE_REQUEST_PACKET)
                out->type = HPT_CONFIG_CHECK_PROBE_REQUEST;
        }
        else if (msgType == MSGTYPE_CONFIG_CHECK_PROBE_RESPONSE) // 0x08  (36 B, like a status change)
        {
            if (length == LEN_CONFIG_CHECK_PROBE_RESPONSE_PACKET)
                out->type = HPT_CONFIG_CHECK_PROBE_RESPONSE;
        }
        break;

    default: // 0x11, 0x22 and anything not known yet
        break;
    }

    /* The header is the same for every message, also for those not known yet: who originated
     * the frame, who transmitted it, on which line. A frame of an unknown type is still a sign
     * of life of these radio modules. */
    {
        out->origin_id = readU32BE(data, DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID);
        out->sender_id = readU32BE(data, DATAPOS_GENERAL_SENDER_RADIO_MODULE_ID);
        out->line_id = readU32BE(data, DATAPOS_GENERAL_LINE_ID);
        out->hops = HOPS_FIRST - data[DATAPOS_GENERAL_HOPS];
    }

    return true;
}
