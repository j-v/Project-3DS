/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

/*
    The note position tests.

    A note moves by the amount of time that has passed, not by a fixed amount
    per frame, so that it reaches its hole exactly when it has to be hit. A
    fixed per-frame move would make its speed depend on the frame rate.

    The comparison against the recorded positions lives in
    test_differential_fixtures.cpp, where they are replayed.
*/

#include "test_framework.h"

#include <vector>

#include "core/chart.h"
#include "core/engine.h"
#include "ds_field.h"
#include "scenario.h"

using namespace p3ds;

namespace {

/** The flying time the test uses, in engine time units. */
constexpr uint32_t FlyTime = 41800; // 25 frames

} // namespace

TEST(note_reaches_its_hole_at_its_hit_time)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(FlyTime / 100);
    builder.time(0);
    builder.target(0, 240000, 135000, 45000, 100000);
    builder.end();

    std::vector<uint8_t> bytes = builder.bytes();
    Chart chart = ChartLoader::parse(bytes.data(), bytes.size());

    GameEngine engine;
    engine.setChart(chart);
    engine.setField(testing::dsFieldConfig());

    uint32_t hitTime = 0;

    // The note is created on the first frame
    engine.beginFrame(0, 0);
    CHECK_EQ(engine.activeNotes().size(), 1u);

    const Note &note = engine.activeNotes().front();
    hitTime = note.time;

    // Halfway through its flight the note is still on its way
    engine.beginFrame(hitTime / 2, hitTime / 2);
    CHECK(engine.activeNotes().front().ofsX != note.startOfsX);
    CHECK(engine.activeNotes().front().ofsY != note.startOfsY);

    // On the frame it has to be hit, the note is exactly at its hole
    engine.beginFrame(hitTime, hitTime / 2);
    CHECK_EQ(engine.activeNotes().front().ofsX, 0);
    CHECK_EQ(engine.activeNotes().front().ofsY, 0);
}

TEST(note_motion_does_not_depend_on_the_frame_rate)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(FlyTime / 100);
    builder.time(0);
    builder.target(0, 240000, 135000, 45000, 100000);
    builder.end();

    std::vector<uint8_t> bytes = builder.bytes();
    Chart chart = ChartLoader::parse(bytes.data(), bytes.size());

    // The same moment in time, reached with a different number of frames
    GameEngine fast;
    fast.setChart(chart);
    fast.setField(testing::dsFieldConfig());

    GameEngine slow;
    slow.setChart(chart);
    slow.setField(testing::dsFieldConfig());

    for (uint32_t now = 0; now <= FlyTime; now += 100)
        fast.beginFrame(now, 100);

    for (uint32_t now = 0; now + 1000 <= FlyTime; now += 1000)
        slow.beginFrame(now, 1000);

    // Both engines are brought to the same moment in time
    fast.beginFrame(FlyTime, 100);
    slow.beginFrame(FlyTime, 1000);

    CHECK_EQ(fast.activeNotes().front().ofsX, slow.activeNotes().front().ofsX);
    CHECK_EQ(fast.activeNotes().front().ofsY, slow.activeNotes().front().ofsY);
}

TEST(note_flies_in_from_just_outside_its_hole)
{
    scenario::ChartBuilder builder;
    builder.targetFlyingTime(FlyTime / 100);
    builder.time(0);
    builder.target(0, 240000, 135000, 45000, 100000);
    builder.end();

    std::vector<uint8_t> bytes = builder.bytes();
    Chart chart = ChartLoader::parse(bytes.data(), bytes.size());

    GameEngine engine;
    engine.setChart(chart);
    engine.setField(FieldConfig());

    engine.beginFrame(0, 0);
    CHECK_EQ(engine.activeNotes().size(), 1u);

    const Note &note = engine.activeNotes().front();
    FieldConfig field;

    // The note starts its flight outside its hole, but within half a screen
    // of it. A travel of thousands of pixels would keep the note invisible
    // until the last moment before it has to be hit.
    int startX = note.startOfsX >> 8;
    int startY = note.startOfsY >> 8;
    CHECK(startX > -field.width / 2 && startX < field.width / 2);
    CHECK(startY > -field.height / 2 && startY < field.height / 2);
}
