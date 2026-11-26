/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

// Ground0 home view: status bar, app list with detail pane, icon grid.
// It is a plain LVGL object tree drawn over the stock home page; the owner
// (UILaunchPage) feeds it the app list and key events and performs the launch.
#pragma once

#include "lvgl/lvgl.h"
#include "keyboard_input.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct Ground0Item
{
    std::string name;
    std::string icon;     // icon path as known to the icon pool
    std::string detail;   // one short line for the detail pane
};

class Ground0Home
{
public:
    // Returns a decoded icon at the requested square size. The pointer must
    // stay valid until release_icons() is called.
    using IconLookup = std::function<const lv_image_dsc_t *(const std::string &, uint32_t)>;

    enum class Action { None, Handled, Launch };
    enum class View { List, Grid };

    Ground0Home(lv_obj_t *parent, IconLookup lookup);
    ~Ground0Home();

    Ground0Home(const Ground0Home &) = delete;
    Ground0Home &operator=(const Ground0Home &) = delete;

    bool valid() const { return root_ != nullptr; }
    void set_items(std::vector<Ground0Item> items, std::size_t selected);
    void release_icons();
    // Index into the list passed to set_items().
    std::size_t selected() const;
    bool searching() const { return searching_; }
    Action handle_key(const struct key_item &item);
    // Re-read who holds the LoRa radio and repaint the status-bar chip.
    void refresh_radio();

private:
    static constexpr int kListRows = 6;
    static constexpr int kGridCols = 5;
    static constexpr int kGridRows = 2;
    static constexpr int kGridCells = kGridCols * kGridRows;

    struct Row
    {
        lv_obj_t *box = nullptr;
        lv_obj_t *name = nullptr;
        lv_obj_t *icon = nullptr;
    };

    void load_settings();
    void save_settings();
    void rebuild();
    void build_bar();
    void build_strip();
    void build_list();
    void build_grid();
    void build_footer();
    void bind();
    void bind_list();
    void bind_grid();
    void update_status();
    void paint_radio();
    void toggle_radio();
    void move(int delta);
    void refilter(std::size_t keep_item);
    Action handle_search_key(const struct key_item &item);
    void set_icon(lv_obj_t *image, const std::string &path, uint32_t size);
    std::size_t page_size() const;

    static void on_root_deleted(lv_event_t *event) noexcept;
    static void on_status_timer(lv_timer_t *timer) noexcept;
    static void on_radio_timer(lv_timer_t *timer) noexcept;

    lv_obj_t *parent_ = nullptr;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *clock_ = nullptr;
    lv_obj_t *battery_ = nullptr;
    lv_obj_t *strip_left_ = nullptr;
    lv_obj_t *strip_right_ = nullptr;
    lv_obj_t *foot_label_ = nullptr;
    lv_obj_t *radio_chip_ = nullptr;
    lv_obj_t *radio_label_ = nullptr;
    lv_timer_t *radio_timer_ = nullptr;
    uint32_t status_color_ = 0;
    int radio_polls_left_ = 0;
    bool radio_pending_ = false;
    bool radio_target_mesh_ = false;
    lv_obj_t *detail_icon_ = nullptr;
    lv_obj_t *detail_name_ = nullptr;
    lv_obj_t *detail_sub_ = nullptr;
    std::array<Row, kGridCells> rows_ = {};
    int row_count_ = 0;
    lv_timer_t *status_timer_ = nullptr;

    IconLookup lookup_;
    std::vector<Ground0Item> items_;
    std::vector<std::size_t> visible_;   // item indexes shown, in display order
    std::size_t selected_ = 0;   // position in visible_
    std::size_t first_ = 0;      // first shown position (list) or page start (grid)
    bool searching_ = false;
    std::string query_;
    View view_ = View::List;
};
