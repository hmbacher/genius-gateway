#pragma once

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2025 theelims
 *   Copyright (C) 2026 hmbacher
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <Arduino.h>

/**
 * @brief Small helpers shared by the Home Assistant integration classes
 */
namespace HAUtils
{
    /**
     * @brief Build a lowercase ASCII slug ([a-z0-9] plus the separator) from free text
     *
     * Runs of separating characters collapse into a single separator and the
     * result never starts or ends with one. The function is idempotent: a slug
     * is returned unchanged.
     *
     * Two flavours share this one implementation:
     *
     * - `transliterate = true` (entity IDs): mirrors what Home Assistant does
     *   when it generates an entity ID itself. Latin letters with diacritics
     *   are reduced to their base letter ("Küche" -> "kuche", "ß" -> "ss",
     *   "é" -> "e"; Latin-1 Supplement and Latin Extended-A are covered) and
     *   every other character outside [A-Za-z0-9] acts as a separator.
     *
     * - `transliterate = false` (topic / node IDs): only space, '_' and '-'
     *   act as separators; every other character outside [A-Za-z0-9] is
     *   dropped without a trace ("My.Device" -> "mydevice"). This is the
     *   behaviour device IDs and MQTT topics have always been built with, so
     *   it must stay byte-identical - changing it would move existing topics.
     *
     * @param text          Input text (UTF-8)
     * @param separator     Character placed between the words of the slug
     * @param transliterate true for entity-ID slugs, false for topic slugs
     * @return The slug; empty if the text contains no usable character
     */
    String slugify(const String &text, char separator = '_', bool transliterate = true);
}
