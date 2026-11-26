/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

// Ground0 system switches behind the quick panel: the HDMI desktop, the
// built-in keyboard handover, lean boot and Wi-Fi. No LVGL in here.
//
// HDMI desk: Ground0 drives the Desk Mode daemon (cz-desk-mode, MIT, by
// InstantComet) through its own files in /var/lib/cz-desk-mode: it reads
// "status" and "settings" and asks for actions by writing "request".
// Lean boot: the background Raspberry Pi desktop (LightDM) is not started at
// boot; the HDMI desktop still starts on demand through the daemon.
#pragma once

#include <string>

namespace ground0 {

enum class Switch { Unknown, On, Off };

struct DeskState
{
    bool installed = false;      // the daemon's state directory exists
    bool hdmi = false;           // a display is plugged in
    bool desktop = false;        // the HDMI desktop is running
    bool keyboard_away = false;  // the built-in keyboard is typing on the HDMI desktop
    bool autostart = true;       // start the desktop when HDMI is plugged in
};

DeskState desk_state();
// "desktop-start", "desktop-stop", "keyboard-desktop", "keyboard-cardputer"
bool desk_request(const char *action);
bool desk_set_autostart(bool on);

Switch lean_boot();
void set_lean_boot(bool on);

struct WifiState
{
    Switch radio = Switch::Unknown;
    std::string ssid;            // empty when not connected
};
WifiState wifi_state();
void set_wifi(bool on);

} // namespace ground0
