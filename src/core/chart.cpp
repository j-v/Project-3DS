/*
    Copyright 2022-2024 Hydr8gon
    Copyright 2026 Project 3DS contributors
    Modified 2026 by Project 3DS contributors.

    This file is part of Project 3DS, a derivative work of Project DS
    <https://github.com/Hydr8gon/Project-DS>.

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include "chart.h"

namespace p3ds {

/**
 * The number of parameters of every opcode.
 *
 * Opcodes the parser does not consume are skipped with these counts, so a
 * chart is walked exactly as it was written. The table covers 0x00-0xFF; the
 * entries above 0x6A have zero parameters.
 */
static const uint8_t paramCounts[0x100] =
{
    0,  1,  4,  2,  2,  2,  7,  4,  2,  6,  2,  1,  6,  2,  1,  1, // 0x00-0x0F
    3,  2,  3,  5,  5,  4,  4,  5,  2,  0,  2,  4,  2,  2,  1, 21, // 0x10-0x1F
    0,  3,  2,  5,  1,  1,  7,  1,  1,  2,  1,  2,  1,  2,  3,  3, // 0x20-0x2F
    1,  2,  2,  3,  6,  6,  1,  1,  2,  3,  1,  2,  2,  4,  4,  1, // 0x30-0x3F
    2,  1,  2,  1,  1,  3,  3,  3,  2,  1,  9,  3,  2,  4,  2,  3, // 0x40-0x4F
    2, 24,  1,  2,  1,  3,  1,  3,  4,  1,  2,  6,  3,  2,  3,  3, // 0x50-0x5F
    4,  1,  1,  3,  3,  4,  2,  3,  3,  8,  2                      // 0x60-0x6A
};

/**
 * Chart coordinates at 256 x 192, used to decide whether a held slide
 * continues the previous one.
 */
static inline int32_t dsScreenX(int32_t chartX)
{
    return chartX * 256 / 480000 - 16;
}

static inline int32_t dsScreenY(int32_t chartY)
{
    return chartY * 192 / 270000 - 16;
}

Chart ChartLoader::parse(const uint8_t *data, size_t size)
{
    Chart chart;

    if (!data || size < 4)
        return chart;

    // The words are read one byte at a time so that the parser does not
    // depend on the alignment or the endianness of the host.
    size_t wordCount = size / 4;
    std::vector<uint32_t> words(wordCount);

    for (size_t i = 0; i < wordCount; i++)
    {
        words[i] = (uint32_t)data[i * 4] | ((uint32_t)data[i * 4 + 1] << 8) |
                   ((uint32_t)data[i * 4 + 2] << 16) | ((uint32_t)data[i * 4 + 3] << 24);
    }

    uint32_t currentTime = 0;
    uint32_t group = 0;
    bool truncated = false;
    bool havePrev = false;
    uint8_t prevType = 0;
    int32_t prevX = 0;
    int32_t prevY = 0;

    // The first word of a chart is a header, so the opcodes start at the second
    size_t count = 1;

    while (count < words.size())
    {
        uint32_t opcode = words[count];

        // The table covers 0x00-0xFF. Opcodes above 0x6A have no parameters,
        // so they are skipped rather than ending the parse. A word above
        // 0xFF is not a chart opcode.
        if (opcode >= 0x100)
            break;

        uint32_t params = paramCounts[opcode];

        if (count + params >= words.size())
        {
            truncated = true;
            break;
        }

        switch (opcode)
        {
            case OPCODE_END:
            {
                chart.ended = true;
                chart.endTime = currentTime;
                count = words.size();
                break;
            }

            case OPCODE_TIME:
            {
                currentTime = words[count + 1];
                group++;
                break;
            }

            case OPCODE_TARGET:
            {
                uint8_t type = (uint8_t)words[count + 1];
                int32_t chartX = (int32_t)words[count + 2];
                int32_t chartY = (int32_t)words[count + 3];
                uint8_t noteType;

                if (type < 4) // Normal buttons
                {
                    noteType = type;
                }
                else if (type < 8) // Held buttons
                {
                    // 4..7 are buttons 0..3 held down
                    noteType = (type & 3) | NOTE_HELD;
                }
                else if (type == 12 || type == 13) // Single slides
                {
                    // 12..13 are the left/right slides, which share the
                    // button values 4/5
                    noteType = (type - 8) | NOTE_SLIDE;
                }
                else if (type == 15 || type == 16) // Held slides
                {
                    // A held slide continues the previous one when it is the
                    // same side and sits further in that direction: left
                    // slides move left, right slides move right
                    uint8_t base = (type == 15 ? 4 : 5) | NOTE_SLIDE_CHAIN;
                    int32_t screenX = dsScreenX(chartX);
                    bool continues = (type == 15) ? (screenX < prevX) : (screenX > prevX);

                    // Drop the continuation bit from the previous note, so a
                    // chain of any length compares against the same base
                    if (havePrev && base == (prevType & ~NOTE_SLIDE_CONT) &&
                        continues && dsScreenY(chartY) == prevY)
                    {
                        // Same side, same height, further that way: this note
                        // is a continuation of the held slide
                        base |= NOTE_SLIDE_CONT;
                    }

                    noteType = base;
                }
                else if (type >= 18 && type < 22) // Event notes
                {
                    noteType = type - 18;
                }
                else
                {
                    // ignore these notes entirely
                    break;
                }

                ChartEvent event;
                event.type = ChartEventType::Target;
                event.time = currentTime;
                event.target.chartType = type;
                event.target.noteType = noteType;
                event.target.x = chartX;
                event.target.y = chartY;
                event.target.angle = (int32_t)words[count + 4];
                event.target.distance = words[count + 5];
                event.target.group = group;
                chart.events.push_back(event);

                prevType = noteType;
                prevX = dsScreenX(chartX);
                prevY = dsScreenY(chartY);
                havePrev = true;
                break;
            }

            case OPCODE_LYRIC:
            {
                ChartEvent event;
                event.type = ChartEventType::Lyric;
                event.time = currentTime;
                event.lyricIndex = words[count + 1];
                chart.events.push_back(event);
                break;
            }

            case OPCODE_MUSIC_PLAY:
            {
                ChartEvent event;
                event.type = ChartEventType::MusicPlay;
                event.time = currentTime;
                chart.events.push_back(event);
                break;
            }

            case OPCODE_BAR_TIME_SET:
            {
                ChartEvent event;
                event.type = ChartEventType::BarTimeSet;
                event.time = currentTime;
                event.bpm = words[count + 1];
                event.beatsPerBar = words[count + 2];
                chart.events.push_back(event);
                break;
            }

            case OPCODE_TARGET_FLYING_TIME:
            {
                ChartEvent event;
                event.type = ChartEventType::TargetFlyingTime;
                event.time = currentTime;
                event.flyingTimeMs = words[count + 1];
                chart.events.push_back(event);
                break;
            }
        }

        // Move to the next opcode
        count += params + 1;
    }

    chart.valid = !truncated;
    return chart;
}

} // namespace p3ds
