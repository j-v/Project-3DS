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

#ifndef P3DS_RESULTS_H
#define P3DS_RESULTS_H

#include <cstdint>

namespace p3ds {

/** The statistics of a finished song. */
struct Results
{
    float clear = 0;
    uint32_t total = 0;
    uint32_t cools = 0;
    uint32_t fines = 0;
    uint32_t safes = 0;
    uint32_t sads = 0;
    uint32_t misses = 0;
    uint32_t comboMax = 0;
    uint32_t scoreBase = 0;
    uint32_t scoreHold = 0;
    uint32_t scoreSlide = 0;
};

} // namespace p3ds

#endif // P3DS_RESULTS_H
