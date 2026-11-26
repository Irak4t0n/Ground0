/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "lvgl/lvgl.h"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class HomeIconBufferPool
{
public:
    static constexpr uint32_t kIconSize = 100;

    HomeIconBufferPool() = default;
    HomeIconBufferPool(const HomeIconBufferPool &) = delete;
    HomeIconBufferPool &operator=(const HomeIconBufferPool &) = delete;

    void rebuild(const std::vector<std::string> &icon_paths);
    const lv_image_dsc_t *find(const std::string &path) const;
    const lv_image_dsc_t *find(const std::string &path, uint32_t size);

    // Ground0: remember the icon paths without decoding anything, and decode an
    // icon at the asked size the first time it is shown. Saves the ~40 KB per
    // app that rebuild() spends on a 100 px copy of every icon.
    void rebuild_on_demand(const std::vector<std::string> &icon_paths);
    const lv_image_dsc_t *find_on_demand(const std::string &path, uint32_t size);
    std::size_t size() const { return icons_.size(); }
    void swap(HomeIconBufferPool &other) noexcept;

private:
    struct DrawBufferDeleter
    {
        void operator()(lv_draw_buf_t *buffer) const noexcept;
    };

    using DrawBufferPtr = std::unique_ptr<lv_draw_buf_t, DrawBufferDeleter>;

    static DrawBufferPtr decode_and_prepare(const std::string &path, uint32_t size);
    static const lv_image_dsc_t *as_image(const DrawBufferPtr &buffer);

    std::unordered_map<std::string, DrawBufferPtr> icons_;
    std::unordered_map<std::string, DrawBufferPtr> resized_icons_;
    std::unordered_set<std::string> known_paths_;
};
