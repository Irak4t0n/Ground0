/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

// Ground0's own settings file: ~/.config/ground0/settings, one "key=value" per
// line; a key that repeats is a list (one favourite per line).
//
// Ground0 used to keep these in the launcher's shared config.json. That file
// belongs to the stock launcher, holds at most 64 entries of 255 characters
// each, and is rewritten whole on every save, so a long favourites list was cut
// short and an older Ground0 could drop settings a newer stock launcher had
// added. Keeping its own file means Ground0 never writes the shared one.
// Settings saved by earlier versions are still read from there until a key is
// first saved here.
#pragma once

#include <string>
#include <vector>

namespace ground0 {
namespace settings {

// GROUND0_SETTINGS, else $XDG_CONFIG_HOME/ground0/settings, else ~/.config/ground0/settings.
std::string path();

std::string get(const std::string &key, const std::string &fallback);
std::vector<std::string> list(const std::string &key);

// Changes are kept in memory until save().
void put(const std::string &key, const std::string &value);
void put_list(const std::string &key, const std::vector<std::string> &values);

// Writes the changed keys. The file is read again first, so keys edited by
// hand while the launcher runs are kept. True when the file is up to date.
bool save();

// Forget what is in memory and read the file again at the next use.
void reload();

// Where earlier versions kept a key: the launcher's shared config, as
// "ground0_<key>". Provided by the launcher (ground0_system.cpp); returns false
// when the key was never set there.
bool legacy_lookup(const std::string &key, std::string &value);

} // namespace settings
} // namespace ground0
