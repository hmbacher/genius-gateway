/**
 * @file GeniusLineCode.h
 * @brief Line code: the line a radio module is set to with its rotary switches (Hekatron: "Linie", e.g. A.0)
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

/* Line code: the position of a radio module's rotary switches, letter << 4 | number (A.0 = 0x00,
 * A.3 = 0x03, H.7 = 0x77; an FM Basis X has no switches and is fixed to A.0). Hekatron calls it the
 * "Linie" of a module. Every frame carries it three times. A radio module accepts a frame only if the
 * addressed line code is its own (or GENIUS_LINE_CODE_ANY) and the Line-ID matches as well, and it
 * forwards a frame only if the line code of the originator is its own - see
 * docs/reverse-engineering/protocol-analysis.md, "Line Addressing". The gateway itself receives every
 * frame, whatever its line code. */
#define GENIUS_LINE_CODE_A0 0x00  ///< Line code of the rotary position A.0 (the only one an FM Basis X has)
#define GENIUS_LINE_CODE_ANY 0xFF ///< Addressed line code that every radio module accepts

/**
 * @brief True for a line a radio module can be set to: A.0 - G.9, and the collective alarm lines
 *        H.0, H.1, H.2, H.4, H.5, H.7 (the line overview of the manufacturer's manual).
 */
static inline bool genius_line_code_valid(uint8_t code)
{
    const uint8_t letter = code >> 4;
    const uint8_t number = code & 0x0F;
    if (number > 9)
        return false;
    if (letter <= 6)
        return true;
    if (letter == 7)
        return number == 0 || number == 1 || number == 2 || number == 4 || number == 5 || number == 7;
    return false;
}

/// Letter of a line code ('A' - 'H')
static inline char genius_line_code_letter(uint8_t code) { return (char)('A' + (code >> 4)); }

/// Number of a line code (0 - 9)
static inline uint8_t genius_line_code_number(uint8_t code) { return code & 0x0F; }

/// Line code of a letter ('A' - 'H') and a number (0 - 9); GENIUS_LINE_CODE_ANY if that is no line
static inline uint8_t genius_line_code_of(char letter, uint8_t number)
{
    if (letter < 'A' || letter > 'H' || number > 9)
        return GENIUS_LINE_CODE_ANY;
    const uint8_t code = (uint8_t)(((letter - 'A') << 4) | number);
    return genius_line_code_valid(code) ? code : GENIUS_LINE_CODE_ANY;
}
