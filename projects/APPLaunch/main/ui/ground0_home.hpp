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
#include "ground0_system.hpp"

#include <string>
#include <unordered_map>
#include <vector>

struct Ground0Item
{
    std::string name;
    std::string icon;     // icon path as known to the icon pool
    std::string detail;   // one short line for the detail pane
    std::string exec;     // Exec line, the key into the app index
    // Filled from the app index (see ground0-helper.py):
    std::string category;
    std::string uuid;
    std::string have;     // installed version
    std::string next;     // newer version available, or empty
    int command = -1;     // >= 0: a Ground0 command shown in search, not an app
};

class Ground0Home
{
public:
    // Returns a decoded icon at the requested square size. The pointer must
    // stay valid until release_icons() is called.
    using IconLookup = std::function<const lv_image_dsc_t *(const std::string &, uint32_t)>;

    enum class Action { None, Handled, Launch };
    enum class View { List, Grid };
    enum class TabKind { Favs, All, Recent, Updates, Category };

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
    // A start or stop was requested elsewhere: show it as pending until it lands.
    void expect_radio(bool mesh_on);

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
        lv_obj_t *mark = nullptr;   // favourite marker
        lv_obj_t *badge = nullptr;  // update available
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
    void switch_tab(int step);
    void rebuild_tabs(const std::string &keep_label);
    TabKind tab_kind() const;
    const std::string &tab_label() const;
    void load_index();
    void apply_index();
    void request_index();
    void start_update(const std::vector<std::string> &uuids);
    void finish_update(const std::string &message);
    void toggle_favorite();
    void note_launch();
    bool is_favorite(const std::string &name) const;
    Action handle_search_key(const struct key_item &item);
    void set_icon(lv_obj_t *image, const std::string &path, uint32_t size);
    std::size_t page_size() const;

    static void on_root_deleted(lv_event_t *event) noexcept;
    static void on_status_timer(lv_timer_t *timer) noexcept;
    static void on_radio_timer(lv_timer_t *timer) noexcept;
    static void on_update_timer(lv_timer_t *timer) noexcept;
    static void on_quick_timer(lv_timer_t *timer) noexcept;

    // Quick panel: radio, HDMI desk, lean boot, Wi-Fi.
    void run_command(int command);
    bool grid_now() const;
    void set_searching(bool on);
    void open_quick();
    void close_quick();
    void build_quick();
    void paint_quick();
    void read_quick(bool slow);
    void act_quick();
    Action handle_quick_key(const struct key_item &item);

    lv_obj_t *parent_ = nullptr;
    lv_obj_t *root_ = nullptr;
    lv_obj_t *clock_ = nullptr;
    lv_obj_t *battery_ = nullptr;
    lv_obj_t *strip_left_ = nullptr;
    lv_obj_t *strip_right_ = nullptr;
    lv_obj_t *foot_label_ = nullptr;
    lv_obj_t *chip_label_ = nullptr;

    static constexpr int kQuickRows = 7;
    lv_obj_t *quick_panel_ = nullptr;
    lv_obj_t *quick_row_[kQuickRows] = {};
    lv_obj_t *quick_name_[kQuickRows] = {};
    lv_obj_t *quick_value_[kQuickRows] = {};
    lv_timer_t *quick_timer_ = nullptr;
    bool quick_open_ = false;
    int quick_selected_ = 0;
    int quick_slow_in_ = 0;         // ticks until lean boot and Wi-Fi are read again
    ground0::DeskState desk_;
    ground0::Switch lean_ = ground0::Switch::Unknown;
    ground0::WifiState wifi_;
    ground0::DongleState dongle_;
    int dongle_nudges_ = 0;         // sync requests sent for the current mismatch
    bool keyboard_away_ = false;    // the keyboard is typing on the HDMI desktop
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
    struct TabDef
    {
        TabKind kind;
        std::string label;
    };
    struct IndexEntry
    {
        std::string category, uuid, have, next;
    };
    std::vector<TabDef> tabs_;
    std::size_t tab_index_ = 0;
    std::string saved_tab_ = "All apps";
    std::unordered_map<std::string, IndexEntry> index_;   // by Exec line
    long index_stamp_ = 0;          // modification time of the index file last loaded
    lv_timer_t *update_timer_ = nullptr;
    bool updating_ = false;
    int update_ticks_ = 0;
    std::vector<std::string> favorites_;   // app names
    std::vector<std::string> recents_;     // app names, newest first
    std::string query_;
    View view_ = View::List;
};
