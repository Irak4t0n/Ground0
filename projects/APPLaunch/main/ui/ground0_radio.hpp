/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

// Ground0 radio switch. One LoRa cap, several programs that want it: a mesh
// daemon (MeshZero by default) holds it while running, and apps that drive the
// radio themselves need that daemon stopped. This tracks who holds the radio,
// toggles the daemon, and hands the radio over around an app launch.
//
// Configuration (launcher config, all strings):
//   ground0_radio_unit     systemd unit of the mesh daemon   (meshzero.service)
//   ground0_radio_free     apps that need the daemon stopped (comma separated)
//   ground0_radio_mesh     apps that need the daemon running (comma separated)
//   ground0_radio_restore  "1": start the daemon again when a free-radio app exits
// An app matches when its name, or the file name of a path in its Exec line,
// equals one of the listed words (case-insensitive).
#pragma once

#include "ground0_system.hpp"

#include <string>
#include <vector>

namespace ground0 {

enum class RadioState { Absent, Mesh, Free };

class Radio
{
public:
    static Radio &instance();

    // Ask systemd for the daemon's state and wait for the answer (about 50 ms,
    // never more than three seconds). Absent: the unit is not installed.
    RadioState poll();
    RadioState state() const { return state_; }
    // The same without waiting: start the question, then ask poll_done() from a
    // timer. It returns true when an answer has arrived and state() is current.
    void poll_start();
    bool poll_done();
    bool polling() const { return probe_.running(); }

    // Start or stop the daemon without waiting; poll() picks up the result.
    void request(bool mesh_on);
    // Start or stop the daemon and wait for it. Returns true when the state matches.
    bool set_blocking(bool mesh_on);

    enum class Need { None, Free, Mesh };
    Need need_for(const std::string &name, const std::string &exec) const;
    bool restore_after_app() const { return restore_; }

    // Ground0 stopped the daemon for an app and still has to start it again.
    // Kept in the config so it survives Ground0 being restarted mid-app.
    void set_owed(bool owed);
    bool owed() const;

private:
    Radio();
    bool run(bool mesh_on, bool background);
    void apply(const std::string &answer);

    std::string unit_;
    std::string free_list_;       // comma separated, as in the settings
    std::string mesh_list_;
    bool restore_ = true;
    bool simulated_ = false;      // GROUND0_RADIO_SIM: no systemd, state kept in memory
    RadioState state_ = RadioState::Absent;
    Probe probe_;
};

} // namespace ground0
