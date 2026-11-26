/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

// Ground0 themes. A theme is one row of colours, shape flags and fonts; every
// Ground0 screen draws from the current row, so a new theme is one more entry
// in the table (ground0_theme.cpp) and no new code.
//
// The "1999 Online skin" rows follow the skin table in GoatsAndMonkeys'
// 1999 Online (src/ui/skin.cpp): its colours, title-bar fills, bevels and
// corner radii, used with his permission.
#pragma once

#include <cstdint>

namespace ground0 {

enum class Fill : uint8_t {
    Solid,     // a
    GradH,     // a -> b, left to right
    GradV,     // a -> b, top to bottom
    Stripes,   // a with 1 px b pinstripes
};

enum class Art : uint8_t {
    None,
    Signal,    // signal rings around a beacon
    Radar,     // radar scope with crosshair and blips
};

struct Theme
{
    const char *id = "";
    const char *name = "";

    uint32_t desk = 0;        // behind the window (shows at rounded corners and beside a title tab)
    uint32_t bg = 0;          // list / grid background
    uint32_t face = 0;        // detail pane and button face
    uint32_t line = 0;        // separators and flat borders
    uint32_t text = 0;
    uint32_t dim = 0;         // secondary text on bg and face

    Fill bar_fill = Fill::Solid;
    uint32_t bar = 0;         // status bar
    uint32_t bar_b = 0;
    uint32_t bar_text = 0;
    bool title_center = false;
    bool title_upper = false;
    bool title_tab = false;   // the bar is only as wide as its title

    uint32_t strip = 0;       // info strip, top to bottom gradient
    uint32_t strip_b = 0;
    uint32_t strip_text = 0;
    uint32_t foot = 0;        // key-hint strip, top to bottom gradient
    uint32_t foot_b = 0;
    uint32_t foot_text = 0;

    uint32_t accent = 0;      // logo zero, search text, flat selection outline
    uint32_t on_accent = 0;   // text drawn on an accent fill
    uint32_t accent2 = 0;     // update / warning tags
    uint32_t sel = 0;         // selected row or cell
    uint32_t sel_text = 0;
    bool gloss = false;       // selection and chips get a lighter top edge

    bool bevel = false;       // two-tone 3D edges (hi/lo) instead of flat 1 px borders
    uint32_t hi = 0;
    uint32_t lo = 0;
    uint8_t radius = 0;       // selection, chip corner radius
    uint8_t win_radius = 0;   // top corners of the window

    const char *font_text = "";   // file names under the launcher font folder
    const char *font_bold = "";
    const char *font_logo = "";
    uint8_t text_px = 12;
    uint8_t small_px = 10;
    uint8_t logo_px = 14;

    Art art = Art::None;
};

int theme_count();
const Theme &theme_at(int index);
const Theme &theme();          // current
int theme_index();
void set_theme(int index);     // wraps around; does not persist
int theme_from_id(const char *id);   // -1 if unknown

} // namespace ground0
