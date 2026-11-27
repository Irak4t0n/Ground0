/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_system.hpp"

#include "cp0_lvgl_app.h"
#include "ground0_settings.hpp"
#include "launcher_platform.hpp"
#include "ui.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <csignal>
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
#include <unordered_map>
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

const char *const kLeanArgv[] = {"systemctl", "is-enabled", "lightdm.service", nullptr};
// Two lines: the radio switch, then the network name (empty when not connected).
const char *const kWifiArgv[] = {"sh", "-c", "nmcli -t -f WIFI general; iwgetid -r", nullptr};

Switch lean_from(const std::string &answer)
{
    if (answer == "enabled") return Switch::Off;
    if (answer == "disabled") return Switch::On;
    return Switch::Unknown;
}

WifiState wifi_from(const std::string &answer)
{
    WifiState state;
    const std::size_t end = answer.find('\n');
    const std::string radio = trimmed(answer.substr(0, end));
    if (radio == "enabled") state.radio = Switch::On;
    if (radio == "disabled") state.radio = Switch::Off;
    if (state.radio == Switch::On && end != std::string::npos) state.ssid = trimmed(answer.substr(end + 1));
    return state;
}

} // namespace

Probe::~Probe()
{
    if (fd_ >= 0) close(fd_);
    if (pid_ > 0) {
        kill(-pid_, SIGKILL);
        waitpid(pid_, nullptr, 0);
    }
}

bool Probe::start(const char *const *argv, int timeout_ms)
{
    if (!argv || !argv[0] || pid_ > 0) return false;
    // The answers are read as text, so the commands must print them in English
    // whatever language the system is set to. Built here: between fork and exec
    // nothing may allocate memory.
    std::vector<std::string> variables;
    for (char **entry = environ; entry && *entry; ++entry)
        if (std::strncmp(*entry, "LC_ALL=", 7) != 0 && std::strncmp(*entry, "LANGUAGE=", 9) != 0)
            variables.emplace_back(*entry);
    variables.emplace_back("LC_ALL=C");
    std::vector<char *> environment;
    for (std::string &variable : variables) environment.push_back(&variable[0]);
    environment.push_back(nullptr);

    int ends[2];
    if (pipe2(ends, O_CLOEXEC) != 0) return false;
    const pid_t pid = fork();
    if (pid < 0) {
        close(ends[0]);
        close(ends[1]);
        return false;
    }
    if (pid == 0) {
        setpgid(0, 0);      // its own group, so stopping it also stops what it started
        const int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
            dup2(null, STDERR_FILENO);
        }
        dup2(ends[1], STDOUT_FILENO);
        execvpe(argv[0], const_cast<char *const *>(argv), environment.data());
        _exit(127);
    }
    setpgid(pid, pid);  // also from this side: whichever runs first wins
    close(ends[1]);
    fcntl(ends[0], F_SETFL, fcntl(ends[0], F_GETFL) | O_NONBLOCK);   // our end only
    fd_ = ends[0];
    pid_ = pid;
    text_.clear();
    deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    return true;
}

bool Probe::done(std::string &out)
{
    if (pid_ <= 0) return false;
    bool closed = false;
    char chunk[256];
    for (;;) {
        const ssize_t count = read(fd_, chunk, sizeof(chunk));
        if (count > 0) {
            if (text_.size() < 4096) text_.append(chunk, static_cast<std::size_t>(count));
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        closed = count == 0 || (errno != EAGAIN && errno != EWOULDBLOCK);
        break;
    }
    const bool late = std::chrono::steady_clock::now() >= deadline_;
    if (!closed && !late) return false;
    bool gone = closed && waitpid(pid_, nullptr, WNOHANG) != 0;
    if (!gone && !late) return false;              // it closed its output and is about to exit
    if (!gone) {
        // Over its time limit: stop it, and let the background list collect it.
        kill(-pid_, SIGKILL);
        children().push_back(pid_);
        text_.clear();
    }
    close(fd_);
    fd_ = -1;
    pid_ = -1;
    out = trimmed(text_);
    return true;
}

std::string Probe::wait()
{
    std::string out;
    if (pid_ <= 0) return out;
    while (!done(out)) usleep(3000);
    return out;
}

std::string capture_now(const char *const *argv, int timeout_ms)
{
    Probe probe;
    if (!probe.start(argv, timeout_ms)) return {};
    return probe.wait();
}

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
    // {"hdmi": .., "desktop": true, "keyboard": "desktop"}; compared without
    // blanks, so a daemon update that writes its JSON differently still reads.
    std::string status = slurp(desk_dir() + "/status");
    status.erase(std::remove_if(status.begin(), status.end(), [](unsigned char c) { return std::isspace(c); }),
                 status.end());
    state.desktop = status.find("\"desktop\":true") != std::string::npos;
    state.keyboard_away = status.find("\"keyboard\":\"desktop\"") != std::string::npos;
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
    return lean_from(capture_now(kLeanArgv));
}

