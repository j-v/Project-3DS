/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include "test_framework.h"

#include "core/clock.h"

using namespace p3ds;

/** The rate the tests use, which is the 3DS hardware rate. */
constexpr uint64_t TestTickRate = 268111856ull;

TEST(clock_converts_a_second_of_ticks)
{
    TickClock clock(TestTickRate);
    clock.reset(0);

    // The tick counter runs at 268,111,856 Hz, so a second of ticks is a
    // second of engine time
    CHECK_EQ(clock.advance(TestTickRate), 100000u);
}

TEST(clock_uses_the_supplied_rate)
{
    // The conversion is not tied to the 3DS: a 1 MHz counter produces the
    // same unit for a second
    TickClock clock(1000000);
    clock.reset(0);

    CHECK_EQ(clock.advance(1000000), 100000u);
}

TEST(clock_converts_known_values)
{
    TickClock clock(TestTickRate);
    clock.reset(0);

    // Half a second
    CHECK_EQ(clock.advance(TestTickRate / 2), 50000u);

    // A tenth of a second is 26811185.6 ticks, so the leftover of the
    // division is kept for the next call
    TickClock other(TestTickRate);
    other.reset(0);
    CHECK_EQ(other.advance(TestTickRate / 10), 9999u);
    CHECK_EQ(other.advance(TestTickRate / 10 * 2), 10000u);
}

TEST(clock_does_not_drift)
{
    // A million frames of one tick each have to add up to exactly the same
    // amount of time as one step of a million ticks
    TickClock stepped(TestTickRate);
    stepped.reset(0);

    uint64_t steppedUnits = 0;

    for (int i = 0; i < 1000000; i++)
        steppedUnits += stepped.advance(i + 1);

    TickClock single(TestTickRate);
    single.reset(0);
    uint32_t singleUnits = single.advance(1000000);

    CHECK_EQ(stepped.currentTicks(), single.currentTicks());
    CHECK_EQ(steppedUnits, singleUnits);

    // 1,000,000 ticks are 3.73 ms, which is 372 units of 10 us
    CHECK_EQ(singleUnits, 372u);
}

TEST(clock_keeps_the_remainder)
{
    TickClock clock(TestTickRate);
    clock.reset(0);

    // A single tick is 0.373 us, which is less than an engine unit, so the
    // first tick does not produce a unit, but the leftovers are kept
    uint32_t total = 0;

    for (uint64_t i = 1; i <= 1000; i++)
        total += clock.advance(i);

    // 1000 ticks are 3.73 us, which is less than one unit of 10 us
    CHECK_EQ(total, 0u);
}

TEST(clock_carries_ticks_across_calls)
{
    // One unit is 2,681.11856 ticks. 2,681 ticks do not complete it, and the
    // next tick does, which proves that the remainder survives between calls.
    TickClock clock(TestTickRate);
    clock.reset(0);

    uint32_t total = 0;

    for (uint64_t i = 1; i <= 2682; i++)
        total += clock.advance(i);

    CHECK_EQ(total, 1u);
}

TEST(clock_ignores_a_backwards_reading)
{
    TickClock clock(TestTickRate);
    clock.reset(1000);
    CHECK_EQ(clock.advance(500), 0u);
    CHECK_EQ(clock.advance(2000), 0u);
    CHECK_EQ(clock.advance(268111856ull + 1000), 100000u);
}

TEST(clock_units_convert_to_milliseconds)
{
    CHECK_NEAR(unitsToMs(100000), 1000.0, 0.001);
    CHECK_NEAR(unitsToMs(5016), 50.16, 0.001);
    CHECK_EQ(msToUnits(1000.0), 100000u);
}
