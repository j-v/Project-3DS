/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#include "test_framework.h"

#include "core/input_mapper.h"

using namespace p3ds;

namespace {

/** The buttons of the 3DS, in the order libctru defines them. */
const uint32_t KeyA = 1u << 0;
const uint32_t KeyB = 1u << 1;
const uint32_t KeyStart = 1u << 3;
const uint32_t KeyDRight = 1u << 4;
const uint32_t KeyDLeft = 1u << 5;
const uint32_t KeyDUp = 1u << 6;
const uint32_t KeyDDown = 1u << 7;
const uint32_t KeyR = 1u << 8;
const uint32_t KeyL = 1u << 9;
const uint32_t KeyX = 1u << 10;
const uint32_t KeyY = 1u << 11;

PhysicalButtons makeButtons()
{
    PhysicalButtons buttons;
    buttons.triangle = KeyX;
    buttons.circle = KeyA;
    buttons.cross = KeyB;
    buttons.square = KeyY;
    buttons.dpadUp = KeyDUp;
    buttons.dpadRight = KeyDRight;
    buttons.dpadDown = KeyDDown;
    buttons.dpadLeft = KeyDLeft;
    buttons.shoulderLeft = KeyL;
    buttons.shoulderRight = KeyR;
    return buttons;
}

} // namespace

TEST(input_maps_the_face_buttons)
{
    InputMapper mapper;
    mapper.setDefaults(makeButtons());

    CHECK_EQ(mapper.map(KeyX), (ActionMask)1 << ActionTriangle);
    CHECK_EQ(mapper.map(KeyA), (ActionMask)1 << ActionCircle);
    CHECK_EQ(mapper.map(KeyB), (ActionMask)1 << ActionCross);
    CHECK_EQ(mapper.map(KeyY), (ActionMask)1 << ActionSquare);
    CHECK_EQ(mapper.map(KeyL), (ActionMask)1 << ActionSlideLeft);
    CHECK_EQ(mapper.map(KeyR), (ActionMask)1 << ActionSlideRight);
}

TEST(input_maps_the_dpad_to_the_face_buttons)
{
    InputMapper mapper;
    mapper.setDefaults(makeButtons());

    CHECK_EQ(mapper.map(KeyDUp), (ActionMask)1 << ActionTriangle);
    CHECK_EQ(mapper.map(KeyDRight), (ActionMask)1 << ActionCircle);
    CHECK_EQ(mapper.map(KeyDDown), (ActionMask)1 << ActionCross);
    CHECK_EQ(mapper.map(KeyDLeft), (ActionMask)1 << ActionSquare);
}

TEST(input_maps_several_buttons_at_once)
{
    InputMapper mapper;
    mapper.setDefaults(makeButtons());

    ActionMask actions = mapper.map(KeyX | KeyA | KeyL);

    CHECK(actions & ((ActionMask)1 << ActionTriangle));
    CHECK(actions & ((ActionMask)1 << ActionCircle));
    CHECK(actions & ((ActionMask)1 << ActionSlideLeft));
    CHECK(!(actions & ((ActionMask)1 << ActionCross)));
}

TEST(input_ignores_the_buttons_it_does_not_use)
{
    InputMapper mapper;
    mapper.setDefaults(makeButtons());

    CHECK_EQ(mapper.map(KeyStart), 0u);
    CHECK_EQ(mapper.map(0), 0u);
}

TEST(input_action_lookup_is_bounded)
{
    InputMapper mapper;
    mapper.setDefaults(makeButtons());

    CHECK_EQ(mapper.actionButtons(ActionTriangle), KeyX | KeyDUp);
    CHECK_EQ(mapper.actionButtons(-1), 0u);
    CHECK_EQ(mapper.actionButtons(ActionCount), 0u);
}

TEST(input_can_be_re_mapped)
{
    // The mapping table is the only place a new control scheme has to go
    InputMapper mapper;
    mapper.setDefaults(makeButtons());
    mapper.setAction(ActionTriangle, KeyStart);

    CHECK_EQ(mapper.map(KeyStart), (ActionMask)1 << ActionTriangle);
    CHECK_EQ(mapper.map(KeyX), 0u);
}
