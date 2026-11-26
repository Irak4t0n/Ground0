/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "ui_launch_page.h"

#include "ground0_radio.hpp"
#include "launch.h"
#include "ui_loading.h"

#include <chrono>

#include "hal_lvgl_bsp.h"
#include "input_keys.h"
#include "keyboard_input.h"
#include "sample_log.h"

#undef SLOGI
#define SLOGI(...) do { } while (0)

namespace {

// ============================================================
// audio
// ============================================================

static void audio_play_switch(void)
{
    cp0_signal_audio_api({"SystemSoundPlay", "1"}, nullptr);
}

static void audio_play_enter(void)
{
    cp0_signal_audio_api({"SystemSoundPlay", "2"}, nullptr);
}

static uint32_t fzxc_to_arrow(uint32_t key)
{
    switch (key)
    {
    case KEY_F:
        return KEY_UP;

    case KEY_X:
        return KEY_DOWN;

    case KEY_Z:
        return KEY_LEFT;

    case KEY_C:
        return KEY_RIGHT;

    default:
        return key;
    }
}

static UILaunchPage *page_from_event(lv_event_t *event)
{
    return event ? static_cast<UILaunchPage *>(lv_event_get_user_data(event)) : nullptr;
}

} // namespace

void UILaunchPage::handle_home_key(lv_event_t *event)
{
    if (!event)
        return;

    struct key_item *elm = static_cast<struct key_item *>(lv_event_get_param(event));
    if (!elm)
        return;

    /* Text editors own printable keys, including F/X/Z/C.  This callback is
     * installed on the active screen, so it must not let those events leak
     * into launcher navigation while an editor has selected text context. */
    if (elm->input_context == KBD_INPUT_CONTEXT_TEXT ||
        cp0_keyboard_get_input_context() == KBD_INPUT_CONTEXT_TEXT) {
        return;
    }

    if (ground0_ && ground0_->valid()) {
        if (ground0_->handle_key(*elm) == Ground0Home::Action::Launch && launch_) {
            launch_->select_app(ground0_->selected());
            audio_play_enter();

            // Hand the LoRa radio over if this app needs the mesh daemon stopped
            // (or running), and give it back when the app exits.
            ground0::Radio &radio = ground0::Radio::instance();
            const app *entry = launch_->app_at(ground0_->selected());
            const auto need = entry ? radio.need_for(entry->Name, entry->Exec) : ground0::Radio::Need::None;
            bool restore_mesh = false;
            if (need != ground0::Radio::Need::None && radio.poll() != ground0::RadioState::Absent) {
                const bool want_mesh = need == ground0::Radio::Need::Mesh;
                if ((radio.state() == ground0::RadioState::Mesh) != want_mesh) {
                    ui_loading::show(want_mesh ? "Starting MeshZero..." : "Freeing LoRa radio...");
                    lv_refr_now(nullptr);
                    radio.set_blocking(want_mesh);
                    ui_loading::hide();
                    restore_mesh = !want_mesh && radio.restore_after_app();
                }
            }

            const auto started = std::chrono::steady_clock::now();
            launch_selected_app();
            // An external app blocks until it exits. A call that returns at once
            // opened a built-in page or terminal instead, which is still running,
            // so the radio stays as it is.
            const bool app_has_exited =
                std::chrono::steady_clock::now() - started > std::chrono::seconds(2);
            const bool restoring = restore_mesh && app_has_exited;
            if (restoring) radio.request(true);
            if (ground0_ && ground0_->valid()) {
                // The daemon takes a moment to come up: show it as pending, not FREE.
                if (restoring)
                    ground0_->expect_radio(true);
                else
                    ground0_->refresh_radio();
            }
        }
        return;
    }

    uint32_t code = elm->semantic_key ? elm->semantic_key : fzxc_to_arrow(elm->key_code);

    SLOGI("[LAUNCHER] main_key_switch raw=%u->code=%u state=%s sym=%s",
           elm->key_code,
           code,
           kbd_state_name(elm->key_state),
           elm->sym_name);

    if (elm->key_state)
    {
        switch (code)
        {
        case KEY_UP:
            break;

        case KEY_DOWN:
            break;

        case KEY_LEFT:
        {
            if (navigation_.keyboard_navigation_enabled())
            {
                audio_play_switch();
                switch_right();
            }
        }
        break;

        case KEY_RIGHT:
        {
            if (navigation_.keyboard_navigation_enabled())
            {
                audio_play_switch();
                switch_left();
            }
        }
        break;

        default:
            break;
        }
    }
    else if (code == KEY_ENTER)
    {
        audio_play_enter();
        launch_selected_app();
    }
}

void UILaunchPage::on_left_arrow_clicked(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->switch_right();
    } catch (...) {
    }
}

void UILaunchPage::on_right_arrow_clicked(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->switch_left();
    } catch (...) {
    }
}

void UILaunchPage::on_app_clicked(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->launch_selected_app();
    } catch (...) {
    }
}

void UILaunchPage::on_home_key(lv_event_t *event) noexcept
{
    try {
        if (UILaunchPage *self = page_from_event(event)) self->handle_home_key(event);
    } catch (...) {
    }
}
