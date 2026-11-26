/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

// Ground0 themes. A theme is one row of colours and shape flags; every
// Ground0 screen draws from the current row, so a new theme is one more entry
// in kThemes (ground0_theme.cpp) and no new code.
//
// The "1999 Online skin" rows take their colours from the skin table in
// GoatsAndMonkeys' 1999 Online (src/ui/skin.cpp), used with his permission.
#pragma once

#include <cstdint>

namespace ground0 {

struct Theme
{
    const char *id;
    const char *name;

    uint32_t bg;        // list / grid background
    uint32_t face;      // info strip and footer background
    uint32_t bar;       // status bar background
    uint32_t bar_text;
    uint32_t line;      // separators and borders
    uint32_t text;
    uint32_t dim;       // secondary text
    uint32_t accent;    // logo zero, highlights, selection outline
    uint32_t on_accent; // text drawn on an accent fill
    uint32_t accent2;   // update / warning tags
    uint32_t sel;       // selected row or cell fill
    uint32_t sel_text;

    uint8_t radius;     // corner radius of selection, chips and icons
    bool bevel;         // two-tone 3D edges instead of flat borders
};

int theme_count();
const Theme &theme_at(int index);
const Theme &theme();          // current
int theme_index();
void set_theme(int index);     // wraps around; does not persist
int theme_from_id(const char *id);   // -1 if unknown

} // namespace ground0
