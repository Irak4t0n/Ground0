// SPDX-FileCopyrightText: 2026 Irak4t0n
// SPDX-License-Identifier: MIT
// Tests for how Ground0 recognises an app (ground0_apps.cpp). Built and run by run.sh.
#include "ground0_apps.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>

namespace {
int checks = 0, fails = 0;
std::string folder;

void is(const char *what, const std::string &got, const std::string &wanted)
{
    ++checks;
    if (got == wanted) return;
    ++fails;
    std::printf("FAIL %s: got '%s', wanted '%s'\n", what, got.c_str(), wanted.c_str());
}

void desktop(const char *file, const char *body)
{
    std::ofstream(folder + "/" + file) << body;
}

std::string yes(bool value) { return value ? "yes" : "no"; }

std::string joined(const std::vector<std::string> &items)
{
    std::string out;
    for (const std::string &item : items) out += (out.empty() ? "" : " ") + item;
    return out;
}
} // namespace

int main()
{
    using namespace ground0;
    char made[] = "/tmp/g0-apps-XXXXXX";
    if (!mkdtemp(made)) return 2;
    folder = made;

    desktop("marauder_cardputerzero.desktop", "[Desktop Entry]\nName=Marauder\nExec=/usr/bin/marauder_cardputerzero\n");
    desktop("scanner.desktop",
            "[Desktop Entry]\r\nName = Scanner \r\nExec = /opt/scan/run.sh --fast \r\n"
            "X-Ground0-WifiAdapter = Idle\r\nX-Ground0-Radio=FREE\r\n");
    desktop("chatty.desktop", "[Desktop Entry]\nName=Chatty\nExec=/usr/bin/chatty\nX-Ground0-Radio=mesh\n");
    desktop("quiet.desktop", "[Desktop Entry]\nName=Marauder\nExec=/usr/bin/quiet\nX-Ground0-WifiAdapter=none\nX-Ground0-Radio=none\n");
    desktop("actions.desktop",
            "[Desktop Entry]\nName=Actions\nExec=/usr/bin/actions\n\n[Desktop Action other]\n"
            "Exec=/usr/bin/somethingelse\nX-Ground0-Radio=mesh\n");
    desktop("zz-python.desktop", "[Desktop Entry]\nName=Hero\nExec=/usr/bin/python3 /usr/share/APPLaunch/apps/zerohero/app.py\n");
    desktop("notes.txt", "Exec=/usr/bin/marauder_cardputerzero\nX-Ground0-Radio=mesh\n");
    desktop("empty.desktop", "");

    // Finding the entry
    AppEntry entry = app_entry_in(folder, "/usr/bin/marauder_cardputerzero");
    is("found by Exec", entry.id, "marauder_cardputerzero");
    is("no keys set", entry.radio + "/" + entry.wifi, "/");
    entry = app_entry_in(folder, "  /opt/scan/run.sh --fast");
    is("blanks and Windows line ends", entry.id, "scanner");
    is("keys are read in lower case", entry.radio + "/" + entry.wifi, "free/idle");
    is("mesh key", app_entry_in(folder, "/usr/bin/chatty").radio, "mesh");
    is("none key", app_entry_in(folder, "/usr/bin/quiet").wifi, "none");
    entry = app_entry_in(folder, "/usr/bin/actions");
    is("keys of another section are not the app's", entry.radio, "");
    is("an action's Exec is not the app", app_entry_in(folder, "/usr/bin/somethingelse").id, "");
    is("unknown Exec", app_entry_in(folder, "/usr/bin/nothere").id, "");
    is("empty Exec", app_entry_in(folder, "").id, "");
    is("missing folder", app_entry_in(folder + "/none", "/usr/bin/chatty").id, "");

    // The names an app can be listed under
    is("keys of a plain app", joined(app_keys("Marauder", "/usr/bin/marauder_cardputerzero", "marauder_cardputerzero")),
       "marauder marauder_cardputerzero marauder_cardputerzero");
    is("keys through a wrapper",
       joined(app_keys("GPS", "/usr/bin/sudo -n -- /usr/share/APPLaunch/bin/M5CardputerZero-Cap-LoRa-1262-GPS", "cap-lora-1262-gps")),
       "gps cap-lora-1262-gps sudo m5cardputerzero-cap-lora-1262-gps");
    is("keys of a script", joined(app_keys("Hero", "python3 /usr/share/APPLaunch/apps/zerohero/app.py --x", "zz-python")),
       "hero zz-python python3 app.py");
    is("relative program", joined(app_keys("Mail", "bin/czmail", "")), "mail czmail");

    // Lists
    const std::vector<std::string> keys = app_keys("LoRa", "/usr/share/Cap-LoRa-1262/bin/Cap-LoRa-1262", "cap-lora-1262");
    is("listed by file name", yes(listed(keys, "lora-tunnel, CAP-LoRa-1262 ,octet")), "yes");
    is("whole names only", yes(listed(app_keys("GPS", "/usr/bin/cap-lora-1262-gps", "cap-lora-1262-gps"), "cap-lora-1262")), "no");
    is("empty list", yes(listed(keys, "")), "no");
    is("list of commas", yes(listed(keys, ",, ,")), "no");
    // An update that renames the program keeps working through the .desktop name.
    is("renamed program, same desktop file",
       yes(listed(app_keys("Marauder CZ", "/usr/bin/marauder2", "marauder_cardputerzero"), "marauder,marauder_cardputerzero")), "yes");

    if (std::system((std::string("rm -rf ") + folder).c_str()) != 0) std::printf("could not remove the test folder\n");
    if (fails) {
        std::printf("apps: %d of %d checks FAILED\n", fails, checks);
        return 1;
    }
    std::printf("apps: %d checks passed\n", checks);
    return 0;
}
