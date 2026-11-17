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

#ifndef P3DS_FIELD_H
#define P3DS_FIELD_H

#include <cstdint>

namespace p3ds {

/**
 * Geometry of the gameplay field.
 *
 * Charts live in a 480000 x 270000 coordinate space, mapped onto a 400 x 240
 * screen. That leaves a 6.7% horizontal stretch.
 *
 * A note's travel is scaled with a base divisor of 270000, adjusted per axis
 * so that a note keeps the same proportions on screen: the horizontal divisor
 * is 270000 * 256 / 400 = 172800 and the vertical one is 270000 * 192 / 240 =
 * 216000. With a divisor of 480000 / width instead, the offset of the flight
 * would be thousands of pixels wide, and a note would only be seen in the
 * last moments before it has to be hit.
 */
struct FieldConfig
{
    int32_t width = 400;
    int32_t height = 240;
    int32_t centerOffset = 16; // half of the 32x32 note sprite
    int32_t travelDivX = 172800; // 270000 * 256 / 400
    int32_t travelDivY = 216000; // 270000 * 192 / 240
};

/**
 * Converts a chart coordinate to a screen pixel.
 *
 * The chart's coordinate space is not distorted beyond the field's own ratio.
 */
inline int32_t chartToScreenX(int32_t chartX, const FieldConfig &field)
{
    return chartX * field.width / 480000 - field.centerOffset;
}

inline int32_t chartToScreenY(int32_t chartY, const FieldConfig &field)
{
    return chartY * field.height / 270000 - field.centerOffset;
}

} // namespace p3ds

#endif // P3DS_FIELD_H
