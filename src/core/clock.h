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

#ifndef P3DS_CLOCK_H
#define P3DS_CLOCK_H

#include <cstdint>

namespace p3ds {

/** The engine's time unit: 10 microseconds. */
constexpr uint32_t UNITS_PER_SECOND = 100000;

/**
 * Converts a monotonic tick counter into the engine's time unit.
 *
 * The rate of the counter is supplied by the platform, so the core does not
 * depend on a particular hardware clock. The conversion carries the
 * remainder, so truncation never accumulates into drift: a million frames of
 * 1 tick each produce exactly the same number of units as one frame of a
 * million ticks.
 */
class TickClock
{
public:
    /** Creates a clock for a counter that ticks at the given rate. */
    explicit TickClock(uint64_t ticksPerSecond);

    /** Restarts the clock at the given tick value. */
    void reset(uint64_t ticks);

    /**
     * Advances the clock to the given tick value and returns the number of
     * 10 microsecond units that have elapsed since the previous call.
     */
    uint32_t advance(uint64_t ticks);

    /** The tick value of the last advance. */
    uint64_t currentTicks() const { return lastTicks; }

private:
    uint64_t tickRate;
    uint64_t lastTicks = 0;
    uint64_t accumulator = 0; // leftover numerator of the last division
};

/** Converts engine time units to milliseconds. */
inline double unitsToMs(uint32_t units)
{
    return (double)units * 1000.0 / UNITS_PER_SECOND;
}

/** Converts milliseconds to engine time units. */
inline uint32_t msToUnits(double ms)
{
    return (uint32_t)(ms * UNITS_PER_SECOND / 1000.0 + 0.5);
}

} // namespace p3ds

#endif // P3DS_CLOCK_H
