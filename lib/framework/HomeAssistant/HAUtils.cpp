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

#include <HomeAssistant/HAUtils.h>

namespace
{
    // Base letters for U+00C0..U+00FF (UTF-8 lead byte 0xC3), indexed by the
    // continuation byte minus 0x80. ' ' marks a character without a letter
    // equivalent (multiplication / division sign), '*' one that expands to two
    // letters and is handled before the table lookup.
    const char LATIN1_SUPPLEMENT[] =
        "aaaaaa*ceeeeiiii" // U+00C0..U+00CF
        "dnooooo ouuuuy**" // U+00D0..U+00DF
        "aaaaaa*ceeeeiiii" // U+00E0..U+00EF
        "dnooooo ouuuuy*y"; // U+00F0..U+00FF

    // Base letters for U+0100..U+017F (UTF-8 lead bytes 0xC4 and 0xC5), same
    // conventions as above.
    const char LATIN_EXTENDED_A[] =
        "aaaaaaccccccccdd" // U+0100..U+010F
        "ddeeeeeeeeeegggg" // U+0110..U+011F
        "gggghhhhiiiiiiii" // U+0120..U+012F
        "ii**jjkkklllllll" // U+0130..U+013F
        "lllnnnnnnn**oooo" // U+0140..U+014F
        "oo**rrrrrrssssss" // U+0150..U+015F
        "ssttttttuuuuuuuu" // U+0160..U+016F
        "uuuuwwyyyzzzzzzs"; // U+0170..U+017F

    /**
     * @brief Reduce a two-byte UTF-8 Latin letter to its lowercase ASCII equivalent
     *
     * @param lead    First byte of the sequence
     * @param cont    Second (continuation) byte of the sequence
     * @param letters Receives one or two letters plus the terminator
     * @return false if the character has no ASCII letter equivalent
     */
    bool latinToAscii(uint8_t lead, uint8_t cont, char letters[3])
    {
        if (cont < 0x80 || cont > 0xBF)
            return false;

        const uint8_t index = cont - 0x80;
        const char *expansion = nullptr;
        char letter = ' ';

        switch (lead)
        {
        case 0xC3:
            if (index == 0x06 || index == 0x26) // U+00C6, U+00E6 (ae ligature)
                expansion = "ae";
            else if (index == 0x1E || index == 0x3E) // U+00DE, U+00FE (thorn)
                expansion = "th";
            else if (index == 0x1F) // U+00DF (sharp s)
                expansion = "ss";
            else
                letter = LATIN1_SUPPLEMENT[index];
            break;
        case 0xC4:
            if (index == 0x32 || index == 0x33) // U+0132, U+0133 (ij ligature)
                expansion = "ij";
            else
                letter = LATIN_EXTENDED_A[index];
            break;
        case 0xC5:
            if (index == 0x0A || index == 0x0B) // U+014A, U+014B (eng)
                expansion = "ng";
            else if (index == 0x12 || index == 0x13) // U+0152, U+0153 (oe ligature)
                expansion = "oe";
            else
                letter = LATIN_EXTENDED_A[64 + index];
            break;
        default:
            return false;
        }

        if (expansion != nullptr)
        {
            letters[0] = expansion[0];
            letters[1] = expansion[1];
            letters[2] = '\0';
            return true;
        }

        if (letter == ' ')
            return false;

        letters[0] = letter;
        letters[1] = '\0';
        return true;
    }
}

String HAUtils::slugify(const String &text, char separator, bool transliterate)
{
    const size_t length = text.length();

    String slug;
    slug.reserve(length);

    // A separator is only written once the next letter follows, which drops
    // leading and trailing separators and collapses runs into one.
    bool separatorPending = false;

    auto append = [&](const char *letters)
    {
        if (separatorPending && !slug.isEmpty())
            slug += separator;
        separatorPending = false;
        slug += letters;
    };

    for (size_t i = 0; i < length; i++)
    {
        const uint8_t c = static_cast<uint8_t>(text[i]);

        if (c < 0x80)
        {
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
            {
                const char letters[2] = {static_cast<char>(c), '\0'};
                append(letters);
            }
            else if (c >= 'A' && c <= 'Z')
            {
                const char letters[2] = {static_cast<char>(c - 'A' + 'a'), '\0'};
                append(letters);
            }
            else if (transliterate || c == ' ' || c == '_' || c == '-')
            {
                separatorPending = true;
            }
            // Topic slugs drop every other ASCII character without a trace.
            continue;
        }

        // Topic slugs drop every non-ASCII byte without a trace.
        if (!transliterate)
            continue;

        // Entity-ID slugs: consume the complete UTF-8 sequence.
        size_t end = i + 1;
        while (end < length && (static_cast<uint8_t>(text[end]) & 0xC0) == 0x80)
            end++;

        const size_t sequenceLength = end - i;
        const uint8_t cont = (sequenceLength >= 2) ? static_cast<uint8_t>(text[i + 1]) : 0;
        i = end - 1;

        // Combining diacritical marks (U+0300..U+036F) follow their base letter
        // in decomposed text and simply vanish.
        if (sequenceLength == 2 && (c == 0xCC || (c == 0xCD && cont <= 0xAF)))
            continue;

        char letters[3];
        if (sequenceLength == 2 && latinToAscii(c, cont, letters))
            append(letters);
        else
            separatorPending = true;
    }

    return slug;
}
