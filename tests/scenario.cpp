/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include "scenario.h"

namespace scenario {

ChartBuilder::ChartBuilder()
{
    // The first word of a chart is a header
    word(0);
}

void ChartBuilder::word(uint32_t value)
{
    words.push_back(value);
}

void ChartBuilder::time(uint32_t units)
{
    word(0x01);
    word(units);
}

void ChartBuilder::target(uint8_t type, int32_t x, int32_t y, int32_t angle, uint32_t distance)
{
    word(0x06);
    word(type);
    word((uint32_t)x);
    word((uint32_t)y);
    word((uint32_t)angle);
    word(distance);
    word(0);
    word(0);
}

void ChartBuilder::lyric(uint32_t index)
{
    word(0x18);
    word(index);
    word(0xFFFFFFFF);
}

void ChartBuilder::musicPlay()
{
    word(0x19);
}

void ChartBuilder::barTimeSet(uint32_t bpm, uint32_t beatsPerBar)
{
    word(0x1C);
    word(bpm);
    word(beatsPerBar);
}

void ChartBuilder::targetFlyingTime(uint32_t milliseconds)
{
    word(0x3A);
    word(milliseconds);
}

void ChartBuilder::end()
{
    word(0x00);
}

std::vector<uint8_t> ChartBuilder::bytes() const
{
    std::vector<uint8_t> bytes;
    bytes.reserve(words.size() * 4);

    for (size_t i = 0; i < words.size(); i++)
    {
        bytes.push_back((uint8_t)(words[i] & 0xFF));
        bytes.push_back((uint8_t)((words[i] >> 8) & 0xFF));
        bytes.push_back((uint8_t)((words[i] >> 16) & 0xFF));
        bytes.push_back((uint8_t)((words[i] >> 24) & 0xFF));
    }

    return bytes;
}

TraceBuilder::TraceBuilder()
{
    held.push_back(0);
}

void TraceBuilder::press(uint32_t actions)
{
    held.push_back(actions);
}

void TraceBuilder::hold(uint32_t actions, int untilFrame)
{
    while ((int)held.size() <= untilFrame)
        held.push_back(actions);
}

void TraceBuilder::idle(int untilFrame)
{
    hold(0, untilFrame);
}

const std::vector<uint32_t> &TraceBuilder::build()
{
    return held;
}

} // namespace scenario
