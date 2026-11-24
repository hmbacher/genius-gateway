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
#define LEN_SUPERVISION_SERIES_PACKET 28           ///< Daily supervision series length (same wire length as the CommissioningProbe request; distinguished by the family byte)
#define LEN_MODULE_MISSING_PACKET 32               ///< "Module missing" report length
#define LEN_SUPERVISION_ROLL_CALL_PACKET 28        ///< Roll call of the radio link supervision length
#define LEN_MODULE_PRESENT_PACKET 32               ///< "Present" answer to a "module missing" report length
#define LEN_LINE_CLOCK_PACKET 33                   ///< Line clock length
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
#define DATAPOS_LINE_TEST_START_STOP_FLAG 28      ///< Line / network test: test mode byte (GENIUS_TEST_MODE_*)
#define DATAPOS_MODULE_MISSING_SN 28              ///< "Module missing" report: serial of the missing radio module (uint32, big endian)
#define DATAPOS_MODULE_PRESENT_SN 28              ///< "Present" answer: serial of the radio module that is there (uint32, big endian)
#define DATAPOS_MSG_FAMILY 26                     ///< Message-family byte: first byte of the message body, groups the message types
#define DATAPOS_MSG_TYPE 27                       ///< Message-type byte within the family (constant per packet type - how the radio module routes a received frame)
#define DATAPOS_CONFIG_CHECK_PROBE_STATUS 29      ///< ConfigCheckProbe response: per-device status/capability flag
#define DATAPOS_CONFIG_CHECK_PROBE_GROUPLINE 30   ///< ConfigCheckProbe response: detector group/line (hi nibble = group A-H, lo nibble = line 0-9)

/* Message-family byte values (data[DATAPOS_MSG_FAMILY]). A message is identified by family
 * AND type: the type values repeat across the families (type 0x00 exists in five of them),
 * so the type byte alone - even with the length - does not tell the packets apart. */
#define MSGFAMILY_STATUS 0x00             ///< Status change (alarm, battery warning, faults)
#define MSGFAMILY_UNKNOWN_11 0x11         ///< Sent by a supervision round of FM Pro X modules (0x11/0x00 opens it, 0x11/0x01 closes its reports) - meaning beyond that unknown
#define MSGFAMILY_LINE_CLOCK 0x22         ///< Line clock, distributed in a supervision round of FM Pro X modules
#define MSGFAMILY_SUPERVISION 0x33        ///< Radio link supervision of FM Pro X modules (daily series, roll call, "module missing", "present")
#define MSGFAMILY_CONFIG_CHECK_PROBE 0x55 ///< ConfigCheckProbe (Linienabschlusstest)
#define MSGFAMILY_COMMISSIONING 0x66      ///< Alarm-line commissioning, CommissioningProbe and line test

/* Message-type byte values (data[DATAPOS_MSG_TYPE]) within their family. */
#define MSGTYPE_STATUS_CHANGE 0x00                ///< Family 0x00: status change (36 B)
#define MSGTYPE_COMMISSIONING_PROBE_REQUEST 0x00  ///< Family 0x66: CommissioningProbe Request (28 B)
#define MSGTYPE_COMMISSIONING_PROBE_RESPONSE 0x01 ///< Family 0x66: CommissioningProbe Response (32 B, carries Req-SN)
#define MSGTYPE_COMMISSIONING 0x03                ///< Family 0x66: alarm-line commissioning (37 B)
#define MSGTYPE_LINE_TEST 0x04                    ///< Family 0x66: line test start/stop (29 B)
#define MSGTYPE_SUPERVISION_SERIES 0x00           ///< Family 0x33: daily series of the radio link supervision (28 B)
#define MSGTYPE_SUPERVISION_ROLL_CALL 0x01        ///< Family 0x33: roll call - every module that hears it sends its series (28 B)
#define MSGTYPE_MODULE_MISSING 0x02               ///< Family 0x33: a module reports another radio module as missing (32 B, carries its serial)
#define MSGTYPE_MODULE_PRESENT 0x03               ///< Family 0x33: answer to a "module missing" report by a module that hears the named one (32 B, carries its serial) - not captured on air yet
#define MSGTYPE_LINE_CLOCK 0x00                   ///< Family 0x22: line clock (33 B: hour, minute, second, 16-bit day counter)
#define MSGTYPE_CONFIG_CHECK_PROBE_05 0x05        ///< Family 0x55: ConfigCheckProbe probe (29 B) - provisional (not yet capture-confirmed)
#define MSGTYPE_CONFIG_CHECK_PROBE_REQUEST 0x06   ///< Family 0x55: ConfigCheckProbe request (28 B)
#define MSGTYPE_CONFIG_CHECK_PROBE_RESPONSE 0x08  ///< Family 0x55: ConfigCheckProbe response (36 B)