void set_lean_boot(bool on)
{
    // Only the boot setting changes: a desktop that is already running stays up.
    const char *argv[] = {"sudo", "-n", "systemctl", on ? "disable" : "enable", "lightdm.service", nullptr};
    run_background(argv);
}

WifiState wifi_state()
{
    return wifi_from(capture_now(kWifiArgv));
}

void QuickReads::start()
{
    if (running()) return;
    // The simulated lean-boot value needs no command; poll() hands it over.
    lean_wanted_ = std::getenv("GROUND0_LEAN_SIM") != nullptr || lean_.start(kLeanArgv);
    wifi_wanted_ = wifi_.start(kWifiArgv);
}

bool QuickReads::poll(Switch &lean, WifiState &wifi)
{
    bool changed = false;
    std::string answer;
    if (lean_wanted_ && !lean_.running()) {
        lean = lean_boot();
        lean_wanted_ = false;
        changed = true;
    } else if (lean_wanted_ && lean_.done(answer)) {
        lean = lean_from(answer);
        lean_wanted_ = false;
        changed = true;
    }
    if (wifi_wanted_ && wifi_.done(answer)) {
        wifi = wifi_from(answer);
        wifi_wanted_ = false;
        changed = true;
    }
    return changed;
}

void QuickReads::finish(Switch &lean, WifiState &wifi)
{
    while (running()) {
        if (!poll(lean, wifi)) usleep(3000);
    }
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
    if (!available || failed || testing || lent) return true;
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
    state.lent = access("/run/ground0-wifi-lent", F_OK) == 0;
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

AppEntry app_entry(const std::string &exec)
{
    const char *override_dir = std::getenv("GROUND0_APPS_DIR");
    const std::string folder = override_dir ? override_dir : launcher_platform::path("applications");
    // Looking an app up reads every .desktop file (about 130 on a full device),
    // so answers are kept until the folder changes: installing, removing or
    // updating an app replaces a file there, which changes the folder's date.
    static std::string known_folder;
    static long long known_stamp = -1;
    static std::unordered_map<std::string, AppEntry> known;
    struct stat info;
    const long long stamp = stat(folder.c_str(), &info) == 0
                                ? static_cast<long long>(info.st_mtim.tv_sec) * 1000000000LL + info.st_mtim.tv_nsec
                                : 0;
    if (folder != known_folder || stamp != known_stamp) {
        known.clear();
        known_folder = folder;
        known_stamp = stamp;
    }
    const auto found = known.find(exec);
    if (found != known.end()) return found->second;
    const AppEntry entry = app_entry_in(folder, exec);
    known.emplace(exec, entry);
    return entry;
}

bool settings::legacy_lookup(const std::string &key, std::string &value)
{
    // The shared config hands back the fallback it is given when a key is not
    // set; this one cannot be a stored value.
    const std::string unset = "\x01unset";
    std::string found = unset;
    cp0_signal_config_api({"GetStr", key, unset},
                          [&](int code, std::string data) { if (code == 0) found = std::move(data); });
    if (found == unset) return false;
    value = found;
    return true;
}

bool dongle_app(const std::string &name, const std::string &exec)
{
    const AppEntry entry = app_entry(exec);
    if (entry.wifi == "idle") return true;
    if (entry.wifi == "none") return false;
    return listed(app_keys(name, exec, entry.id),
                  settings::get("dongle_apps",
                                "marauder,marauder_cardputerzero,zerogotchi,lokicz,lokictl,quartermaster,"
                                "raspyjack,run-raspyjack.sh"));
}

void dongle_lend()
{
    const std::string helper = wifi_helper();
    if (access(helper.c_str(), X_OK) != 0) return;
    const char *argv[] = {"sudo", "-n", helper.c_str(), "lend", nullptr};
    // Returns as soon as the adapter is idle (about half a second); the built-in
    // radio reconnects in the background. Never waited for longer than this.
    capture_now(argv, 25000);
}

void dongle_return()
{
    run_wifi_helper("return");
}

// Kept on disk so it survives the launcher being restarted while the app is open.
void set_dongle_owed(bool owed)
{
    settings::put("dongle_owed", owed ? "1" : "0");
    settings::save();
}

bool dongle_owed()
{
    return settings::get("dongle_owed", "0") == "1";
}

} // namespace ground0
