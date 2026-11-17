/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#ifndef P3DS_SCENARIO_H
#define P3DS_SCENARIO_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace scenario {

/**
 * Builds the .dsc charts the tests use.
 *
 * The builder writes the words the chart parser reads, so a chart can be
 * replayed through the reference engine and this one at the same time.
 */
class ChartBuilder
{
public:
    ChartBuilder();

    /** Waits until the given time. */
    void time(uint32_t units);

    /** Adds a note. */
    void target(uint8_t type, int32_t x, int32_t y, int32_t angle = 0, uint32_t distance = 100000);

    /** Adds a lyric. */
    void lyric(uint32_t index);

    /** Starts the music. */
    void musicPlay();

    /** Sets the flying time from a tempo. */
    void barTimeSet(uint32_t bpm, uint32_t beatsPerBar);

    /** Sets the flying time directly. */
    void targetFlyingTime(uint32_t milliseconds);

    /** Ends the chart. */
    void end();

    /** The contents of the chart file. */
    std::vector<uint8_t> bytes() const;

private:
    void word(uint32_t value);

    std::vector<uint32_t> words;
};

/** The frames a trace of held buttons covers. */
class TraceBuilder
{
public:
    TraceBuilder();

    /** The number of frames in the trace. */
    size_t frames() const { return held.size(); }

    /** Holds a set of buttons for a single frame. */
    void press(uint32_t actions);

    /** Holds a set of buttons until the given frame. */
    void hold(uint32_t actions, int untilFrame);

    /** Holds nothing until the given frame. */
    void idle(int untilFrame);

    /** The trace, one mask per frame. */
    const std::vector<uint32_t> &build();

private:
    std::vector<uint32_t> held;
};

/** The number of engine time units in a frame of the reference engine. */
constexpr uint32_t FrameTime = 1672;

} // namespace scenario

#endif // P3DS_SCENARIO_H
