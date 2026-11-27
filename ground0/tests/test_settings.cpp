// SPDX-FileCopyrightText: 2026 Irak4t0n
// SPDX-License-Identifier: MIT
// Tests for Ground0's settings file (ground0_settings.cpp). Built and run by run.sh.
#include "ground0_settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <unistd.h>

namespace {
std::map<std::string, std::string> legacy;     // the shared config of an earlier version
int checks = 0, fails = 0;

void is(const char *what, const std::string &got, const std::string &wanted)
{
    ++checks;
    if (got == wanted) return;
    ++fails;
    std::printf("FAIL %s: got '%s', wanted '%s'\n", what, got.c_str(), wanted.c_str());
}

std::string joined(const std::vector<std::string> &items)
{
    std::string out;
    for (const std::string &item : items) out += (out.empty() ? "" : ",") + item;
    return out;
}

std::string file_text()
{
    std::ifstream in(ground0::settings::path());
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}
} // namespace

bool ground0::settings::legacy_lookup(const std::string &key, std::string &value)
{
    const auto found = legacy.find(key);
    if (found == legacy.end()) return false;
    value = found->second;
    return true;
}

int main()
{
    using namespace ground0;
    char folder[] = "/tmp/g0-settings-XXXXXX";
    if (!mkdtemp(folder)) return 2;
    const std::string file = std::string(folder) + "/deep/er/settings";
    setenv("GROUND0_SETTINGS", file.c_str(), 1);

    // Nothing saved anywhere: fallbacks.
    is("fallback", settings::get("theme", "chromeshore"), "chromeshore");
    is("empty list", joined(settings::list("fav")), "");

    // An earlier version's shared config is still honoured.
    legacy["ground0_theme"] = "irak4t0n";
    legacy["ground0_favs"] = "0Hero|Marauder||LORA Tunnel";
    legacy["ground0_recent"] = "CLI";
    is("legacy value", settings::get("theme", "chromeshore"), "irak4t0n");
    is("legacy list", joined(settings::list("fav")), "0Hero,Marauder,LORA Tunnel");
    is("no file yet", file_text(), "");

    // Saving moves a key here, folders are made, and the earlier value no longer matters.
    settings::put("theme", "winxp");
    is("save", settings::save() ? "ok" : "failed", "ok");
    legacy["ground0_theme"] = "stale";
    settings::reload();
    is("own value wins", settings::get("theme", "x"), "winxp");
    is("other keys still legacy", joined(settings::list("fav")), "0Hero,Marauder,LORA Tunnel");

    // A long list survives whole (the shared config cut values at 255 characters).
    std::vector<std::string> many;
    for (int i = 0; i < 60; ++i) many.push_back("Application number " + std::to_string(i));
    settings::put_list("fav", many);
    settings::save();
    settings::reload();
    is("60 favourites", std::to_string(settings::list("fav").size()), "60");
    is("last favourite", settings::list("fav").back(), "Application number 59");

    // An empty list stays empty: it does not fall back to the earlier location.
    settings::put_list("fav", {});
    settings::save();
    settings::reload();
    is("emptied list", joined(settings::list("fav")), "");

    // Odd content: '=' and '|' in values are kept, line breaks cannot split a line.
    settings::put("tab", "A=B|C");
    settings::put("note", "two\nlines\r");
    settings::put("=bad", "x");
    settings::save();
    settings::reload();
    is("equals and bar kept", settings::get("tab", ""), "A=B|C");
    is("line breaks removed", settings::get("note", ""), "twolines");
    is("bad key refused", settings::get("=bad", "none"), "none");

    // A key edited by hand while the launcher runs is kept by the next save,
    // and keys this version does not know are carried along.
    {
        std::ofstream out(file, std::ios::app);
        out << "dongle_apps=marauder,myapp\r\nfuture_key=42\n\n# a comment\nnot a setting line\n";
    }
    settings::put("view", "grid");
    settings::save();
    settings::reload();
    is("hand edit kept", settings::get("dongle_apps", ""), "marauder,myapp");
    is("unknown key kept", settings::get("future_key", ""), "42");
    is("our change saved", settings::get("view", ""), "grid");

    // Saving what is already there writes nothing.
    const std::string before = file_text();
    unlink(file.c_str());
    settings::put("view", "grid");
    settings::save();
    is("no write when unchanged", file_text(), "");
    settings::reload();

    // A folder that cannot be written: save reports it and keeps the change for later.
    setenv("GROUND0_SETTINGS", "/proc/ground0/settings", 1);
    settings::reload();
    settings::put("theme", "amber");
    is("unwritable", settings::save() ? "ok" : "failed", "failed");
    is("value still in memory", settings::get("theme", ""), "amber");

    if (std::system((std::string("rm -rf ") + std::string(folder)).c_str()) != 0) std::printf("could not remove the test folder\n");
    if (fails) {
        std::printf("settings: %d of %d checks FAILED\n", fails, checks);
        return 1;
    }
    std::printf("settings: %d checks passed\n", checks);
    return before.empty() ? 1 : 0;
}
