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
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace ground0 {

namespace {

std::vector<pid_t> &children()
{
    static std::vector<pid_t> list;
    return list;
}

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

void run_background(const char *const *argv)
{
    if (!argv || !argv[0]) return;
    reap_background();
    const pid_t pid = fork();
    if (pid == 0) {
        const int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            dup2(null, STDOUT_FILENO);
            dup2(null, STDERR_FILENO);
            if (null > STDERR_FILENO) close(null);
        }
        execvp(argv[0], const_cast<char *const *>(argv));
        _exit(127);
    }
    if (pid > 0) children().push_back(pid);
}

void reap_background()
{
    std::vector<pid_t> &list = children();
    for (std::size_t i = 0; i < list.size();) {
        // Only our own children are waited for, by number, so nothing else in
        // the launcher loses the exit status of a process it started.
        const pid_t done = waitpid(list[i], nullptr, WNOHANG);
        if (done != 0)
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
        else
            ++i;
    }
}

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
    run_background(argv);
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

std::string wifi_helper()
{
    if (const char *path = std::getenv("GROUND0_WIFI_HELPER")) return path;
    return "/usr/lib/ground0/ground0-wifi";
}

// Runs the root helper with one fixed word. Returns false when it is not installed.
bool run_wifi_helper(const char *word)
{
    const std::string helper = wifi_helper();
    if (access(helper.c_str(), X_OK) != 0) return false;
    const char *argv[] = {"sudo", "-n", helper.c_str(), word, nullptr};
    run_background(argv);
    return true;
}

void set_wifi(bool on)
{
    // The launcher's own session may not be allowed to switch the radio; the helper is.
    if (run_wifi_helper(on ? "radio-on" : "radio-off")) return;
    const char *argv[] = {"nmcli", "radio", "wifi", on ? "on" : "off", nullptr};
    run_background(argv);
}

bool DongleState::settled() const
{
    if (!available || failed || testing) return true;
    const bool want = enabled && !adapter.empty() && !monitor;
    return want == moved;
}

DongleState dongle_state()
{
    DongleState state;
    if (const char *sim = std::getenv("GROUND0_DONGLE_SIM")) {   // "off", "none", "moved", "monitor", "testing", "failed"
        // A path instead of a word: the word is read from that file, so a test can change it live.
        const std::string mode = sim[0] == '/' ? trimmed(slurp(sim)) : std::string(sim);
        state.available = true;
        state.enabled = mode != "off";
        if (mode != "off" && mode != "none") state.adapter = "wlan1";
        state.testing = mode == "testing";
        state.failed = mode == "failed";
        state.monitor = mode == "monitor";
        state.moved = mode == "moved";
        return state;
    }
    state.available = access(wifi_helper().c_str(), X_OK) == 0;
    if (!state.available) return state;
    state.enabled = access("/etc/ground0/wifi-follow", F_OK) == 0;
    state.moved = access("/run/ground0-wifi", F_OK) == 0;
    state.failed = access("/run/ground0-wifi-failed", F_OK) == 0;
    state.testing = access("/run/ground0-wifi-trial", F_OK) == 0;
    DIR *dir = opendir("/sys/class/net");
    if (!dir) return state;
    while (const dirent *entry = readdir(dir)) {
        const std::string base = std::string("/sys/class/net/") + entry->d_name;
        if (entry->d_name[0] == '.' || access((base + "/phy80211").c_str(), F_OK) != 0) continue;
        char target[256] = "";
        const ssize_t length = readlink((base + "/device/subsystem").c_str(), target, sizeof(target) - 1);
        if (length <= 0) continue;
        target[length] = 0;
        const char *slash = std::strrchr(target, '/');
        if (std::strcmp(slash ? slash + 1 : target, "usb") != 0) continue;
        state.adapter = entry->d_name;
        state.monitor = trimmed(slurp(base + "/type")) != "1";
        break;
    }
    closedir(dir);
    return state;
}

void set_dongle_follow(bool on)
{
    run_wifi_helper(on ? "on" : "off");
}

void dongle_sync()
{
    run_wifi_helper("sync");
}

} // namespace ground0
