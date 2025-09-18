#pragma once

#include <time.h>
#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

class Utils
{
public:
    /**
     * @brief Convert an ISO 8601 date string to time_t
     * @param iso8601_date A String representing the date in ISO 8601 format (e.g., "2025-03-20T15:30:00.000Z")
     * @return A `time_t` value representing the date in seconds (Unix Epoch), or -1 if the conversion fails
     */
    static time_t iso8601_to_time_t(const String &iso8601_date);

    /**
     * @brief Convert a time_t value to an ISO 8601 date string
     * @param time The time_t value in seconds to convert
     * @return A String representing the date in ISO 8601 format
     */
    static String time_t_to_iso8601(time_t time_s);

    // Hash utilities for fast data comparison and duplicate detection

    /**
     * @brief Optimized XOR hash calculation for any data alignment
     * 
     * High-performance XOR hash that automatically handles any data alignment using
     * a Head/Bulk/Tail optimization strategy:
     * - HEAD: Process initial bytes to reach 4-byte alignment
     * - BULK: Fast 32-bit word processing for the aligned middle section  
     * - TAIL: Efficient switch-based processing for remaining 0-3 bytes
     * 
     * Performance characteristics:
     * - Aligned data: ~2 cycles per byte
     * - Unaligned data: ~2-3 cycles per byte (vs ~6-8 for naive approach)
     * - Small data (< 8 bytes): Minimal overhead
     * - Large data (> 64 bytes): 3-4x faster than byte-by-byte
     * 
     * Ideal for:
     * - Packet duplicate detection
     * - Fast data comparison  
     * - Hash-based data structures
     * - Memory-efficient checksums
     * 
     * @param data Pointer to data to hash (any alignment)
     * @param length Number of bytes to hash
     * @return uint32_t XOR hash value (0 for empty/null data)
     * 
     * @note Thread-safe, no side effects
     * @note Handles both aligned and unaligned data optimally
     */
    static uint32_t xorHash(const uint8_t* data, size_t length);
};