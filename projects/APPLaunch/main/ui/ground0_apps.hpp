/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

// How Ground0 recognises an app: by what its .desktop file says, and by the
// names it can be listed under. Nothing here depends on the launcher, so it is
// tested on its own (ground0/tests).
#pragma once

#include <string>
#include <vector>

namespace ground0 {

// What an app says about itself in its .desktop file, so that an app can state
// what it needs without Ground0 having to know it by name:
//   X-Ground0-Radio=free | mesh | none        the LoRa radio (see ground0_radio.hpp)
//   X-Ground0-WifiAdapter=idle | none         a USB Wi-Fi adapter for itself
// "none" switches off what Ground0's own lists would otherwise do for the app.
struct AppEntry
{
    std::string id;      // file name without ".desktop"; empty when no file has this Exec line
    std::string radio;   // lower case, empty when the key is absent
    std::string wifi;
};
AppEntry app_entry_in(const std::string &folder, const std::string &exec);
// The names an app can be listed under, lower case: its name, its .desktop
// file name, the file name of every path in its Exec line, and the first word.
std::vector<std::string> app_keys(const std::string &name, const std::string &exec, const std::string &id);
// True when one of `keys` is in the comma separated `list`.
bool listed(const std::vector<std::string> &keys, const std::string &list);

} // namespace ground0
