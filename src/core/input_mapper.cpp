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

#include "input_mapper.h"

namespace p3ds {

void InputMapper::setDefaults(const PhysicalButtons &buttons)
{
    // Each face button doubles as a D-pad direction, and the shoulders are
    // the slides.
    setAction(ActionTriangle, buttons.triangle | buttons.dpadUp);
    setAction(ActionCircle, buttons.circle | buttons.dpadRight);
    setAction(ActionCross, buttons.cross | buttons.dpadDown);
    setAction(ActionSquare, buttons.square | buttons.dpadLeft);
    setAction(ActionSlideLeft, buttons.shoulderLeft);
    setAction(ActionSlideRight, buttons.shoulderRight);
}

void InputMapper::setAction(int action, uint32_t buttonMask)
{
    if (action >= 0 && action < ActionCount)
        mapping[action] = buttonMask;
}

ActionMask InputMapper::map(uint32_t pressed) const
{
    ActionMask actions = 0;

    for (int i = 0; i < ActionCount; i++)
    {
        if (mapping[i] && (pressed & mapping[i]))
            actions |= (ActionMask)1 << i;
    }

    return actions;
}

} // namespace p3ds