/* Test mode byte of a line / network test frame (data[DATAPOS_LINE_TEST_START_STOP_FLAG]).
 * The radio modules send exactly these three values: a start with a 15-minute
 * timer (0x04), a start with a 30-minute timer (0x06) and the stop (0x00). The manufacturer
 * states about 15 minutes for a line test and about 30 minutes for a network test. */
#define GENIUS_TEST_MODE_STOP 0x00         ///< Stop the running line / network test
#define GENIUS_TEST_MODE_LINE_TEST 0x04    ///< Start a line test (alarm line, about 15 min)
#define GENIUS_TEST_MODE_NETWORK_TEST 0x06 ///< Start a network test (whole network, about 30 min)

#define GATEWAY_ID 0xFFFFFFFE ///< Gateway identifier for genius protocol

typedef enum genius_packet_type
{
  HPT_UNKNOWN = -1,                 ///< Unknown packet type
  HPT_COMMISSIONING = 0,            ///< Commissioning packet (smoke detector assignment to alarm line)
  HPT_COMMISSIONING_PROBE_REQUEST,  ///< CommissioningProbe request packet (request for smoke detectors to identify)
  HPT_COMMISSIONING_PROBE_RESPONSE, ///< CommissioningProbe response packet (smoke detector identification response)
  HPT_STATUS_CHANGE,                ///< Status change packet: set/cleared status flags of one smoke detector (alarm start/stop is the alarm-bit case)
  HPT_LINE_TEST_START,              ///< Line test start packet (test mode 0x04)
  HPT_LINE_TEST_STOP,               ///< Line / network test stop packet (test mode 0x00, ends either test)
  HPT_CONFIG_CHECK_PROBE_REQUEST,   ///< ConfigCheckProbe request (radio module 0x55-family, subtype 0x06; direct-range probe)
  HPT_CONFIG_CHECK_PROBE_RESPONSE,  ///< ConfigCheckProbe response (radio module 0x55-family, subtype 0x08; carries responder group/line + status)
  HPT_NETWORK_TEST_START,           ///< Network test start packet (test mode 0x06)
  HPT_SUPERVISION_SERIES,           ///< Daily series of the radio link supervision (family 0x33, type 0x00) - a sign of life of the sender
  HPT_MODULE_MISSING,               ///< A radio module reports another one as missing (family 0x33, type 0x02)
  HPT_SUPERVISION_ROLL_CALL,        ///< Roll call of the radio link supervision (family 0x33, type 0x01)
  HPT_MODULE_PRESENT,               ///< A radio module answers a "module missing" report: it hears the named module (family 0x33, type 0x03)
  HPT_LINE_CLOCK                    ///< Line clock, distributed in a supervision round (family 0x22, type 0x00)
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
  uint32_t missing_module_sn; ///< HPT_MODULE_MISSING: serial of the radio module that is reported as missing
  uint32_t present_module_sn; ///< HPT_MODULE_PRESENT: serial of the radio module that the sender hears
} genius_packet_t;

/**
 * @brief Classify a received Genius frame and extract its fields.
 *
 * The packet type is determined from the on-air message-family byte (offset 26) and the
 * message-type byte (offset 27), with the per-type length as a validator - mirroring how
 * the genuine radio module routes a frame. Frames of a family or type that is not known
 * (e.g. the telegrams of family 0x11) stay HPT_UNKNOWN instead of being taken for
 * a packet of another family that happens to share type byte and length.
 * A status change is classified by family, type and length only; which flags it sets or
 * clears is left to the consumer, so new flags need no change here.
 *
 * @param data    Raw frame bytes (as received, starting with the 0x02 lead byte)
 * @param length  Frame length in bytes
 * @param out     Receives the classification; type is HPT_UNKNOWN for unrecognized frames
 * @return false if a pointer argument is null, true otherwise
 */
bool genius_analyze_packet(const uint8_t *data, size_t length, genius_packet_t *out);
