/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include "differential_cases.h"

#include <cstddef>
#include <utility>

#include "scenario.h"

namespace diffcases {
namespace {

/**
 * Builds a chart of notes that are one flying time apart.
 *
 * The note types are taken from the given list, and the notes are placed at
 * the given chart coordinates.
 */
std::vector<uint8_t> buildChart(const std::vector<uint8_t> &types, int32_t x = 240000, int32_t y = 135000)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(FlyTime / 100);

    for (size_t i = 0; i < types.size(); i++)
    {
        builder.time((uint32_t)(i * FlyTime));
        builder.target(types[i], x, y, 0, 100000);
    }

    builder.end();
    return builder.bytes();
}

Case makeCase(const char *name, std::vector<uint8_t> chart, std::vector<uint32_t> trace)
{
    Case c;
    c.name = name;
    c.difficulty = 1;
    c.chart = std::move(chart);
    c.trace = std::move(trace);
    return c;
}

std::vector<Case> buildCases()
{
    std::vector<Case> cases;

    // A perfect play: every note is hit on the frame it has to be
    {
        std::vector<uint8_t> types;
        scenario::TraceBuilder trace;

        for (int i = 0; i < 60; i++)
        {
            types.push_back((uint8_t)(i % 4));
            trace.idle((i + 1) * 25);
            trace.press(1u << (i % 4));
        }

        cases.push_back(makeCase("perfect_play", buildChart(types), trace.build()));
    }

    // Every judgement window: cool is three frames, fine six, sad nine, and a
    // note is missed after twelve
    {
        static const int offsets[] = { -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 9, 10 };
        std::vector<uint8_t> types;
        scenario::TraceBuilder trace;

        for (size_t i = 0; i < sizeof(offsets) / sizeof(offsets[0]); i++)
        {
            types.push_back((uint8_t)(i % 4));
            trace.idle((int)((i + 1) * FlyTime / scenario::FrameTime));
            trace.press(1u << (i % 4));
        }

        cases.push_back(makeCase("judgement_offsets", buildChart(types), trace.build()));
    }

    // Missed notes: the life runs out after seven misses
    {
        std::vector<uint8_t> types;
        scenario::TraceBuilder trace;

        for (int i = 0; i < 30; i++)
        {
            types.push_back((uint8_t)(i % 4));
            trace.idle((i + 1) * 25);
        }

        cases.push_back(makeCase("misses", buildChart(types), trace.build()));
    }

    // Multi-notes: two notes share a hit time
    {
        scenario::ChartBuilder builder;
        scenario::TraceBuilder trace;

        builder.targetFlyingTime(FlyTime / 100);

        for (int i = 0; i < 20; i++)
        {
            builder.time((uint32_t)(i * FlyTime));
            builder.target((uint8_t)(i % 4), 240000, 135000, 0, 100000);
            builder.target((uint8_t)((i + 1) % 4), 240000, 135000, 0, 100000);
            trace.idle((i + 1) * 25);
            trace.press((1u << (i % 4)) | (1u << ((i + 1) % 4)));
        }

        builder.end();
        cases.push_back(makeCase("multi_notes", builder.bytes(), trace.build()));
    }

    // Held notes: the button is held past the note and then released
    {
        scenario::ChartBuilder builder;
        scenario::TraceBuilder trace;

        builder.targetFlyingTime(FlyTime / 100);

        for (int i = 0; i < 10; i++)
        {
            builder.time((uint32_t)(i * FlyTime));
            builder.target((uint8_t)(4 + (i % 4)), 240000, 135000, 0, 100000);
            trace.idle((i + 1) * 25);
            trace.press(1u << (i % 4));
            trace.hold(1u << (i % 4), (i + 1) * 25 + 30);
            trace.idle((i + 1) * 25 + 31);
        }

        builder.end();
        cases.push_back(makeCase("held_notes", builder.bytes(), trace.build()));
    }

    // Slides: a single slide followed by a chain of held slides
    {
        scenario::ChartBuilder builder;
        scenario::TraceBuilder trace;

        builder.targetFlyingTime(FlyTime / 100);

        for (int i = 0; i < 10; i++)
        {
            builder.time((uint32_t)(i * FlyTime));
            builder.target((uint8_t)(i % 2 ? 13 : 12), 240000, 135000, 0, 100000);
            builder.target((uint8_t)(i % 2 ? 16 : 15), 240000, 135000, 0, 100000);
            builder.target((uint8_t)(i % 2 ? 16 : 15), i % 2 ? 280000 : 200000, 135000, 0, 100000);

            trace.idle((i + 1) * 25);

            // The last group is left alone on purpose: a wrong button after
            // the final group is cleared would read past the end of the note
            // queue
            if (i < 9)
            {
                trace.press((1u << 4) | (1u << 5));
                trace.hold((1u << 4) | (1u << 5), (i + 1) * 25 + 20);
            }
        }

        builder.end();
        cases.push_back(makeCase("slides", builder.bytes(), trace.build()));
    }

    // Wrong buttons: every third note is hit with the wrong button
    {
        std::vector<uint8_t> types;
        scenario::TraceBuilder trace;

        for (int i = 0; i < 20; i++)
        {
            types.push_back((uint8_t)(i % 4));
            trace.idle((i + 1) * 25);
            trace.press(1u << ((i % 4 + 1) % 4));
        }

        cases.push_back(makeCase("wrong_keys", buildChart(types), trace.build()));
    }

    // A button that is released and pressed again is a new press
    {
        scenario::ChartBuilder builder;
        scenario::TraceBuilder trace;

        builder.targetFlyingTime(FlyTime / 100);

        for (int i = 0; i < 12; i++)
        {
            builder.time((uint32_t)(i * FlyTime));
            builder.target((uint8_t)(i % 4), 240000, 135000, 0, 100000);
            trace.idle((i + 1) * 25);
            trace.press(1u << (i % 4));
            trace.idle((i + 1) * 25 + 3);
        }

        builder.end();
        cases.push_back(makeCase("release_and_repress", builder.bytes(), trace.build()));
    }

    return cases;
}

} // namespace

const std::vector<Case> &all()
{
    static const std::vector<Case> cases = buildCases();
    return cases;
}

const PositionCase &position()
{
    static const PositionCase positionCase = []()
    {
        scenario::ChartBuilder builder;
        builder.targetFlyingTime(FlyTime / 100);
        builder.time(0);
        builder.target(0, 240000, 135000, 0, 100000);
        builder.end();

        PositionCase c;
        c.chart = builder.bytes();
        c.trace.assign(120, 0);
        return c;
    }();

    return positionCase;
}

} // namespace diffcases
