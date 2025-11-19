/**
 * @file Utils.cpp
 * @brief Implementation of utility functions for time conversion and data hashing
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

#include <Utils.hpp>

time_t Utils::iso8601_to_time_t(const String &iso8601_date)
{
    struct tm tm = {0};
    int millis = 0; // Ignore milliseconds
    if (sscanf(iso8601_date.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d.%3dZ",
               &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &millis) != 7)
    {
        return -1;
    }
    tm.tm_year -= 1900; // Adjust year
    tm.tm_mon -= 1;     // Adjust month (0-based)
    return mktime(&tm);
}

String Utils::time_t_to_iso8601(time_t time_s)
{
    struct tm *tm = gmtime(&time_s);
    // Buffer size for ISO8601 format: "YYYY-MM-DDTHH:MM:SS.000Z" = 24 chars + null terminator
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.000Z", tm);
    return String(buf);
}

uint32_t Utils::xorHash(const uint8_t *data, size_t length)
{
    if (!data || length == 0)
    {
        return 0;
    }

    uint32_t hash = 0;
    const uint8_t *current = data;
    size_t remaining = length;

    // HEAD: Process initial unaligned bytes to reach 4-byte boundary
    uintptr_t alignment = (uintptr_t)current & 3;
    if (alignment != 0 && remaining > 0)
    {
        size_t bytesToAlign = 4 - alignment;
        size_t alignBytes = (bytesToAlign < remaining) ? bytesToAlign : remaining;

        for (size_t i = 0; i < alignBytes; i++)
        {
            hash ^= current[i] << ((i & 3) * 8);
        }

        current += alignBytes;
        remaining -= alignBytes;
    }

    // BULK: Process aligned data using fast 32-bit word operations
    if (remaining >= 4)
    {
        const uint32_t *words = (const uint32_t *)current;
        size_t wordCount = remaining / 4;

        // Process full 32-bit words
        for (size_t i = 0; i < wordCount; i++)
        {
            hash ^= words[i];
        }

        current += wordCount * 4;
        remaining -= wordCount * 4;
    }

    // TAIL: Process remaining bytes (0-3 bytes)
    if (remaining > 0)
    {
        uint32_t tailWord = 0;
        switch (remaining)
        {
        case 3:
            tailWord |= current[2] << 16;
            [[fallthrough]];
        case 2:
            tailWord |= current[1] << 8;
            [[fallthrough]];
        case 1:
            tailWord |= current[0];
            break;
        }
        hash ^= tailWord;
    }

    return hash;
}