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

#include "platform.h"

#include <3ds.h>

#include <cstdio>

namespace p3ds {
namespace platform {

bool init()
{
    // The APT sets up the event handler that runs the hooks: e.g. the one that
    // pauses the game when the HOME menu is opened
    if (R_FAILED(aptInit()))
        return false;

    gfxInitDefault();
    gfxSetDoubleBuffering(GFX_TOP, true);
    gfxSetDoubleBuffering(GFX_BOTTOM, true);

    // The SD card is mounted as "sdmc:/". A launcher may have mounted it
    // already, and a failure here is not fatal: the game only shows its
    // message about the missing charts when the files cannot be read.
    archiveMountSdmc();

    return true;
}

void exit()
{
    aptExit();
    gfxExit();
}

uint64_t ticks()
{
    // The tick counter runs at a fixed rate of 268,111,856 Hz, no matter
    // what the clock of the ARM11 is
    return svcGetSystemTick();
}

bool isNew3DS()
{
    bool new3ds = false;

    if (R_SUCCEEDED(APT_CheckNew3DS(&new3ds)))
        return new3ds;

    return false;
}

void requestNew3DSPerformance()
{
    // The ptm:sysm session has to be open before the CPU can be configured.
    // Without this, the call fails and the console stays at 268 MHz, which is
    // too slow to decode the quadraphonic OGG files in real time.
    Result initResult = ptmSysmInit();
    Result configResult = -1;

    if (R_SUCCEEDED(initResult))
    {
        // Bit 0 asks for the 804 MHz clock and bit 1 for the L2 cache. The
        // firmware may refuse, and a setting of the user takes precedence, so
        // the result is only logged: the gameplay time does not depend on the
        // clock.
        configResult = PTMSYSM_ConfigureNew3DSCPU(0x3);
        ptmSysmExit();
    }

#ifdef P3DS_DIAG
    // How fast the CPU actually runs. The tick counter is always 268111856 Hz,
    // so the same loop takes about three times as many ticks at 268 MHz as it
    // does at 804 MHz.
    volatile uint32_t sink = 0;
    uint64_t before = svcGetSystemTick();

    for (uint32_t i = 0; i < 10000000; i++)
        sink += i;

    uint64_t after = svcGetSystemTick();
    double milliseconds = (double)(after - before) * 1000.0 / (double)TICK_RATE;
    (void)sink;

    FILE *log = fopen("sdmc:/project-3ds-diag.log", "a");

    if (log)
    {
        fprintf(log, "cpu: new 3ds %d, ptm %08lx, config %08lx, 10M loop %.1f ms\n",
                isNew3DS() ? 1 : 0, (unsigned long)initResult,
                (unsigned long)configResult, milliseconds);
        fclose(log);
    }
#else
    (void)initResult;
    (void)configResult;
#endif
}

uint32_t heldButtons()
{
    return hidKeysHeld();
}

uint32_t pressedButtons()
{
    return hidKeysDown();
}

uint32_t releasedButtons()
{
    return hidKeysUp();
}

ClockReport measureClock(uint32_t frames)
{
    ClockReport report;

    // The wall clock is the RTC-calibrated clock, not steady_clock: steady_clock
    // is implemented with the same tick counter, so measuring against it
    // would compare the counter with itself. The RTC has millisecond
    // resolution, so a short measurement is approximate but can still show
    // a rate that is wrong by a large factor.
    uint64_t firstTicks = ticks();
    uint64_t lastTicks = firstTicks;
    uint64_t firstTime = osGetTime();

    for (uint32_t i = 0; i < frames; i++)
    {
        gspWaitForVBlank();
        lastTicks = ticks();
    }

    uint64_t lastTime = osGetTime();
    uint64_t elapsedTicks = lastTicks - firstTicks;
    uint64_t elapsedMs = lastTime >= firstTime ? lastTime - firstTime : 0;

    report.frames = frames;
    report.elapsedMs = elapsedMs;
    report.displayPeriod = frames ? (double)elapsedTicks / frames : 0;

    if (elapsedMs > 0)
    {
        double seconds = (double)elapsedMs / 1000.0;
        report.tickRate = (double)elapsedTicks / seconds;
        report.displayRate = (double)frames / seconds;
    }

    return report;
}

} // namespace platform
} // namespace p3ds
