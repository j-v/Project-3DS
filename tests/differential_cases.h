/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#ifndef P3DS_DIFFERENTIAL_CASES_H
#define P3DS_DIFFERENTIAL_CASES_H

#include <cstdint>
#include <vector>

/**
 * The scenarios the differential tests used to run through both engines.
 *
 * They are kept here so that the frozen outcomes in tests/fixtures/ can be
 * replayed against this engine, and so that the archived harness on the
 * archive/differential-tests branch can regenerate them.
 */
namespace diffcases {

/** The flying time the scenarios use, in engine time units. */
constexpr uint32_t FlyTime = 41800; // 25 frames

/** One scenario: a chart and the buttons that are held during each frame. */
struct Case
{
    const char *name;
    int difficulty;
    std::vector<uint8_t> chart;
    std::vector<uint32_t> trace;
};

/** The scenarios that compare judgements, scoring, life and combo. */
const std::vector<Case> &all();

/** The scenario that compares the drawn position of a note. */
struct PositionCase
{
    std::vector<uint8_t> chart;
    std::vector<uint32_t> trace;
};

const PositionCase &position();

} // namespace diffcases

#endif // P3DS_DIFFERENTIAL_CASES_H
