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
            if (need != ground0::Radio::Need::None && radio.poll() != ground0::RadioState::Absent) {
                const bool want_mesh = need == ground0::Radio::Need::Mesh;
                if ((radio.state() == ground0::RadioState::Mesh) != want_mesh) {
                    ui_loading::show(want_mesh ? "Starting MeshZero..." : "Freeing LoRa radio...");
                    lv_refr_now(nullptr);
                    radio.set_blocking(want_mesh);
                    ui_loading::hide();
                    if (!want_mesh && radio.restore_after_app()) radio.set_owed(true);
                }
            }

            // Apps that take the USB Wi-Fi adapter for themselves refuse one that
            // is carrying a connection: leave it idle first (Wi-Fi goes back to
            // the built-in radio) and take it back when the launcher is home.
            if (entry && ground0::dongle_app(entry->Name, entry->Exec)) {
                const ground0::DongleState dongle = ground0::dongle_state();
                if (dongle.available) {
                    ground0::set_dongle_owed(true);
                    // With no adapter plugged in this only leaves a note for one
                    // that is plugged in later, and takes a moment: no message.
                    const bool show = !dongle.adapter.empty();
                    if (show) {
                        ui_loading::show("Freeing Wi-Fi dongle...");
                        lv_refr_now(nullptr);
                    }
                    ground0::dongle_lend();
                    if (show) ui_loading::hide();
                }
            }

            launch_selected_app();
            // Whenever the launcher is back on its home screen (the app exited,
            // failed to start, or its page was closed) show_home_screen() has
            // already given the radio back; see settle_radio_debt().
            if (ground0_ && ground0_->valid()) ground0_->resume();
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

// Ground0 stopped the mesh daemon for an app that needed the radio free. Now
// that the launcher is home again, start it. Called from show_home_screen(),
// which every way back home goes through: an external app exiting (however
// quickly), a launch that failed, a terminal or built-in page being closed.
void UILaunchPage::settle_dongle_debt()
{
    if (!ground0::dongle_owed()) return;
    ground0::set_dongle_owed(false);
    ground0::dongle_return();
}

void UILaunchPage::settle_radio_debt()
{
    ground0::Radio &radio = ground0::Radio::instance();
    if (!radio.owed()) return;
    radio.set_owed(false);
    if (radio.poll() != ground0::RadioState::Free) return;
    radio.request(true);
    if (ground0_ && ground0_->valid()) ground0_->expect_radio(true);
}
