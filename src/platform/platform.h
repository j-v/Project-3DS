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

#ifndef P3DS_PLATFORM_H
#define P3DS_PLATFORM_H

#include <cstdint>

namespace p3ds {

/** What the clock self test measured. */
struct ClockReport
{
    double tickRate = 0;         // the tick rate measured against the RTC
    double displayRate = 0;      // the display rate measured against the RTC
    double displayPeriod = 0;    // the measured length of a frame, in ticks
    uint32_t frames = 0;
    uint64_t elapsedMs = 0;      // the measurement window, in milliseconds
};

namespace platform {

/**
 * The rate of the 3DS hardware tick counter.
 *
 * The counter is clocked from the crystal of the SoC, not from the ARM11
 * core clock, so it does not change when a New 3DS boosts to 804 MHz or when
 * the firmware changes the core clock.
 */
constexpr uint64_t TICK_RATE = 268111856ull;

/** Sets the console up. */
bool init();

/** Cleans the console up. */
void exit();

/** The current value of the hardware tick counter. */
uint64_t ticks();

/** Whether the console is a New 3DS. */
bool isNew3DS();

/**
 * Asks for the faster clock and the L2 cache of a New 3DS.
 *
 * The request is best effort: the firmware is free to refuse it, and the
 * clock of the console may change at any moment. The gameplay does not
 * depend on it, because the gameplay time comes from the tick counter.
 */
void requestNew3DSPerformance();

/** The buttons that are currently held down. */
uint32_t heldButtons();

/** The buttons that were pressed this frame. */
uint32_t pressedButtons();

/** The buttons that were released this frame. */
uint32_t releasedButtons();

/** Measures the tick rate and the display rate. */
ClockReport measureClock(uint32_t frames);

} // namespace platform

} // namespace p3ds

#endif // P3DS_PLATFORM_H
