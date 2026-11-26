/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_home.hpp"

#include "cp0_lvgl_app.h"
#include "ground0_theme.hpp"
#include "input_keys.h"
#include "ui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <utility>

#include <unistd.h>

namespace {

constexpr int kScreenW = 320;
constexpr int kScreenH = 170;
constexpr int kBarH = 20;
constexpr int kStripH = 16;
constexpr int kFootH = 13;
constexpr int kBodyY = kBarH + kStripH;
constexpr int kBodyH = kScreenH - kBarH - kStripH - kFootH;   // 121
constexpr int kRowH = 20;
constexpr int kListW = 190;
constexpr int kRowIcon = 16;
constexpr int kDetailIcon = 56;
constexpr int kCellW = 64;
constexpr int kCellH = 60;
constexpr int kCellIcon = 36;

// Fonts live in the launcher font folder; Ground0's own are in its ground0/
// subfolder. GROUND0_FONT_DIR points at a loose copy of that subfolder, for
// running a test build against an installed package that does not ship them.
std::string font_path(const char *file)
{
    static std::string font_dir;
    if (font_dir.empty()) {
        std::string known;
        cp0_signal_filesystem_api({"Path", "JetBrainsMono-Regular.ttf"},
                                  [&](int code, std::string data) { if (code == 0) known = std::move(data); });
        const std::size_t slash = known.find_last_of('/');
        font_dir = slash == std::string::npos ? "." : known.substr(0, slash);
    }
    const std::string name = file ? file : "";
    if (name.rfind("ground0/", 0) == 0) {
        if (const char *override_dir = std::getenv("GROUND0_FONT_DIR")) {
            const std::string candidate = std::string(override_dir) + "/" + name.substr(8);
            if (access(candidate.c_str(), R_OK) == 0) return candidate;
        }
    }
    const std::string bundled = font_dir + "/" + name;
    if (access(bundled.c_str(), R_OK) == 0) return bundled;
    return font_dir + "/JetBrainsMono-Regular.ttf";   // a theme font is missing: stay readable
}

const lv_font_t *load_font(const char *file, uint16_t size, bool bold = false)
{
    lv_font_t *font = launcher_fonts().get(
        font_path(file).c_str(), size,
        bold ? LV_FREETYPE_FONT_STYLE_BOLD : LV_FREETYPE_FONT_STYLE_NORMAL);
    return font ? font : LV_FONT_DEFAULT;
}

const lv_font_t *text_font()  { const auto &t = ground0::theme(); return load_font(t.font_text, t.text_px); }
const lv_font_t *bold_font()  { const auto &t = ground0::theme(); return load_font(t.font_bold, t.text_px, true); }
const lv_font_t *small_font() { const auto &t = ground0::theme(); return load_font(t.font_text, t.small_px); }
const lv_font_t *chip_font()  { const auto &t = ground0::theme(); return load_font(t.font_bold, t.small_px, true); }
const lv_font_t *logo_font()  { const auto &t = ground0::theme(); return load_font(t.font_logo, t.logo_px, true); }

lv_color_t hex(uint32_t value)
{
    return lv_color_hex(value);
}

// A bare rectangle: no theme styles, no scrolling, no click handling.
lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, bool filled = true)
{
    lv_obj_t *obj = lv_obj_create(parent);
    if (!obj) return nullptr;
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_clear_flag(obj, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    if (filled) {
        lv_obj_set_style_bg_color(obj, hex(color), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    }
    return obj;
}

lv_obj_t *text(lv_obj_t *parent, int x, int y, int w, const lv_font_t *font, uint32_t color,
               const char *value, lv_text_align_t align = LV_TEXT_ALIGN_LEFT)
{
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    if (w > 0) {
        lv_obj_set_width(label, w);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_set_height(label, lv_font_get_line_height(font));
    }
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_label_set_text(label, value ? value : "");
    return label;
}

// Vertical position that centres one line of `font` in a strip `height` tall.
int centred(int height, const lv_font_t *font)
{
    return (height - static_cast<int>(lv_font_get_line_height(font))) / 2;
}

void gradient(lv_obj_t *obj, uint32_t a, uint32_t b, bool horizontal)
{
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, hex(a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    if (a == b) {
        lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        return;
    }
    lv_obj_set_style_bg_grad_color(obj, hex(b), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(obj, horizontal ? LV_GRAD_DIR_HOR : LV_GRAD_DIR_VER, LV_PART_MAIN);
}

uint32_t lighten(uint32_t color, int percent)
{
    auto mix = [percent](uint32_t channel) {
        return channel + (255 - channel) * static_cast<uint32_t>(percent) / 100;
    };
    return (mix((color >> 16) & 0xff) << 16) | (mix((color >> 8) & 0xff) << 8) | mix(color & 0xff);
}

// Selection fill: glossy themes fade from a lighter top edge, the rest are flat.
void selection_fill(lv_obj_t *obj)
{
    const ground0::Theme &t = ground0::theme();
    gradient(obj, t.gloss ? lighten(t.sel, 35) : t.sel, t.sel, false);
}

// Two-tone 3D edge. Raised: light top-left, dark bottom-right. Sunken: the reverse.
void bevel_edge(lv_obj_t *parent, int x, int y, int w, int h, bool raised)
{
    const ground0::Theme &t = ground0::theme();
    const uint32_t top_left = raised ? t.hi : t.lo;
    const uint32_t bottom_right = raised ? t.lo : t.hi;
    box(parent, x, y, w, 1, top_left);
    box(parent, x, y, 1, h, top_left);
    box(parent, x, y + h - 1, w, 1, bottom_right);
    box(parent, x + w - 1, y, 1, h, bottom_right);
}

// Perceived brightness, 0..255000.
uint32_t luma(uint32_t color)
{
    return ((color >> 16) & 0xff) * 299 + ((color >> 8) & 0xff) * 587 + (color & 0xff) * 114;
}

bool is_dark(uint32_t color)
{
    return luma(color) < 128000;
}

bool reads_on(uint32_t color, uint32_t background)
{
    const uint32_t a = luma(color), b = luma(background);
    return (a > b ? a - b : b - a) > 70000;
}

lv_obj_t *ring(lv_obj_t *parent, int cx, int cy, int r, uint32_t color, lv_opa_t opa)
{
    lv_obj_t *obj = box(parent, cx - r, cy - r, 2 * r, 2 * r, 0, false);
    if (!obj) return nullptr;
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, hex(color), LV_PART_MAIN);
    lv_obj_set_style_border_opa(obj, opa, LV_PART_MAIN);
    return obj;
}

lv_obj_t *faint(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, lv_opa_t opa)
{
    lv_obj_t *obj = box(parent, x, y, w, h, color);
    if (obj) lv_obj_set_style_bg_opa(obj, opa, LV_PART_MAIN);
    return obj;
}

// Faint theme art behind the list or grid. Drawn from primitives, no image.
void draw_art(lv_obj_t *body, int width, int height)
{
    const ground0::Theme &t = ground0::theme();
    if (t.art == ground0::Art::Signal) {
        // Signal rings around a beacon: arcs opening left and right.
        const int cx = width - 62, cy = height / 2;
        for (int i = 1; i <= 3; ++i) {
            const int r = 12 + i * 14;
            lv_obj_t *arc = lv_arc_create(body);
            if (!arc) continue;
            lv_obj_remove_style_all(arc);
            lv_obj_set_size(arc, 2 * r, 2 * r);
            lv_obj_set_pos(arc, cx - r, cy - r);
            lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
            lv_arc_set_bg_angles(arc, 135, 225);
            lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
            lv_obj_set_style_arc_color(arc, hex(t.accent), LV_PART_MAIN);
            lv_obj_set_style_arc_opa(arc, LV_OPA_20, LV_PART_MAIN);
            lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
            lv_arc_set_value(arc, 0);
            lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);

            lv_obj_t *mirror = lv_arc_create(body);
            if (!mirror) continue;
            lv_obj_remove_style_all(mirror);
            lv_obj_set_size(mirror, 2 * r, 2 * r);
            lv_obj_set_pos(mirror, cx - r, cy - r);
            lv_obj_clear_flag(mirror, LV_OBJ_FLAG_CLICKABLE);
            lv_arc_set_bg_angles(mirror, 315, 45);
            lv_obj_set_style_arc_width(mirror, 3, LV_PART_MAIN);
            lv_obj_set_style_arc_color(mirror, hex(t.accent), LV_PART_MAIN);
            lv_obj_set_style_arc_opa(mirror, LV_OPA_20, LV_PART_MAIN);
            lv_obj_set_style_arc_rounded(mirror, true, LV_PART_MAIN);
            lv_obj_set_style_arc_opa(mirror, LV_OPA_TRANSP, LV_PART_INDICATOR);
        }
        lv_obj_t *beacon = faint(body, cx - 5, cy - 5, 10, 10, t.accent2, LV_OPA_40);
        if (beacon) lv_obj_set_style_radius(beacon, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    } else if (t.art == ground0::Art::Radar) {
        // Data rain: broken vertical streaks.
        static const int kColumns[] = {12, 30, 52, 70, 96, 118, 140, 166};
        int seed = 7;
        for (int x : kColumns) {
            if (x >= width) break;
            int y = (seed * 13) % 17;
            while (y < height) {
                const int len = 4 + (seed * 7) % 13;
                faint(body, x, y, 1, std::min(len, height - y), t.accent, LV_OPA_30);
                y += len + 5 + (seed * 5) % 9;
                seed = (seed * 31 + 11) % 97;
            }
        }
        // Radar scope with crosshair and two blips.
        const int cx = width - 62, cy = height / 2;
        for (int r : {18, 36, 54})
            ring(body, cx, cy, r, t.accent, LV_OPA_30);
        faint(body, cx - 54, cy, 108, 1, t.accent, LV_OPA_20);
        faint(body, cx, cy - 54, 1, 108, t.accent, LV_OPA_20);
        lv_obj_t *blip = faint(body, cx + 20, cy - 24, 5, 5, t.accent2, LV_OPA_50);
        if (blip) lv_obj_set_style_radius(blip, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_t *echo = faint(body, cx - 30, cy + 16, 4, 4, t.accent, LV_OPA_50);
        if (echo) lv_obj_set_style_radius(echo, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }
}

std::string config_get_str(const char *key, std::string value)
{
    cp0_signal_config_api({"GetStr", key, value},
                          [&](int code, std::string data) { if (code == 0) value = std::move(data); });
    return value;
}

void config_set_str(const char *key, const std::string &value)
{
    cp0_signal_config_api({"SetStr", key, value}, [](int, std::string) {});
    cp0_signal_config_api({"Save"}, [](int, std::string) {});
}

std::string lower(const std::string &value)
{
    std::string out = value;
    for (char &c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

// 0 = name starts with the query, 1 = a word in the name starts with it,
// 2 = it appears anywhere, -1 = no match.
int match_rank(const std::string &name, const std::string &query)
{
    if (query.empty()) return 0;
    const std::size_t at = name.find(query);
    if (at == std::string::npos) return -1;
    if (at == 0) return 0;
    for (std::size_t pos = at; pos != std::string::npos; pos = name.find(query, pos + 1)) {
        const unsigned char before = static_cast<unsigned char>(name[pos - 1]);
        if (!std::isalnum(before)) return 1;
    }
    return 2;
}

} // namespace

Ground0Home::Ground0Home(lv_obj_t *parent, IconLookup lookup)
    : parent_(parent), lookup_(std::move(lookup))
{
    if (!parent_) return;
    load_settings();
    root_ = box(parent_, 0, 0, kScreenW, kScreenH, ground0::theme().desk);
    if (!root_) return;
    lv_obj_add_event_cb(root_, on_root_deleted, LV_EVENT_DELETE, this);
    status_timer_ = lv_timer_create(on_status_timer, 15000, this);
    rebuild();
}

Ground0Home::~Ground0Home()
{
    if (status_timer_) lv_timer_delete(status_timer_);
    status_timer_ = nullptr;
    if (root_) {
        lv_obj_remove_event_cb_with_user_data(root_, on_root_deleted, this);
        lv_obj_delete(root_);
        root_ = nullptr;
    }
}

void Ground0Home::on_root_deleted(lv_event_t *event) noexcept
{
    auto *self = static_cast<Ground0Home *>(lv_event_get_user_data(event));
    if (!self || lv_event_get_target(event) != lv_event_get_current_target(event)) return;
    self->root_ = nullptr;
    self->clock_ = self->battery_ = self->strip_left_ = self->strip_right_ = nullptr;
    self->foot_label_ = nullptr;
    self->detail_icon_ = self->detail_name_ = self->detail_sub_ = nullptr;
    self->rows_ = {};
    self->row_count_ = 0;
}

void Ground0Home::on_status_timer(lv_timer_t *timer) noexcept
{
    try {
        auto *self = static_cast<Ground0Home *>(lv_timer_get_user_data(timer));
        if (self && self->root_) self->update_status();
    } catch (...) {
    }
}

void Ground0Home::load_settings()
{
    const int index = ground0::theme_from_id(config_get_str("ground0_theme", "chromeshore").c_str());
    ground0::set_theme(index < 0 ? 0 : index);
    view_ = config_get_str("ground0_view", "list") == "grid" ? View::Grid : View::List;
}

void Ground0Home::save_settings()
{
    config_set_str("ground0_theme", ground0::theme().id);
    config_set_str("ground0_view", view_ == View::Grid ? "grid" : "list");
}

std::size_t Ground0Home::page_size() const
{
    return view_ == View::Grid ? kGridCells : kListRows;
}

void Ground0Home::release_icons()
{
    for (Row &row : rows_)
        if (row.icon) lv_image_set_src(row.icon, nullptr);
    if (detail_icon_) lv_image_set_src(detail_icon_, nullptr);
}

void Ground0Home::set_items(std::vector<Ground0Item> items, std::size_t selected)
{
    items_ = std::move(items);
    refilter(selected);
}

std::size_t Ground0Home::selected() const
{
    return selected_ < visible_.size() ? visible_[selected_] : 0;
}

void Ground0Home::refilter(std::size_t keep_item)
{
    const std::string query = lower(query_);
    std::vector<std::pair<int, std::size_t>> ranked;
    ranked.reserve(items_.size());
    for (std::size_t i = 0; i < items_.size(); ++i) {
        const int rank = searching_ ? match_rank(lower(items_[i].name), query) : 0;
        if (rank >= 0) ranked.emplace_back(rank, i);
    }
    // Stable: equal ranks keep the launcher's own app order.
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto &a, const auto &b) { return a.first < b.first; });
    visible_.clear();
    visible_.reserve(ranked.size());
    for (const auto &entry : ranked) visible_.push_back(entry.second);

    selected_ = 0;
    if (!searching_ || query.empty()) {
        const auto found = std::find(visible_.begin(), visible_.end(), keep_item);
        if (found != visible_.end())
            selected_ = static_cast<std::size_t>(found - visible_.begin());
    }
    first_ = 0;
    bind();
}

void Ground0Home::rebuild()
{
    if (!root_) return;
    lv_obj_clean(root_);
    clock_ = battery_ = strip_left_ = strip_right_ = nullptr;
    foot_label_ = nullptr;
    detail_icon_ = detail_name_ = detail_sub_ = nullptr;
    rows_ = {};
    row_count_ = 0;

    lv_obj_set_style_bg_color(root_, hex(ground0::theme().desk), LV_PART_MAIN);
    build_bar();
    build_strip();
    if (view_ == View::Grid)
        build_grid();
    else
        build_list();
    build_footer();
    update_status();
    bind();
}

void Ground0Home::build_bar()
{
    const ground0::Theme &t = ground0::theme();
    const lv_font_t *logo = logo_font();
    const int title_y = centred(kBarH, logo);

    // Measure the wordmark first: a title tab is only as wide as its text.
    lv_obj_t *word = text(root_, 0, title_y, 0, logo, t.bar_text, "GROUND");
    lv_obj_t *zero = text(root_, 0, title_y, 0, logo, t.bar_text, "0");
    if (!word || !zero) return;
    lv_obj_update_layout(word);
    lv_obj_update_layout(zero);
    const int word_w = lv_obj_get_width(word);
    const int title_w = word_w + lv_obj_get_width(zero);

    const int bar_w = t.title_tab ? title_w + 20 : kScreenW;
    // Taller than the bar by the corner radius, so only the top corners are
    // rounded: the info strip covers the rounded bottom edge.
    lv_obj_t *bar = box(root_, 0, 0, bar_w, kBarH + t.win_radius, t.bar);
    if (!bar) return;
    lv_obj_set_style_radius(bar, t.win_radius, LV_PART_MAIN);
    gradient(bar, t.bar, t.bar_fill == ground0::Fill::Stripes ? t.bar : t.bar_b,
             t.bar_fill == ground0::Fill::GradH);

    const int title_x = t.title_center ? (bar_w - title_w) / 2 : (t.title_tab ? 10 : 6);
    if (t.bar_fill == ground0::Fill::Stripes) {
        for (int y = 4; y < kBarH - 4; y += 2)
            box(root_, 4, y, bar_w - 8, 1, t.bar_b);
        box(root_, title_x - 5, 1, title_w + 10, kBarH - 2, t.bar);        // clear behind the title
        box(root_, kScreenW - 88, 1, 84, kBarH - 2, t.bar);                // and behind the status text
    }
    if (t.bevel) {
        bevel_edge(root_, 0, 0, bar_w, kBarH, true);
    } else if (t.bar_fill == ground0::Fill::Solid || t.title_tab) {
        box(root_, 0, kBarH - 1, kScreenW, 1, t.line);
    }

    // The zero takes the accent colour where that reads against the bar.
    const uint32_t under_zero = t.bar_fill == ground0::Fill::GradH ? t.bar : (t.bar_fill == ground0::Fill::GradV ? t.bar_b : t.bar);
    uint32_t zero_color = t.bar_text;
    if (reads_on(t.accent, under_zero))
        zero_color = t.accent;
    else if (reads_on(0xffe14a, under_zero))
        zero_color = 0xffe14a;
    lv_obj_set_style_text_color(zero, hex(zero_color), LV_PART_MAIN);
    lv_obj_set_pos(word, title_x, title_y);
    lv_obj_set_pos(zero, title_x + word_w, title_y);
    lv_obj_move_foreground(word);
    lv_obj_move_foreground(zero);

    const lv_font_t *status = chip_font();
    const uint32_t status_color = t.title_tab ? (is_dark(t.desk) ? 0xffffff : 0x000000) : t.bar_text;
    const int status_y = centred(kBarH, status);
    clock_ = text(root_, kScreenW - 44, status_y, 38, status, status_color, "--:--", LV_TEXT_ALIGN_RIGHT);
    battery_ = text(root_, kScreenW - 84, status_y, 38, status, status_color, "", LV_TEXT_ALIGN_RIGHT);
}

void Ground0Home::build_strip()
{
    const ground0::Theme &t = ground0::theme();
    lv_obj_t *strip = box(root_, 0, kBarH, kScreenW, kStripH, t.strip);
    if (!strip) return;
    gradient(strip, t.strip, t.strip_b, false);
    if (t.bevel)
        bevel_edge(strip, 0, 0, kScreenW, kStripH, true);
    else
        box(strip, 0, kStripH - 1, kScreenW, 1, t.line);
    const lv_font_t *small = small_font();
    const int y = centred(kStripH, small);
    strip_left_ = text(strip, 6, y, 150, small, t.strip_text, "");
    strip_right_ = text(strip, kScreenW - 156, y, 150, small, t.strip_text, t.name, LV_TEXT_ALIGN_RIGHT);
}

void Ground0Home::build_list()
{
    const ground0::Theme &t = ground0::theme();
    const lv_font_t *name_font = text_font();
    const bool outline = !t.bevel && !t.gloss;

    lv_obj_t *body = box(root_, 0, kBodyY, kListW, kBodyH, t.bg);
    if (body) draw_art(body, kListW, kBodyH);

    row_count_ = kListRows;
    for (int i = 0; i < kListRows; ++i) {
        Row &row = rows_[i];
        row.box = box(root_, t.bevel ? 1 : 0, kBodyY + i * kRowH, kListW - (t.bevel ? 2 : 0), kRowH, t.sel);
        if (!row.box) continue;
        selection_fill(row.box);
        lv_obj_set_style_radius(row.box, t.radius, LV_PART_MAIN);
        lv_obj_set_style_border_color(row.box, hex(t.accent), LV_PART_MAIN);
        lv_obj_set_style_border_width(row.box, outline ? 1 : 0, LV_PART_MAIN);
        row.name = text(row.box, 6, centred(kRowH, name_font), kListW - kRowIcon - 18, name_font, t.text, "");
        row.icon = lv_image_create(row.box);
        if (row.icon) {
            lv_obj_set_pos(row.icon, kListW - kRowIcon - 7, (kRowH - kRowIcon) / 2);
            lv_obj_set_size(row.icon, kRowIcon, kRowIcon);
            lv_image_set_inner_align(row.icon, LV_IMAGE_ALIGN_CENTER);
        }
    }
    if (t.bevel) bevel_edge(root_, 0, kBodyY, kListW, kBodyH, false);   // sunken list field

    const int pane_x = kListW;
    const int pane_w = kScreenW - kListW;
    lv_obj_t *pane = box(root_, pane_x, kBodyY, pane_w, kBodyH, t.face);
    if (!pane) return;
    if (!t.bevel) box(pane, 0, 0, 1, kBodyH, t.line);
    detail_icon_ = lv_image_create(pane);
    if (detail_icon_) {
        lv_obj_set_pos(detail_icon_, (pane_w - kDetailIcon) / 2, 6);
        lv_obj_set_size(detail_icon_, kDetailIcon, kDetailIcon);
        lv_image_set_inner_align(detail_icon_, LV_IMAGE_ALIGN_CENTER);
    }
    const lv_font_t *bold = bold_font();
    const lv_font_t *small = small_font();
    detail_name_ = text(pane, 6, 66, pane_w - 12, bold, t.text, "", LV_TEXT_ALIGN_CENTER);
    detail_sub_ = text(pane, 6, 66 + static_cast<int>(lv_font_get_line_height(bold)) + 1, pane_w - 12,
                       small, t.dim, "", LV_TEXT_ALIGN_CENTER);

    const int chip_w = 82, chip_h = 16;
    const int chip_x = (pane_w - chip_w) / 2, chip_y = kBodyH - chip_h - 3;
    const lv_font_t *chip_text = chip_font();
    if (t.bevel) {
        lv_obj_t *chip = box(pane, chip_x, chip_y, chip_w, chip_h, t.face);
        if (chip) {
            bevel_edge(chip, 0, 0, chip_w, chip_h, true);
            text(chip, 0, centred(chip_h, chip_text), chip_w, chip_text, t.text, "Enter open", LV_TEXT_ALIGN_CENTER);
        }
    } else {
        lv_obj_t *chip = box(pane, chip_x, chip_y, chip_w, chip_h, t.accent);
        if (chip) {
            if (t.gloss) gradient(chip, lighten(t.accent, 35), t.accent, false);
            lv_obj_set_style_radius(chip, t.radius, LV_PART_MAIN);
            text(chip, 0, centred(chip_h, chip_text), chip_w, chip_text, t.on_accent, "Enter open", LV_TEXT_ALIGN_CENTER);
        }
    }
}

void Ground0Home::build_grid()
{
    const ground0::Theme &t = ground0::theme();
    const lv_font_t *label_font = small_font();
    const bool outline = !t.bevel && !t.gloss;

    lv_obj_t *body = box(root_, 0, kBodyY, kScreenW, kBodyH, t.bg);
    if (body) draw_art(body, kScreenW, kBodyH);

    row_count_ = kGridCells;
    for (int i = 0; i < kGridCells; ++i) {
        Row &cell = rows_[i];
        const int x = (i % kGridCols) * kCellW;
        const int y = kBodyY + (i / kGridCols) * kCellH;
        cell.box = box(root_, x + 1, y + 1, kCellW - 2, kCellH - 2, t.sel);
        if (!cell.box) continue;
        selection_fill(cell.box);
        lv_obj_set_style_radius(cell.box, t.radius, LV_PART_MAIN);
        lv_obj_set_style_border_color(cell.box, hex(t.accent), LV_PART_MAIN);
        lv_obj_set_style_border_width(cell.box, outline ? 1 : 0, LV_PART_MAIN);
        cell.icon = lv_image_create(cell.box);
        if (cell.icon) {
            lv_obj_set_pos(cell.icon, (kCellW - 2 - kCellIcon) / 2, 3);
            lv_obj_set_size(cell.icon, kCellIcon, kCellIcon);
            lv_image_set_inner_align(cell.icon, LV_IMAGE_ALIGN_CENTER);
        }
        const int label_h = static_cast<int>(lv_font_get_line_height(label_font));
        cell.name = text(cell.box, 2, kCellH - 3 - label_h, kCellW - 6, label_font, t.text, "",
                         LV_TEXT_ALIGN_CENTER);
    }
    if (t.bevel) bevel_edge(root_, 0, kBodyY, kScreenW, kBodyH, false);
}

void Ground0Home::build_footer()
{
    const ground0::Theme &t = ground0::theme();
    lv_obj_t *foot = box(root_, 0, kScreenH - kFootH, kScreenW, kFootH, t.foot);
    if (!foot) return;
    gradient(foot, t.foot, t.foot_b, false);
    box(foot, 0, 0, kScreenW, 1, t.bevel ? t.hi : t.line);
    const lv_font_t *hint = load_font(t.font_text, t.small_px >= 14 ? 12 : t.small_px - 1);
    foot_label_ = text(foot, 6, centred(kFootH, hint) + 1, kScreenW - 12, hint, t.foot_text, "");
}

void Ground0Home::set_icon(lv_obj_t *image, const std::string &path, uint32_t size)
{
    if (!image) return;
    const lv_image_dsc_t *src = (lookup_ && !path.empty()) ? lookup_(path, size) : nullptr;
    lv_image_set_src(image, src);
}

void Ground0Home::bind()
{
    if (!root_) return;
    const std::size_t page = page_size();
    const std::size_t count = visible_.size();
    if (count == 0) {
        first_ = 0;
        selected_ = 0;
    } else if (view_ == View::Grid) {
        first_ = (selected_ / page) * page;
    } else {
        if (selected_ < first_) first_ = selected_;
        if (selected_ >= first_ + page) first_ = selected_ + 1 - page;
        if (first_ + page > count)
            first_ = count > page ? count - page : 0;
    }

    if (view_ == View::Grid)
        bind_grid();
    else
        bind_list();

    if (strip_left_) {
        const ground0::Theme &t = ground0::theme();
        char buffer[96];
        if (searching_)
            std::snprintf(buffer, sizeof(buffer), "> %s_", query_.c_str());
        else if (count == 0)
            std::snprintf(buffer, sizeof(buffer), "No apps");
        else
            std::snprintf(buffer, sizeof(buffer), "All apps  %zu of %zu", selected_ + 1, count);
        lv_label_set_text(strip_left_, buffer);
        // Search text takes the accent colour where that reads against the strip.
        const bool accent_reads = reads_on(t.accent, t.strip);
        lv_obj_set_style_text_color(
            strip_left_, hex(searching_ && accent_reads ? t.accent : t.strip_text), LV_PART_MAIN);
    }
    if (strip_right_) {
        const ground0::Theme &t = ground0::theme();
        if (searching_) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), count == 1 ? "%zu match" : "%zu matches", count);
            lv_label_set_text(strip_right_, buffer);
        } else {
            lv_label_set_text(strip_right_, t.name);
        }
    }
    if (foot_label_) {
        const char *hint = searching_
            ? "type to filter  Tab next  Enter open  Esc close"
            : view_ == View::Grid
                ? "F/X/Z/C move  Enter open  Space search  G list  T theme"
                : "F/X move  Z/C page  Enter  Space search  G grid  T theme";
        lv_label_set_text(foot_label_, hint);
    }
}

void Ground0Home::bind_list()
{
    const ground0::Theme &t = ground0::theme();
    for (int i = 0; i < row_count_; ++i) {
        Row &row = rows_[i];
        if (!row.box) continue;
        const std::size_t position = first_ + static_cast<std::size_t>(i);
        if (position >= visible_.size()) {
            lv_obj_add_flag(row.box, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(row.box, LV_OBJ_FLAG_HIDDEN);
        const std::size_t index = visible_[position];
        const bool on = position == selected_;
        lv_obj_set_style_bg_opa(row.box, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_opa(row.box, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        if (row.name) {
            lv_label_set_text(row.name, items_[index].name.c_str());
            lv_obj_set_style_text_color(row.name, hex(on ? t.sel_text : t.text), LV_PART_MAIN);
        }
        set_icon(row.icon, items_[index].icon, kRowIcon);
    }

    if (selected_ < visible_.size()) {
        const Ground0Item &item = items_[visible_[selected_]];
        set_icon(detail_icon_, item.icon, kDetailIcon);
        if (detail_name_) lv_label_set_text(detail_name_, item.name.c_str());
        if (detail_sub_) lv_label_set_text(detail_sub_, item.detail.c_str());
    } else {
        set_icon(detail_icon_, {}, kDetailIcon);
        if (detail_name_) lv_label_set_text(detail_name_, "");
        if (detail_sub_) lv_label_set_text(detail_sub_, "");
    }
}

void Ground0Home::bind_grid()
{
    const ground0::Theme &t = ground0::theme();
    for (int i = 0; i < row_count_; ++i) {
        Row &cell = rows_[i];
        if (!cell.box) continue;
        const std::size_t position = first_ + static_cast<std::size_t>(i);
        if (position >= visible_.size()) {
            lv_obj_add_flag(cell.box, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(cell.box, LV_OBJ_FLAG_HIDDEN);
        const std::size_t index = visible_[position];
        const bool on = position == selected_;
        lv_obj_set_style_bg_opa(cell.box, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_border_opa(cell.box, on ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
        if (cell.name) {
            lv_label_set_text(cell.name, items_[index].name.c_str());
            lv_obj_set_style_text_color(cell.name, hex(on ? t.sel_text : t.text), LV_PART_MAIN);
        }
        set_icon(cell.icon, items_[index].icon, kCellIcon);
    }
}

void Ground0Home::update_status()
{
    if (clock_) {
        char buffer[8] = "--:--";
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        if (localtime_r(&now, &local)) std::strftime(buffer, sizeof(buffer), "%H:%M", &local);
        lv_label_set_text(clock_, buffer);
    }
    if (battery_) {
        const cp0_battery_info_t battery = cp0_battery_read();
        char buffer[8] = "";
        if (battery.valid == 1) std::snprintf(buffer, sizeof(buffer), "%d%%", battery.soc);
        lv_label_set_text(battery_, buffer);
    }
}

void Ground0Home::move(int delta)
{
    if (visible_.empty()) return;
    const long count = static_cast<long>(visible_.size());
    long next = static_cast<long>(selected_) + delta;
    if (delta == 1 || delta == -1) {
        next = (next % count + count) % count;          // single steps wrap around
    } else {
        next = std::max(0L, std::min(count - 1, next)); // page and row jumps clamp
    }
    selected_ = static_cast<std::size_t>(next);
    bind();
}

Ground0Home::Action Ground0Home::handle_key(const struct key_item &item)
{
    if (!root_) return Action::None;
    if (searching_) return handle_search_key(item);

    const uint32_t code = item.semantic_key ? item.semantic_key : item.key_code;

    if (item.key_state == KBD_KEY_RELEASED)
        return code == KEY_ENTER && !visible_.empty() ? Action::Launch : Action::None;

    const bool grid = view_ == View::Grid;
    switch (code) {
    case KEY_UP:
        move(grid ? -kGridCols : -1);
        return Action::Handled;
    case KEY_DOWN:
        move(grid ? kGridCols : 1);
        return Action::Handled;
    case KEY_LEFT:
        move(grid ? -1 : -kListRows);
        return Action::Handled;
    case KEY_RIGHT:
        move(grid ? 1 : kListRows);
        return Action::Handled;
    default:
        break;
    }

    if (item.key_state != KBD_KEY_PRESSED) return Action::None;
    if (code == KEY_SPACE) {
        const std::size_t keep = selected();
        searching_ = true;
        query_.clear();
        refilter(keep);
        return Action::Handled;
    }
    if (code == KEY_G) {
        release_icons();
        view_ = grid ? View::List : View::Grid;
        first_ = 0;
        save_settings();
        rebuild();
        return Action::Handled;
    }
    if (code == KEY_T) {
        const int step = (item.mods & KBD_MOD_SHIFT) ? -1 : 1;
        ground0::set_theme(ground0::theme_index() + step);
        release_icons();
        save_settings();
        rebuild();
        return Action::Handled;
    }
    return Action::None;
}

Ground0Home::Action Ground0Home::handle_search_key(const struct key_item &item)
{
    // Search reads raw keys: in this mode F/X/Z/C are letters, not arrows.
    const uint32_t code = item.key_code;

    if (item.key_state == KBD_KEY_RELEASED) {
        if (code != KEY_ENTER || visible_.empty()) return Action::None;
        // Leave search with the chosen app selected, then let the owner launch it.
        const std::size_t chosen = selected();
        searching_ = false;
        query_.clear();
        refilter(chosen);
        return Action::Launch;
    }

    if (code == KEY_ESC && item.key_state == KBD_KEY_PRESSED) {
        const std::size_t keep = selected();
        searching_ = false;
        query_.clear();
        refilter(keep);
        return Action::Handled;
    }
    if (code == KEY_TAB) {
        move((item.mods & KBD_MOD_SHIFT) ? -1 : 1);
        return Action::Handled;
    }
    if (code == KEY_BACKSPACE) {
        if (query_.empty()) {
            const std::size_t keep = selected();
            searching_ = false;
            refilter(keep);
            return Action::Handled;
        }
        // Drop one whole UTF-8 character.
        while (!query_.empty() && (static_cast<unsigned char>(query_.back()) & 0xC0) == 0x80)
            query_.pop_back();
        if (!query_.empty()) query_.pop_back();
        refilter(0);
        return Action::Handled;
    }
    if (code == KEY_ENTER) return Action::Handled;

    const unsigned char first = static_cast<unsigned char>(item.utf8[0]);
    if (first >= 0x20 && first != 0x7F && !(item.mods & (KBD_MOD_CTRL | KBD_MOD_ALT)) &&
        query_.size() < 32) {
        query_ += item.utf8;
        refilter(0);
        return Action::Handled;
    }
    return Action::Handled;
}
