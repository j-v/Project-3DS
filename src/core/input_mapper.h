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

#ifndef P3DS_INPUT_MAPPER_H
#define P3DS_INPUT_MAPPER_H

#include <cstdint>

namespace p3ds {

/**
 * The logical actions of the game.
 *
 * These are bit indices corresponding to bits in ActionMask
 */
enum Action
{
    ActionTriangle = 0,
    ActionCircle = 1,
    ActionCross = 2,
    ActionSquare = 3,
    ActionSlideLeft = 4,
    ActionSlideRight = 5,

    // Not an action: the number of actions, used as the array size and the
    // loop bound. Keeping it last means it follows any new action.
    ActionCount = 6
};

// The engine stores the required actions in 8-bit masks, so a new action
// beyond bit 7 needs those masks widened first.
static_assert(ActionCount <= 8, "the engine's action masks are 8 bits");
static_assert(ActionSlideRight + 1 == ActionCount, "ActionCount must follow the last action");

/** A set of logical actions, one bit per action. */
typedef uint32_t ActionMask;

/**
 * The physical buttons the mapper knows about.
 *
 * The platform fills this in with masks in its own button space (the 3DS HID
 * key bits, for instance). The core never needs to know what the bits mean,
 * which keeps the mapping table testable on the host.
 */
struct PhysicalButtons
{
    uint32_t triangle = 0;
    uint32_t circle = 0;
    uint32_t cross = 0;
    uint32_t square = 0;
    uint32_t dpadUp = 0;
    uint32_t dpadRight = 0;
    uint32_t dpadDown = 0;
    uint32_t dpadLeft = 0;
    uint32_t shoulderLeft = 0;
    uint32_t shoulderRight = 0;
};

/**
 * Maps physical buttons to logical actions.
 *
 * The whole mapping lives in one table so that a C-stick scheme can be added
 * later by editing the table only.
 */
class InputMapper
{
public:
    /** Installs the default mapping. */
    void setDefaults(const PhysicalButtons &buttons);

    /** Overrides the buttons that trigger an action. */
    void setAction(int action, uint32_t buttonMask);

    /** The buttons that trigger an action. */
    uint32_t actionButtons(int action) const
    {
        return (action >= 0 && action < ActionCount) ? mapping[action] : 0;
    }

    /** Converts a mask of pressed physical buttons into a mask of actions. */
    ActionMask map(uint32_t pressed) const;

private:
    uint32_t mapping[ActionCount] = {};
};

} // namespace p3ds

#endif // P3DS_INPUT_MAPPER_H
