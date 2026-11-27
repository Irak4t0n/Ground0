/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sys/stat.h>
#include <utility>

namespace ground0 {
namespace settings {

namespace {

using Entries = std::vector<std::pair<std::string, std::string>>;

struct State
{
    bool loaded = false;
    Entries entries;                                        // the file, in order, unknown keys included
    std::map<std::string, std::vector<std::string>> dirty;  // keys changed since the last save
};

State &state()
{
    static State instance;
    return instance;
}

std::string single_line(std::string value)
{
    value.erase(std::remove_if(value.begin(), value.end(), [](char c) { return c == '\n' || c == '\r'; }),
                value.end());
    return value;
}

Entries read_file()
{
    Entries entries;
    std::ifstream in(path());
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos || equals == 0) continue;
        entries.emplace_back(line.substr(0, equals), line.substr(equals + 1));
    }
    return entries;
}

void load()
{
    State &s = state();
    if (s.loaded) return;
    s.entries = read_file();
    s.loaded = true;
}

void replace(Entries &entries, const std::string &key, const std::vector<std::string> &values)
{
    entries.erase(std::remove_if(entries.begin(), entries.end(),
                                 [&](const std::pair<std::string, std::string> &entry) { return entry.first == key; }),
                  entries.end());
    // An empty list is written as one empty value: "saved, and empty" must not
    // look like "never saved" (which falls back to the earlier location).
    if (values.empty()) entries.emplace_back(key, std::string());
    for (const std::string &value : values) entries.emplace_back(key, value);
}

bool has(const Entries &entries, const std::string &key)
{
    return std::any_of(entries.begin(), entries.end(),
                       [&](const std::pair<std::string, std::string> &entry) { return entry.first == key; });
}

// Where an earlier version kept this key in the shared config.
std::string legacy_name(const std::string &key)
{
    if (key == "fav") return "ground0_favs";
    return "ground0_" + key;
}

void make_folders(const std::string &file)
{
    for (std::size_t slash = file.find('/', 1); slash != std::string::npos; slash = file.find('/', slash + 1))
        mkdir(file.substr(0, slash).c_str(), 0755);     // fails harmlessly where the folder exists
}

} // namespace

std::string path()
{
    if (const char *file = std::getenv("GROUND0_SETTINGS")) return file;
    if (const char *config = std::getenv("XDG_CONFIG_HOME"))
        if (config[0] == '/') return std::string(config) + "/ground0/settings";
    const char *home = std::getenv("HOME");
    return std::string(home && home[0] ? home : "/tmp") + "/.config/ground0/settings";
}

std::string get(const std::string &key, const std::string &fallback)
{
    load();
    const Entries &entries = state().entries;
    for (auto entry = entries.rbegin(); entry != entries.rend(); ++entry)
        if (entry->first == key) return entry->second;
    std::string value;
    return legacy_lookup(legacy_name(key), value) ? value : fallback;
}

std::vector<std::string> list(const std::string &key)
{
    load();
    std::vector<std::string> out;
    const Entries &entries = state().entries;
    if (has(entries, key)) {
        for (const auto &entry : entries)
            if (entry.first == key && !entry.second.empty()) out.push_back(entry.second);
        return out;
    }
    // Earlier versions kept a list as one string, entries separated by '|'.
    std::string joined;
    if (!legacy_lookup(legacy_name(key), joined)) return out;
    std::size_t start = 0;
    while (start <= joined.size()) {
        const std::size_t end = joined.find('|', start);
        const std::string item = joined.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!item.empty()) out.push_back(item);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return out;
}

void put(const std::string &key, const std::string &value)
{
    put_list(key, {value});
}

void put_list(const std::string &key, const std::vector<std::string> &values)
{
    if (key.empty() || key.find('=') != std::string::npos || key[0] == '#') return;
    load();
    std::vector<std::string> clean;
    clean.reserve(values.size());
    for (const std::string &value : values) clean.push_back(single_line(value));
    // Nothing to write when the file already says exactly this.
    std::vector<std::string> now;
    State &s = state();
    for (const auto &entry : s.entries)
        if (entry.first == key) now.push_back(entry.second);
    const std::vector<std::string> wanted = clean.empty() ? std::vector<std::string>{std::string()} : clean;
    if (now == wanted && s.dirty.find(key) == s.dirty.end()) return;
    replace(s.entries, key, clean);
    s.dirty[key] = clean;
}

bool save()
{
    State &s = state();
    if (s.dirty.empty()) return true;
    // Start from what is on disk now, so a key edited by hand is not lost.
    Entries entries = read_file();
    for (const auto &change : s.dirty) replace(entries, change.first, change.second);

    const std::string file = path();
    make_folders(file);
    const std::string temporary = file + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        if (!out) return false;
        out << "# Ground0 settings. One key=value per line; a key that repeats is a list.\n";
        for (const auto &entry : entries) out << entry.first << '=' << entry.second << '\n';
        out.flush();
        if (!out.good()) {
            out.close();
            std::remove(temporary.c_str());
            return false;
        }
    }
    if (std::rename(temporary.c_str(), file.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    s.entries = std::move(entries);
    s.dirty.clear();
    return true;
}

void reload()
{
    State &s = state();
    s.loaded = false;
    s.entries.clear();
    s.dirty.clear();
}

} // namespace settings
} // namespace ground0
