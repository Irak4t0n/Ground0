/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_radio.hpp"

#include "cp0_lvgl_app.h"
#include "ground0_settings.hpp"
#include "ground0_system.hpp"
#include "ui.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>

namespace ground0 {
namespace {

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
    unit_ = settings::get("radio_unit", "meshzero.service");
    if (!safe_unit(unit_)) unit_.clear();
    free_list_ = settings::get("radio_free", "lora-tunnel,quartermaster,camillia-launch,camillia,cap-lora-1262,"
                                             "pocket-lorascan,octet,meshterm");
    mesh_list_ = settings::get("radio_mesh", "1999online");
    restore_ = settings::get("radio_restore", "1") != "0";
    if (const char *sim = std::getenv("GROUND0_RADIO_SIM")) {
        simulated_ = true;
        state_ = std::strcmp(sim, "free") == 0 ? RadioState::Free : RadioState::Mesh;
    }
}

RadioState Radio::poll()
{
    if (simulated_) return state_;
    if (unit_.empty()) return state_ = RadioState::Absent;

    // An answer that is already on its way is used, not asked for twice.
    if (probe_.running()) {
        apply(probe_.wait());
        return state_;
    }
    const char *argv[] = {"systemctl", "show", "-p", "LoadState", "-p", "ActiveState", "--value",
                          unit_.c_str(), nullptr};
    apply(capture_now(argv));
    return state_;
}

void Radio::poll_start()
{
    if (simulated_ || unit_.empty() || probe_.running()) return;
    const char *argv[] = {"systemctl", "show", "-p", "LoadState", "-p", "ActiveState", "--value",
                          unit_.c_str(), nullptr};
    probe_.start(argv);
}

bool Radio::poll_done()
{
    std::string answer;
    if (!probe_.done(answer)) return false;
    apply(answer);
    return true;
}

void Radio::apply(const std::string &answer)
{
    // No answer (systemd too busy to reply in time): keep what was known.
    if (answer.empty()) return;
    if (answer.find("loaded") == std::string::npos) {
        state_ = RadioState::Absent;
        return;
    }
    // "active", "activating" and "reloading" all mean the daemon holds the radio.
    const bool held = answer.find("inactive") == std::string::npos &&
                      answer.find("failed") == std::string::npos &&
                      answer.find("deactivating") == std::string::npos;
    state_ = held ? RadioState::Mesh : RadioState::Free;
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
    if (background) {
        run_background(argv);
        return true;
    }
    return cp0_process_run_argv(argv, 0) == 0;
}

void Radio::set_owed(bool owed)
{
    if (simulated_) return;
    settings::put("radio_owed", owed ? "1" : "0");
    settings::save();
}

bool Radio::owed() const
{
    return !simulated_ && settings::get("radio_owed", "0") == "1";
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
    // What the app says about itself comes first, then Ground0's lists.
    const AppEntry entry = app_entry(exec);
    if (entry.radio == "free") return Need::Free;
    if (entry.radio == "mesh") return Need::Mesh;
    if (entry.radio == "none") return Need::None;
    const std::vector<std::string> keys = app_keys(name, exec, entry.id);
    if (listed(keys, free_list_)) return Need::Free;
    if (listed(keys, mesh_list_)) return Need::Mesh;
    return Need::None;
}

} // namespace ground0
