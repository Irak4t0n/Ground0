/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_system.hpp"

#include "ui.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace ground0 {

namespace {

std::string desk_dir()
{
    if (const char *dir = std::getenv("GROUND0_DESK_DIR")) return dir;
    return "/var/lib/cz-desk-mode";
}

std::string slurp(const std::string &path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

std::string trimmed(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ')) value.pop_back();
    return value;
}

// KEY=VALUE files; later files override earlier ones.
std::string setting(const std::string &key, std::string value)
{
    for (const std::string &path : {std::string("/etc/default/cz-desk-mode"), desk_dir() + "/settings"}) {
        std::ifstream in(path);
        std::string line;
        while (std::getline(in, line)) {
            if (line.compare(0, key.size() + 1, key + "=") == 0) value = trimmed(line.substr(key.size() + 1));
        }
    }
    return value;
}

bool write_atomic(const std::string &path, const std::string &body)
{
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) return false;
        out << body;
        if (!out.good()) return false;
    }
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

bool hdmi_connected()
{
    if (const char *sim = std::getenv("GROUND0_HDMI_SIM")) return std::strcmp(sim, "1") == 0;
    DIR *dir = opendir("/sys/class/drm");
    if (!dir) return false;
    bool connected = false;
    while (const dirent *entry = readdir(dir)) {
        if (!std::strstr(entry->d_name, "-HDMI-A-")) continue;
        if (trimmed(slurp(std::string("/sys/class/drm/") + entry->d_name + "/status")) == "connected") connected = true;
    }
    closedir(dir);
    return connected;
}

std::string capture(const char *const *argv)
{
    char out[256] = "";
    cp0_process_capture_argv(argv, out, sizeof(out));
    return trimmed(out);
}

} // namespace

DeskState desk_state()
{
    DeskState state;
    struct stat info;
    state.installed = stat(desk_dir().c_str(), &info) == 0 && S_ISDIR(info.st_mode);
    if (!state.installed) return state;
    state.hdmi = hdmi_connected();
    const std::string status = slurp(desk_dir() + "/status");   // {"hdmi": .., "desktop": true, "keyboard": "desktop"}
    state.desktop = status.find("\"desktop\": true") != std::string::npos;
    state.keyboard_away = status.find("\"keyboard\": \"desktop\"") != std::string::npos;
    state.autostart = setting("AUTOSTART", "1") != "0";
    return state;
}

bool desk_request(const char *action)
{
    return write_atomic(desk_dir() + "/request", std::string(action) + "\n");
}

bool desk_set_autostart(bool on)
{
    // The daemon reads only these two keys; keep the other one as it is.
    const std::string body = "# Written by Ground0; overrides /etc/default/cz-desk-mode.\n"
                             "AUTOSTART=" + std::string(on ? "1" : "0") + "\n"
                             "KEYBOARD_ON_START=" + setting("KEYBOARD_ON_START", "ask") + "\n";
    return write_atomic(desk_dir() + "/settings", body);
}

Switch lean_boot()
{
    if (const char *sim = std::getenv("GROUND0_LEAN_SIM")) return std::strcmp(sim, "1") == 0 ? Switch::On : Switch::Off;
    const char *argv[] = {"systemctl", "is-enabled", "lightdm.service", nullptr};
    const std::string answer = capture(argv);
    if (answer == "enabled") return Switch::Off;
    if (answer == "disabled") return Switch::On;
    return Switch::Unknown;
}

void set_lean_boot(bool on)
{
    // Only the boot setting changes: a desktop that is already running stays up.
    const char *argv[] = {"sudo", "-n", "systemctl", on ? "disable" : "enable", "lightdm.service", nullptr};
    cp0_process_run_argv(argv, 1);
}

WifiState wifi_state()
{
    WifiState state;
    const char *radio[] = {"nmcli", "-t", "-f", "WIFI", "general", nullptr};
    const std::string answer = capture(radio);
    if (answer == "enabled") state.radio = Switch::On;
    if (answer == "disabled") state.radio = Switch::Off;
    if (state.radio == Switch::On) {
        const char *ssid[] = {"iwgetid", "-r", nullptr};
        state.ssid = capture(ssid);
    }
    return state;
}

void set_wifi(bool on)
{
    const char *argv[] = {"nmcli", "radio", "wifi", on ? "on" : "off", nullptr};
    cp0_process_run_argv(argv, 1);
}

} // namespace ground0
