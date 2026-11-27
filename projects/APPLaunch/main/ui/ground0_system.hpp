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

#include "ground0_apps.hpp"

#include <chrono>
#include <string>
#include <sys/types.h>
#include <vector>

namespace ground0 {

// A command whose output is collected without making the caller wait: start()
// it, then ask done() from a timer. The status reads go through this, so a slow
// or stuck system command can never freeze the screen. A command that runs past
// its time limit is stopped and counts as having printed nothing.
class Probe
{
public:
    Probe() = default;
    ~Probe();
    Probe(const Probe &) = delete;
    Probe &operator=(const Probe &) = delete;

    bool start(const char *const *argv, int timeout_ms = 4000);
    bool running() const { return pid_ > 0; }
    // True once per run, when the command has ended; `out` is what it printed.
    bool done(std::string &out);
    // The same, waited for (never longer than the time limit).
    std::string wait();

private:
    pid_t pid_ = -1;
    int fd_ = -1;
    std::string text_;
    std::chrono::steady_clock::time_point deadline_;
};

// Run a command and wait for what it prints, for at most `timeout_ms`.
std::string capture_now(const char *const *argv, int timeout_ms = 3000);

// Start a command and return at once, with its output discarded. The launcher's
// own background runner never collects its children, which leaves a defunct
// process behind for each one; these are collected by reap_background().
void run_background(const char *const *argv);
void reap_background();

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

// Lean boot and Wi-Fi each take a command to read (about 50 and 100 ms on a
// Cardputer Zero). The quick panel reads both in the background through this.
class QuickReads
{
public:
    void start();                                   // no effect while a read is running
    bool running() const { return lean_wanted_ || wifi_wanted_; }
    bool poll(Switch &lean, WifiState &wifi);       // true when a value was stored
    void finish(Switch &lean, WifiState &wifi);     // wait for the answers
private:
    Probe lean_;
    Probe wifi_;
    bool lean_wanted_ = false;
    bool wifi_wanted_ = false;
};

// Wi-Fi follows a USB adapter: while one is plugged in and in normal mode the
// network moves to it and the built-in radio disconnects (see
// ground0/packaging/files/ground0-wifi). Read from sysfs and two small files,
// so it is cheap enough for the status tick.
struct DongleState
{
    bool available = false;   // the helper is installed
    bool enabled = false;     // the behaviour is switched on
    std::string adapter;      // USB Wi-Fi interface name, empty when none is plugged in
    bool monitor = false;     // an app has the adapter in monitor mode
    bool moved = false;       // Wi-Fi is on the adapter now
    bool testing = false;     // the adapter is on trial beside the built-in radio
    bool failed = false;      // the adapter did not hold a connection; left alone until asked again
    bool lent = false;        // left idle for an app that wants the adapter for itself
    bool settled() const;     // nothing for the helper to do
};
DongleState dongle_state();
void set_dongle_follow(bool on);
void dongle_sync();

// The app with this Exec line, looked up in the launcher's applications folder.
AppEntry app_entry(const std::string &exec);

// Apps that take a Wi-Fi adapter for themselves (monitor mode) and refuse one
// that is carrying a connection. Before one starts the adapter is left idle and
// Wi-Fi goes back to the built-in radio; when the launcher is home again the
// adapter is taken back. An app is one of these when its .desktop file says
// X-Ground0-WifiAdapter=idle, or when it is in the "dongle_apps" setting.
bool dongle_app(const std::string &name, const std::string &exec);
void dongle_lend();           // waits until the adapter is idle (about half a second)
void dongle_return();
void set_dongle_owed(bool owed);
bool dongle_owed();

} // namespace ground0
