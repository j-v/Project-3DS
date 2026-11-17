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

#include "clock.h"

namespace p3ds {

TickClock::TickClock(uint64_t ticksPerSecond)
    : tickRate(ticksPerSecond)
{
}

void TickClock::reset(uint64_t ticks)
{
    lastTicks = ticks;
    accumulator = 0;
}

uint32_t TickClock::advance(uint64_t ticks)
{
    // A zero rate is a programming error, and a backwards reading (which the
    // hardware cannot produce) is treated as no time passing at all.
    if (tickRate == 0 || ticks <= lastTicks)
        return 0;

    uint64_t delta = ticks - lastTicks;
    lastTicks = ticks;

    // The numerator is kept so that the division's remainder survives to the
    // next call, which is what keeps the conversion free of drift.
    accumulator += delta * UNITS_PER_SECOND;
    uint64_t units = accumulator / tickRate;
    accumulator %= tickRate;

    return (uint32_t)units;
}

} // namespace p3ds
