/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_theme.hpp"

#include <cstring>
#include <vector>

namespace ground0 {
namespace {

// Stock fonts (share/font) and Ground0's own (share/font/ground0, all OFL).
constexpr const char *kMono = "JetBrainsMono-Regular.ttf";
constexpr const char *kMonoBold = "JetBrainsMono-Bold.ttf";
constexpr const char *kSans = "DejaVuSans.ttf";
constexpr const char *kTech = "ground0/ShareTechMono-Regular.ttf";
constexpr const char *kCrt = "ground0/VT323-Regular.ttf";
constexpr const char *kChakra = "ground0/ChakraPetch-Bold.ttf";
constexpr const char *kOrbitron = "ground0/Orbitron-Variable.ttf";
constexpr const char *kStencil = "ground0/SairaStencilOne-Regular.ttf";
constexpr const char *kPixel = "ground0/PressStart2P-Regular.ttf";

// A dark, flat Ground0 original.
Theme original(const char *id, const char *name, uint32_t bg, uint32_t panel, uint32_t line,
               uint32_t text, uint32_t dim, uint32_t accent, uint32_t on_accent,
               uint32_t accent2, uint32_t sel)
{
    Theme t;
    t.id = id;
    t.name = name;
    t.desk = bg;
    t.bg = bg;
    t.face = panel;
    t.line = line;
    t.text = text;
    t.dim = dim;
    t.bar = t.bar_b = panel;
    t.bar_text = text;
    t.strip = t.strip_b = panel;
    t.strip_text = dim;
    t.foot = t.foot_b = panel;
    t.foot_text = dim;
    t.accent = accent;
    t.on_accent = on_accent;
    t.accent2 = accent2;
    t.sel = sel;
    t.sel_text = text;
    t.hi = t.lo = line;
    t.font_text = kMono;
    t.font_bold = kMonoBold;
    t.font_logo = kChakra;
    return t;
}

// A 1999 Online desktop skin: white list, grey chrome, DejaVu Sans like the original.
Theme skin(const char *id, const char *name, uint32_t desk, uint32_t face, Fill bar_fill,
           uint32_t bar, uint32_t bar_b, uint32_t bar_text, uint32_t sel, uint32_t sel_text,
           uint32_t lo)
{
    Theme t;
    t.id = id;
    t.name = name;
    t.desk = desk;
    t.bg = 0xffffff;
    t.face = face;
    t.line = lo;
    t.text = 0x000000;
    t.dim = 0x4a4a4a;
    t.bar_fill = bar_fill;
    t.bar = bar;
    t.bar_b = bar_b;
    t.bar_text = bar_text;
    t.strip = t.strip_b = face;
    t.strip_text = 0x000000;
    t.foot = t.foot_b = face;
    t.foot_text = 0x000000;
    t.accent = sel;
    t.on_accent = sel_text;
    t.accent2 = 0xc00000;
    t.sel = sel;
    t.sel_text = sel_text;
    t.hi = 0xffffff;
    t.lo = lo;
    t.font_text = kSans;
    t.font_bold = kSans;
    t.font_logo = kSans;
    t.text_px = 11;
    t.small_px = 10;
    t.logo_px = 12;
    return t;
}

std::vector<Theme> make_themes()
{
    std::vector<Theme> all;
    Theme t;

    // ---- Ground0 originals -------------------------------------------------
    t = original("chromeshore", "Chromeshore Signal", 0x08111f, 0x0b1729, 0x1e3352, 0xe6eef8,
                 0x8fa3bd, 0x2bd9f5, 0x04202a, 0xffb020, 0x12305a);
    t.radius = 2;
    t.art = Art::Signal;
    all.push_back(t);

    t = original("irak4t0n", "IRAK4T0N", 0x070a07, 0x0d130d, 0x24402a, 0xd8ffe0, 0x7fb58c,
                 0x39ff6a, 0x031207, 0xffc400, 0x143a1e);
    t.font_text = t.font_bold = kTech;
    t.font_logo = kStencil;
    t.title_upper = true;
    t.art = Art::Radar;
    all.push_back(t);

    t = original("nightcity", "Night City", 0x0b0714, 0x140b24, 0x3b1f5c, 0xf6eeff, 0xb39bd6,
                 0xf6e71d, 0x191600, 0xff2e88, 0x35104a);
    t.font_logo = kOrbitron;
    t.logo_px = 13;
    t.bar_fill = Fill::GradH;
    t.bar_b = 0x3b1250;
    all.push_back(t);

    t = original("netrunner", "Netrunner", 0x03121a, 0x06202c, 0x0f4257, 0xd6f6ff, 0x7fb3c4,
                 0x00e5ff, 0x00181d, 0xff3d5a, 0x083a4d);
    t.font_text = t.font_bold = kTech;
    t.bar_fill = Fill::GradH;
    t.bar_b = 0x0f4257;
    t.radius = 1;
    all.push_back(t);

    t = original("meshspace", "Mesh Space", 0x101a4a, 0x1b2a6b, 0x5a6fd0, 0xffffff, 0xc4ccf5,
                 0xffe14a, 0x1a1400, 0xff8ad8, 0x2c3f96);
    t.font_text = t.font_bold = kSans;
    t.text_px = 11;
    t.font_logo = kPixel;
    t.logo_px = 9;
    t.bar_fill = Fill::GradH;
    t.bar_b = 0x6a2a8a;
    all.push_back(t);

    t = original("phosphor", "Phosphor", 0x000a02, 0x001405, 0x0b4a1c, 0x5cff7e, 0x3fbf5c,
                 0x5cff7e, 0x001405, 0xc8ffd4, 0x0b3315);
    t.font_text = t.font_bold = t.font_logo = kCrt;
    t.text_px = 16;
    t.small_px = 14;
    t.logo_px = 20;
    t.title_upper = true;
    all.push_back(t);

    t = original("amber", "Amber Terminal", 0x0c0700, 0x160d00, 0x5a3a00, 0xffb84a, 0xc98a2e,
                 0xffb84a, 0x160d00, 0xffe2a8, 0x3a2500);
    t.font_text = t.font_bold = t.font_logo = kCrt;
    t.text_px = 16;
    t.small_px = 14;
    t.logo_px = 20;
    t.title_upper = true;
    all.push_back(t);

    t = original("sunset", "Sunset Grid", 0x1a0b2e, 0x26103f, 0x5b2a86, 0xffe9f6, 0xc9a5e0,
                 0xff7a3d, 0x240c00, 0xff4fa3, 0x451a6b);
    t.font_logo = kOrbitron;
    t.logo_px = 13;
    t.bar_fill = Fill::GradH;
    t.bar_b = 0x7a2a6b;
    t.radius = 3;
    t.gloss = true;
    all.push_back(t);

    t = original("blackout", "Blackout", 0x000000, 0x000000, 0x2a2a2a, 0xe8e8e8, 0x9a9a9a,
                 0xff3b30, 0x1a0000, 0xe8e8e8, 0x2a0a08);
    all.push_back(t);

    t = original("daylight", "Daylight", 0xf4f1e8, 0xffffff, 0x1a1a1a, 0x111111, 0x4a4a4a,
                 0x0b4fd8, 0xffffff, 0xb23a00, 0xd6e2ff);
    t.font_text = t.font_bold = kSans;
    t.text_px = 11;
    t.radius = 3;
    all.push_back(t);

    // ---- 1999 Online skins --------------------------------------------------
    t = skin("w95", "Windows 95", 0xc0c0c0, 0xc0c0c0, Fill::Solid, 0x000080, 0x000080, 0xffffff,
             0x000080, 0xffffff, 0x404040);
    t.bevel = true;
    all.push_back(t);

    t = skin("me", "Windows ME", 0xd4d0c8, 0xd4d0c8, Fill::GradH, 0x0a246a, 0xa6caf0, 0xffffff,
             0x0a246a, 0xffffff, 0x404040);
    t.bevel = true;
    all.push_back(t);

    t = skin("xp", "Windows XP", 0x3a6ea5, 0xece9d8, Fill::GradV, 0x3a93ff, 0x0046c8, 0xffffff,
             0x316ac5, 0xffffff, 0x003c74);
    t.strip = 0x3a80f0;
    t.strip_b = 0x245edb;
    t.strip_text = 0xffffff;
    t.foot = 0x3a80f0;
    t.foot_b = 0x1f4fc0;
    t.foot_text = 0xffffff;
    t.accent2 = 0xe04a2f;
    t.radius = 3;
    t.win_radius = 6;
    t.gloss = true;
    all.push_back(t);

    t = skin("vista", "Windows Vista", 0x1f5a7a, 0xf0f0f0, Fill::GradV, 0x8fa8c4, 0x2f4a6c,
             0xffffff, 0xcde8ff, 0x000000, 0x707070);
    t.strip = 0x4a5a6c;
    t.strip_b = 0x0c1016;
    t.strip_text = 0xffffff;
    t.foot = 0x4a5a6c;
    t.foot_b = 0x0c1016;
    t.foot_text = 0xffffff;
    t.accent = 0x1f5a7a;
    t.on_accent = 0xffffff;
    t.accent2 = 0xc9402e;
    t.radius = 3;
    t.win_radius = 6;
    t.gloss = true;
    all.push_back(t);

    t = skin("sys7", "Mac System 7", 0xaaaaaa, 0xffffff, Fill::Stripes, 0xffffff, 0x000000,
             0x000000, 0x000000, 0xffffff, 0x000000);
    t.title_center = true;
    t.accent2 = 0x000000;
    t.radius = 6;
    all.push_back(t);

    t = skin("mac9", "Mac OS 9", 0x6a8fc0, 0xdddddd, Fill::Stripes, 0xdddddd, 0x999999, 0x000000,
             0x333399, 0xffffff, 0x000000);
    t.title_center = true;
    t.strip = t.strip_b = 0xe8e8e8;
    t.foot = t.foot_b = 0xbbbbbb;
    t.radius = 5;
    all.push_back(t);

    t = skin("aqua", "Mac OS X Aqua", 0x2f6fc9, 0xececec, Fill::Stripes, 0xf4f4f4, 0xd8d8d8,
             0x000000, 0x3875d7, 0xffffff, 0x8c8c8c);
    t.title_center = true;
    t.strip = 0xf6f6f6;
    t.strip_b = 0xd2d2d2;
    t.foot = 0xf0f0f0;
    t.foot_b = 0xcfcfcf;
    t.radius = 6;
    t.win_radius = 6;
    t.gloss = true;
    all.push_back(t);

    t = skin("graphite", "Mac OS X Graphite", 0x56667c, 0xececec, Fill::Stripes, 0xf4f4f4,
             0xd8d8d8, 0x000000, 0x7c8ca4, 0x000000, 0x8c8c8c);
    t.title_center = true;
    t.strip = 0xf6f6f6;
    t.strip_b = 0xd2d2d2;
    t.foot = 0xf0f0f0;
    t.foot_b = 0xcfcfcf;
    t.accent = 0x4a586c;
    t.on_accent = 0xffffff;
    t.accent2 = 0x803030;
    t.radius = 6;
    t.win_radius = 6;
    t.gloss = true;
    all.push_back(t);

    t = skin("metal", "Mac OS X Metal", 0x2a5d9f, 0xc4c4c4, Fill::GradV, 0xe4e4e4, 0xa4a4a4,
             0x000000, 0x3875d7, 0xffffff, 0x6a6a6a);
    t.title_center = true;
    t.dim = 0x3a3a3a;
    t.strip = 0xdadada;
    t.strip_b = 0xa2a2a2;
    t.foot = 0xc8c8c8;
    t.foot_b = 0x9a9a9a;
    t.hi = 0xf2f2f2;
    t.radius = 5;
    t.win_radius = 6;
    t.gloss = true;
    all.push_back(t);

    t = skin("leopard", "Mac OS X Leopard", 0x34296e, 0xe8e8e8, Fill::GradV, 0xe8e8e8, 0xbcbcbc,
             0x2a2a2a, 0x2e65d0, 0xffffff, 0x7a7a7a);
    t.title_center = true;
    t.strip = 0xe4e4e4;
    t.strip_b = 0xbcbcbc;
    t.strip_text = 0x2a2a2a;
    t.foot = 0xdcdcdc;
    t.foot_b = 0xb4b4b4;
    t.foot_text = 0x2a2a2a;
    t.radius = 5;
    t.win_radius = 6;
    t.gloss = true;
    all.push_back(t);

    t = skin("beos", "BeOS", 0x336698, 0xd8d8d8, Fill::Solid, 0xffcb00, 0xffcb00, 0x000000,
             0x9a9a9a, 0x000000, 0x606060);
    t.title_tab = true;
    t.accent = 0x1f4f80;
    t.on_accent = 0xffffff;
    t.radius = 3;
    all.push_back(t);

    t = skin("os2", "OS/2 Warp", 0x5f8f8f, 0xcccccc, Fill::Solid, 0x1f7a7a, 0x1f7a7a, 0xffffff,
             0x808080, 0x000000, 0x808080);
    t.title_center = true;
    t.accent = 0x145a5a;
    t.on_accent = 0xffffff;
    t.bevel = true;
    all.push_back(t);

    t = skin("x11", "X11 Motif", 0x4a6078, 0xaeb2c3, Fill::Solid, 0x4a6078, 0x4a6078, 0xffffff,
             0x000000, 0xffffff, 0x5d6069);
    t.title_center = true;
    t.bg = 0xc9ccd9;
    t.dim = 0x33363f;
    t.strip = t.strip_b = 0x000000;
    t.strip_text = 0xe0e0e0;
    t.accent = 0x000090;
    t.accent2 = 0xa00000;
    t.hi = 0xdfe1ea;
    t.bevel = true;
    all.push_back(t);

    t = skin("winamp", "Winamp", 0x000000, 0x2a2a3c, Fill::GradV, 0x1a1a40, 0x38385a, 0xc8c8dc,
             0x0000c6, 0xffffff, 0x111111);
    t.title_center = true;
    t.title_upper = true;
    t.bg = 0x000000;
    t.line = 0x4a4a66;
    t.text = 0x00e000;
    t.dim = 0x00a000;
    t.strip = t.strip_b = 0x000000;
    t.strip_text = 0x00e000;
    t.foot = t.foot_b = 0x2a2a3c;
    t.foot_text = 0xc8b464;
    t.accent = 0xc8b464;
    t.on_accent = 0x000000;
    t.accent2 = 0xc8b464;
    t.hi = 0x7a7a9a;
    t.bevel = true;
    t.font_text = t.font_bold = t.font_logo = kCrt;
    t.text_px = 16;
    t.small_px = 14;
    t.logo_px = 18;
    all.push_back(t);

    return all;
}

const std::vector<Theme> &themes()
{
    static const std::vector<Theme> all = make_themes();
    return all;
}

int g_current = 0;

int wrap(int index)
{
    const int count = static_cast<int>(themes().size());
    index %= count;
    return index < 0 ? index + count : index;
}

} // namespace

int theme_count()
{
    return static_cast<int>(themes().size());
}

const Theme &theme_at(int index)
{
    return themes()[static_cast<std::size_t>(wrap(index))];
}

const Theme &theme()
{
    return themes()[static_cast<std::size_t>(g_current)];
}

int theme_index()
{
    return g_current;
}

void set_theme(int index)
{
    g_current = wrap(index);
}

int theme_from_id(const char *id)
{
    if (!id) return -1;
    const std::vector<Theme> &all = themes();
    for (std::size_t i = 0; i < all.size(); ++i)
        if (std::strcmp(all[i].id, id) == 0) return static_cast<int>(i);
    return -1;
}

} // namespace ground0
