/*
 * SPDX-FileCopyrightText: 2026 Irak4t0n
 *
 * SPDX-License-Identifier: MIT
 */

#include "ground0_apps.hpp"

#include <algorithm>
#include <cctype>
#include <dirent.h>
#include <fstream>
#include <sstream>

namespace ground0 {

namespace {

std::string lowered(std::string value)
{
    for (char &c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return value;
}

std::string stripped(const std::string &value)
{
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

} // namespace

AppEntry app_entry_in(const std::string &folder, const std::string &exec)
{
    AppEntry found;
    const std::string wanted = stripped(exec);
    if (wanted.empty()) return found;
    DIR *dir = opendir(folder.c_str());
    if (!dir) return found;
    while (const dirent *file = readdir(dir)) {
        const std::string name = file->d_name;
        if (name.size() <= 8 || name.compare(name.size() - 8, 8, ".desktop") != 0) continue;
        std::ifstream in(folder + "/" + name);
        AppEntry entry;
        bool matches = false, in_entry = false;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty() && line[0] == '[') {
                in_entry = stripped(line) == "[Desktop Entry]";
                continue;
            }
            const std::size_t equals = line.find('=');
            if (!in_entry || equals == std::string::npos) continue;
            const std::string key = stripped(line.substr(0, equals));
            const std::string value = stripped(line.substr(equals + 1));
            if (key == "Exec") matches = value == wanted;
            else if (key == "X-Ground0-Radio") entry.radio = lowered(value);
            else if (key == "X-Ground0-WifiAdapter") entry.wifi = lowered(value);
        }
        if (!matches) continue;
        entry.id = name.substr(0, name.size() - 8);
        found = entry;
        break;
    }
    closedir(dir);
    return found;
}

std::vector<std::string> app_keys(const std::string &name, const std::string &exec, const std::string &id)
{
    // Whole names only, so "cap-lora-1262" does not also catch "...-cap-lora-1262-gps".
    std::vector<std::string> keys = {lowered(name)};
    if (!id.empty()) keys.push_back(lowered(id));
    std::stringstream tokens(exec);
    std::string token;
    bool first = true;
    while (tokens >> token) {
        const std::size_t slash = token.find_last_of('/');
        if (slash == std::string::npos && !first) continue;
        first = false;
        keys.push_back(lowered(slash == std::string::npos ? token : token.substr(slash + 1)));
    }
    return keys;
}

bool listed(const std::vector<std::string> &keys, const std::string &list)
{
    std::stringstream words(lowered(list));
    std::string word;
    while (std::getline(words, word, ',')) {
        word = stripped(word);
        if (!word.empty() && std::find(keys.begin(), keys.end(), word) != keys.end()) return true;
    }
    return false;
}

} // namespace ground0
