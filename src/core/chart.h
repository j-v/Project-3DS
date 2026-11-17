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

#ifndef P3DS_CHART_H
#define P3DS_CHART_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace p3ds {

/**
 * The note type flags.
 *
 * The bits of a note type are:
 *
 *   bit 7 6 5 4 3 2 1 0
 *       C K S H b b b b
 *
 *   b = button (0-3 for normal notes, 4-5 for left/right slides)
 *   H = held button
 *   S = single slide
 *   K = held-slide chain
 *   C = continuation of a held slide
 *
 * NOTE_TYPE_MASK extracts the button bits. The parser builds these flags from
 * the raw chart type, and the engine uses them to decide what a note is.
 */
enum : uint8_t
{
    NOTE_TYPE_MASK = 0x0F,     // the button a note is hit with
    NOTE_HELD = 1 << 4,        // BIT(4): a held button
    NOTE_SLIDE = 1 << 5,       // BIT(5): a single slide
    NOTE_SLIDE_CHAIN = 1 << 6, // BIT(6): a held slide
    NOTE_SLIDE_CONT = 1 << 7   // BIT(7): the continuation of a held slide
};

/** The opcodes the parser consumes. */
enum : uint32_t
{
    OPCODE_END = 0x00,
    OPCODE_TIME = 0x01,
    OPCODE_TARGET = 0x06,
    OPCODE_LYRIC = 0x18,
    OPCODE_MUSIC_PLAY = 0x19,
    OPCODE_BAR_TIME_SET = 0x1C,
    OPCODE_TARGET_FLYING_TIME = 0x3A
};

enum class ChartEventType
{
    Time,
    Target,
    Lyric,
    MusicPlay,
    BarTimeSet,
    TargetFlyingTime
};

/** A target (note) from the chart. */
struct ChartTarget
{
    uint8_t chartType = 0; // the raw note type from the chart
    uint8_t noteType = 0;  // the classified type, using the flags above
    int32_t x = 0;         // chart coordinates
    int32_t y = 0;
    int32_t angle = 0;    // -180000 to 180000, 0 at the top, clockwise
    uint32_t distance = 0; // how far the note travels while it flies in
    uint32_t group = 0;    // how many TIME opcodes came before this target
};

/** One event of a chart. */
struct ChartEvent
{
    ChartEventType type = ChartEventType::Time;
    uint32_t time = 0; // the absolute chart time, in 10 microsecond units
    ChartTarget target;
    uint32_t lyricIndex = 0;
    uint32_t bpm = 0;
    uint32_t beatsPerBar = 0;
    uint32_t flyingTimeMs = 0;
};

/** A parsed chart. */
struct Chart
{
    std::vector<ChartEvent> events;
    uint32_t endTime = 0; // when the chart's end opcode is reached
    bool ended = false;
    bool valid = false;
};

/**
 * Parses .dsc scripts into chart events.
 *
 * The parser consumes these opcodes and skips the rest using their parameter
 * counts, so a chart with extra or unknown opcodes still loads.
 */
class ChartLoader
{
public:
    /** Parses the contents of a .dsc file. */
    static Chart parse(const uint8_t *data, size_t size);
};

} // namespace p3ds

#endif // P3DS_CHART_H
