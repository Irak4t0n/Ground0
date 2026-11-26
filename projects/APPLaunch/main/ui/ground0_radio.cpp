/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_radio.hpp"

#include "cp0_lvgl_app.h"
#include "ui.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace ground0 {
namespace {

std::string lower(std::string value)
{
    for (char &c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::string config_str(const char *key, std::string value)
{
    cp0_signal_config_api({"GetStr", key, value},
                          [&](int code, std::string data) { if (code == 0) value = std::move(data); });
    return value;
}

std::vector<std::string> words(const std::string &list)
{
    std::vector<std::string> out;
    std::stringstream stream(lower(list));
    std::string word;
    while (std::getline(stream, word, ',')) {
        const auto first = word.find_first_not_of(" \t");
        const auto last = word.find_last_not_of(" \t");
        if (first != std::string::npos) out.push_back(word.substr(first, last - first + 1));
    }
    return out;
}

bool contains_any(const std::string &text, const std::vector<std::string> &list)
{
    return std::any_of(list.begin(), list.end(),
                       [&](const std::string &word) { return text.find(word) != std::string::npos; });
}

// A unit name goes onto a command line: letters, digits and . _ - @ only.
bool safe_unit(const std::string &unit)
{
    return !unit.empty() && unit.size() < 96 &&
           std::all_of(unit.begin(), unit.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '.' || c == '_' || c == '-' || c == '@';
           });
}

} // namespace

Radio &Radio::instance()
{
    static Radio radio;
    return radio;
}

Radio::Radio()
{
    unit_ = config_str("ground0_radio_unit", "meshzero.service");
    if (!safe_unit(unit_)) unit_.clear();
    free_words_ = words(config_str("ground0_radio_free", "lora-tunnel,quartermaster,camillia,pocket-lorascan,cap-lora-1262,meshterm"));
    mesh_words_ = words(config_str("ground0_radio_mesh", "1999online"));
    restore_ = config_str("ground0_radio_restore", "1") != "0";
    if (const char *sim = std::getenv("GROUND0_RADIO_SIM")) {
        simulated_ = true;
        state_ = std::strcmp(sim, "free") == 0 ? RadioState::Free : RadioState::Mesh;
    }
}

RadioState Radio::poll()
{
    if (simulated_) return state_;
    if (unit_.empty()) return state_ = RadioState::Absent;

    const char *argv[] = {"systemctl", "show", "-p", "LoadState", "-p", "ActiveState", "--value",
                          unit_.c_str(), nullptr};
    char out[96] = "";
    cp0_process_capture_argv(argv, out, sizeof(out));
    const std::string text(out);
    if (text.find("loaded") == std::string::npos) return state_ = RadioState::Absent;
    // "active", "activating" and "reloading" all mean the daemon holds the radio.
    const bool held = text.find("inactive") == std::string::npos &&
                      text.find("failed") == std::string::npos &&
                      text.find("deactivating") == std::string::npos;
    return state_ = held ? RadioState::Mesh : RadioState::Free;
}

bool Radio::run(bool mesh_on, bool background)
{
    if (simulated_) {
        state_ = mesh_on ? RadioState::Mesh : RadioState::Free;
        return true;
    }
    if (unit_.empty()) return false;
    // -n: never prompt. Without a sudo rule for this the request fails quietly
    // and the next poll shows the unchanged state.
    const char *argv[] = {"sudo", "-n", "systemctl", mesh_on ? "start" : "stop", unit_.c_str(), nullptr};
    return cp0_process_run_argv(argv, background ? 1 : 0) == 0;
}

void Radio::request(bool mesh_on)
{
    run(mesh_on, true);
}

bool Radio::set_blocking(bool mesh_on)
{
    run(mesh_on, false);
    return poll() == (mesh_on ? RadioState::Mesh : RadioState::Free);
}

Radio::Need Radio::need_for(const std::string &name, const std::string &exec) const
{
    const std::string text = lower(name + " " + exec);
    if (contains_any(text, free_words_)) return Need::Free;
    if (contains_any(text, mesh_words_)) return Need::Mesh;
    return Need::None;
}

} // namespace ground0
