/**
 * @file GeniusPacket.h
 * @brief Genius radio protocol: frame layout, packet types and frame classification
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

/* Kept free of framework/Arduino dependencies on purpose, so the frame classification can be
 * compiled and unit-tested on the host. */
#include <stddef.h>
#include <stdint.h>

#define HOPS_FIRST 0xF ///< Initial hops value for packet routing
#define HOPS_LAST 0x0  ///< Final hops value for packet routing

#define LEN_COMMISSIONING_PACKET 37                ///< Commissioning packet length
#define LEN_COMMISSIONING_PROBE_REQUEST_PACKET 28  ///< CommissioningProbe request packet length
#define LEN_COMMISSIONING_PROBE_RESPONSE_PACKET 32 ///< CommissioningProbe response packet length
#define LEN_STATUS_CHANGE_PACKET 36                ///< Status change packet length (alarm start/stop, battery low, faults, ...)
#define LEN_LINE_TEST_PACKET 29                    ///< Line test packet length
#define LEN_CONFIG_CHECK_PROBE_REQUEST_PACKET 28   ///< ConfigCheckProbe request length (same wire length as CommissioningProbe Request; distinguished by the message-type byte)
#define LEN_CONFIG_CHECK_PROBE_RESPONSE_PACKET 36  ///< ConfigCheckProbe response length (same wire length as a status change; distinguished by the message-type byte)

#define DATAPOS_GENERAL_ORIGIN_RADIO_MODULE_ID 9  ///< Origin radio module ID position
#define DATAPOS_GENERAL_SENDER_RADIO_MODULE_ID 14 ///< Sender radio module ID position
#define DATAPOS_GENERAL_LINE_ID 18                ///< Line ID position
#define DATAPOS_GENERAL_HOPS 22                   ///< Hops counter position
#define DATAPOS_COMISSIONING_NEW_LINE_ID 28       ///< New line ID position in commissioning packets
#define DATAPOS_COMOSSIONING_TIME_HOUR 32         ///< Hour position in commissioning packets
#define DATAPOS_COMOSSIONING_TIME_MINUTE 33       ///< Minute position in commissioning packets
#define DATAPOS_COMOSSIONING_TIME_SECOND 34       ///< Second position in commissioning packets
#define DATAPOS_STATUS_SET_MASK 28                ///< Status change: status flags newly SET (uint16, little endian)
#define DATAPOS_STATUS_CLEARED_MASK 30            ///< Status change: status flags newly CLEARED (uint16, little endian)
#define DATAPOS_STATUS_DETECTOR_SN 32             ///< Status change: serial of the affected smoke detector (uint32, little endian)
#define DATAPOS_LINE_TEST_START_STOP_FLAG 28      ///< Line test start/stop flag position
#define DATAPOS_MSG_TYPE 27                       ///< Message-type discriminator byte (constant per packet type - how the radio module routes a received frame)
#define DATAPOS_CONFIG_CHECK_PROBE_STATUS 29      ///< ConfigCheckProbe response: per-device status/capability flag
#define DATAPOS_CONFIG_CHECK_PROBE_GROUPLINE 30   ///< ConfigCheckProbe response: detector group/line (hi nibble = group A-H, lo nibble = line 0-9)

/* Message-type byte values (data[DATAPOS_MSG_TYPE]) - the on-air type discriminator the
 * genuine radio module routes on. Length disambiguates the single shared value (0x00). */
#define MSGTYPE_STATUS_CHANGE_OR_COMMISSIONING_PROBE_REQ 0x00 ///< Status change (36 B) OR CommissioningProbe Request (28 B)
#define MSGTYPE_COMMISSIONING_PROBE_RESPONSE 0x01             ///< CommissioningProbe Response (32 B, carries Req-SN)
#define MSGTYPE_COMMISSIONING 0x03                            ///< Alarm-line commissioning (37 B)
#define MSGTYPE_LINE_TEST 0x04                                ///< Line test start/stop (29 B)
#define MSGTYPE_CONFIG_CHECK_PROBE_05 0x05                    ///< ConfigCheckProbe probe (29 B) - provisional (not yet capture-confirmed)
#define MSGTYPE_CONFIG_CHECK_PROBE_REQUEST 0x06               ///< ConfigCheckProbe request (28 B)
#define MSGTYPE_CONFIG_CHECK_PROBE_RESPONSE 0x08              ///< ConfigCheckProbe response (36 B)

#define GATEWAY_ID 0xFFFFFFFE ///< Gateway identifier for genius protocol

typedef enum genius_packet_type
{
  HPT_UNKNOWN = -1,                 ///< Unknown packet type
  HPT_COMMISSIONING = 0,            ///< Commissioning packet (smoke detector assignment to alarm line)
  HPT_COMMISSIONING_PROBE_REQUEST,  ///< CommissioningProbe request packet (request for smoke detectors to identify)
  HPT_COMMISSIONING_PROBE_RESPONSE, ///< CommissioningProbe response packet (smoke detector identification response)
  HPT_STATUS_CHANGE,                ///< Status change packet: set/cleared status flags of one smoke detector (alarm start/stop is the alarm-bit case)
  HPT_LINE_TEST_START,              ///< Line test start packet (line test initiation)
  HPT_LINE_TEST_STOP,               ///< Line test stop packet (line test completion)
  HPT_CONFIG_CHECK_PROBE_REQUEST,   ///< ConfigCheckProbe request (radio module 0x55-family, subtype 0x06; direct-range probe)
  HPT_CONFIG_CHECK_PROBE_RESPONSE   ///< ConfigCheckProbe response (radio module 0x55-family, subtype 0x08; carries responder group/line + status)
} genius_packet_type_t;

typedef struct genius_packet_t
{
  genius_packet_type_t type; ///< Packet type classification
  uint32_t origin_id;        ///< Original sender radio module ID
  uint32_t sender_id;        ///< Current sender radio module ID
  uint32_t line_id;          ///< Associated alarm line ID
  uint8_t hops;              ///< Hop count for packet routing
  uint16_t status_set;       ///< HPT_STATUS_CHANGE: status flags newly set (GENIUS_STATUS_*)
  uint16_t status_cleared;   ///< HPT_STATUS_CHANGE: status flags newly cleared (GENIUS_STATUS_*)
  uint32_t detector_sn;      ///< HPT_STATUS_CHANGE: serial of the affected smoke detector
} genius_packet_t;

/**
 * @brief Classify a received Genius frame and extract its fields.
 *
 * The packet type is determined from the on-air message-type byte (offset 27), with the
 * per-type length as a validator - mirroring how the genuine radio module routes a frame.
 * A status change is classified by type and length only; which flags it sets or clears is
 * left to the consumer, so new flags need no change here.
 *
 * @param data    Raw frame bytes (as received, starting with the 0x02 lead byte)
 * @param length  Frame length in bytes
 * @param out     Receives the classification; type is HPT_UNKNOWN for unrecognized frames
 * @return false if a pointer argument is null, true otherwise
 */
bool genius_analyze_packet(const uint8_t *data, size_t length, genius_packet_t *out);
