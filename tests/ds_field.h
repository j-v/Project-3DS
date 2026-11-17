/*
    Copyright 2026 Project 3DS contributors

    SPDX-License-Identifier: GPL-3.0-or-later
    This file is licensed under the GNU General Public License, version 3 or
    later. It is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See LICENSE for the full licence text.
*/

#ifndef P3DS_DS_FIELD_H
#define P3DS_DS_FIELD_H

#include "core/field.h"

namespace testing {

/**
 * The field geometry the frozen fixtures were recorded with.
 *
 * The tests that replay the fixtures have to use it as well. It is test-only
 * code and does not belong in the core.
 */
inline p3ds::FieldConfig dsFieldConfig()
{
    p3ds::FieldConfig field;
    field.width = 256;
    field.height = 192;
    field.travelDivX = 270000;
    field.travelDivY = 270000;
    return field;
}

} // namespace testing

#endif // P3DS_DS_FIELD_H
