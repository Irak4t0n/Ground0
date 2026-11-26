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
#include <ctime>
#include <string>
#include <utility>

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

const lv_font_t *font_or_default(lv_font_t *font)
{
    return font ? font : LV_FONT_DEFAULT;
}

const lv_font_t *text_font(uint16_t size)
{
    return font_or_default(launcher_fonts().get("JetBrainsMono-Regular.ttf", size));
}

const lv_font_t *bold_font(uint16_t size)
{
    return font_or_default(
        launcher_fonts().get("JetBrainsMono-Bold.ttf", size, LV_FREETYPE_FONT_STYLE_BOLD));
}

const lv_font_t *logo_font()
{
    return font_or_default(
        launcher_fonts().get("Montserrat-Bold.ttf", 14, LV_FREETYPE_FONT_STYLE_BOLD));
}

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
    root_ = box(parent_, 0, 0, kScreenW, kScreenH, ground0::theme().bg);
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

    lv_obj_set_style_bg_color(root_, hex(ground0::theme().bg), LV_PART_MAIN);
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
    lv_obj_t *bar = box(root_, 0, 0, kScreenW, kBarH, t.bar);
    if (!bar) return;
    box(bar, 0, kBarH - 1, kScreenW, 1, t.line);

    const lv_font_t *logo = logo_font();
    lv_obj_t *word = text(bar, 6, 2, 0, logo, t.bar_text, "GROUND");
    lv_obj_t *zero = text(bar, 6, 2, 0, logo, t.bar == t.accent ? t.bar_text : t.accent, "0");
    if (word && zero) {
        lv_obj_update_layout(word);
        lv_obj_set_x(zero, 6 + lv_obj_get_width(word));
    }

    const lv_font_t *small = bold_font(11);
    clock_ = text(bar, kScreenW - 42, 4, 36, small, t.bar_text, "--:--", LV_TEXT_ALIGN_RIGHT);
    battery_ = text(bar, kScreenW - 82, 4, 36, small, t.bar_text, "", LV_TEXT_ALIGN_RIGHT);
}

void Ground0Home::build_strip()
{
    const ground0::Theme &t = ground0::theme();
    lv_obj_t *strip = box(root_, 0, kBarH, kScreenW, kStripH, t.face);
    if (!strip) return;
    box(strip, 0, kStripH - 1, kScreenW, 1, t.line);
    const lv_font_t *small = text_font(10);
    // Text on `face`: light skins use black-ish text, dark themes use `dim`.
    strip_left_ = text(strip, 6, 2, 150, small, t.dim, "");
    strip_right_ = text(strip, kScreenW - 156, 2, 150, small, t.dim, t.name, LV_TEXT_ALIGN_RIGHT);
}

void Ground0Home::build_list()
{
    const ground0::Theme &t = ground0::theme();
    const lv_font_t *name_font = text_font(12);
    row_count_ = kListRows;
    for (int i = 0; i < kListRows; ++i) {
        Row &row = rows_[i];
        row.box = box(root_, 0, kBodyY + i * kRowH, kListW, kRowH, t.sel);
        if (!row.box) continue;
        lv_obj_set_style_radius(row.box, t.radius, LV_PART_MAIN);
        lv_obj_set_style_border_color(row.box, hex(t.accent), LV_PART_MAIN);
        row.name = text(row.box, 6, 3, kListW - kRowIcon - 16, name_font, t.text, "");
        row.icon = lv_image_create(row.box);
        if (row.icon) {
            lv_obj_set_pos(row.icon, kListW - kRowIcon - 5, (kRowH - kRowIcon) / 2);
            lv_obj_set_size(row.icon, kRowIcon, kRowIcon);
            lv_image_set_inner_align(row.icon, LV_IMAGE_ALIGN_CENTER);
        }
    }

    const int pane_x = kListW;
    const int pane_w = kScreenW - kListW;
    lv_obj_t *pane = box(root_, pane_x, kBodyY, pane_w, kBodyH, t.face);
    if (!pane) return;
    box(pane, 0, 0, 1, kBodyH, t.line);
    detail_icon_ = lv_image_create(pane);
    if (detail_icon_) {
        lv_obj_set_pos(detail_icon_, (pane_w - kDetailIcon) / 2, 8);
        lv_obj_set_size(detail_icon_, kDetailIcon, kDetailIcon);
        lv_image_set_inner_align(detail_icon_, LV_IMAGE_ALIGN_CENTER);
    }
    detail_name_ = text(pane, 6, 70, pane_w - 12, bold_font(12), t.text, "", LV_TEXT_ALIGN_CENTER);
    detail_sub_ = text(pane, 6, 87, pane_w - 12, text_font(10), t.dim, "", LV_TEXT_ALIGN_CENTER);

    lv_obj_t *chip = box(pane, (pane_w - 78) / 2, 102, 78, 15, t.accent);
    if (chip) {
        lv_obj_set_style_radius(chip, t.radius, LV_PART_MAIN);
        text(chip, 0, 2, 78, bold_font(10), t.on_accent, "Enter open", LV_TEXT_ALIGN_CENTER);
    }
}

void Ground0Home::build_grid()
{
    const ground0::Theme &t = ground0::theme();
    const lv_font_t *label_font = text_font(10);
    row_count_ = kGridCells;
    for (int i = 0; i < kGridCells; ++i) {
        Row &cell = rows_[i];
        const int x = (i % kGridCols) * kCellW;
        const int y = kBodyY + (i / kGridCols) * kCellH;
        cell.box = box(root_, x + 1, y + 1, kCellW - 2, kCellH - 2, t.sel);
        if (!cell.box) continue;
        lv_obj_set_style_radius(cell.box, t.radius, LV_PART_MAIN);
        lv_obj_set_style_border_color(cell.box, hex(t.accent), LV_PART_MAIN);
        cell.icon = lv_image_create(cell.box);
        if (cell.icon) {
            lv_obj_set_pos(cell.icon, (kCellW - 2 - kCellIcon) / 2, 4);
            lv_obj_set_size(cell.icon, kCellIcon, kCellIcon);
            lv_image_set_inner_align(cell.icon, LV_IMAGE_ALIGN_CENTER);
        }
        cell.name = text(cell.box, 2, 43, kCellW - 6, label_font, t.text, "", LV_TEXT_ALIGN_CENTER);
    }
}

void Ground0Home::build_footer()
{
    const ground0::Theme &t = ground0::theme();
    lv_obj_t *foot = box(root_, 0, kScreenH - kFootH, kScreenW, kFootH, t.face);
    if (!foot) return;
    box(foot, 0, 0, kScreenW, 1, t.line);
    foot_label_ = text(foot, 6, 2, kScreenW - 12, text_font(9), t.dim, "");
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
        lv_obj_set_style_text_color(strip_left_, hex(searching_ ? t.accent : t.dim), LV_PART_MAIN);
    }
    if (strip_right_) {
        if (searching_) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), count == 1 ? "%zu match" : "%zu matches", count);
            lv_label_set_text(strip_right_, buffer);
        } else {
            lv_label_set_text(strip_right_, ground0::theme().name);
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
        lv_obj_set_style_border_width(row.box, on ? 1 : 0, LV_PART_MAIN);
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
        lv_obj_set_style_border_width(cell.box, on ? 1 : 0, LV_PART_MAIN);
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
