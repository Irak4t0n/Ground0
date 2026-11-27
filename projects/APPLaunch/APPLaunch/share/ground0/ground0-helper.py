#!/usr/bin/python3
# SPDX-FileCopyrightText: 2026 Irak4t0n
# SPDX-License-Identifier: MIT
"""Ground0 helper: the slow, occasional work the launcher hands off.

  ground0-helper.py index            write ~/.cache/ground0/index.tsv
  ground0-helper.py apply UUID...    update those store apps through 0Hero

index.tsv has one line per launcher entry:
    exec <TAB> category <TAB> uuid <TAB> installed version <TAB> new version <TAB> title
"new version" is empty when the app is current or is not a store app.

Categories come from the CardputerZero store registry. Apps the store does
not know fall back to their .desktop Categories, then to "Other".
Updates are applied by 0Hero's engine (zerohero_core), so the Store's own
transaction flow is used; without 0Hero installed, "apply" reports that.
"""
import glob
import json
import os
import re
import subprocess
import sys
import time

HOME = os.path.expanduser("~")
CACHE = os.environ.get("GROUND0_CACHE_DIR") or os.path.join(HOME, ".cache", "ground0")
INDEX = os.path.join(CACHE, "index.tsv")
REGISTRY_CACHE = os.path.join(CACHE, "registry.json")
LOG = os.path.join(CACHE, "update.log")
STORE_STATE = os.environ.get("GROUND0_STORE_STATE") or os.path.join(HOME, ".local", "share", "cardputerzero-appstore")
APPS_DIR = os.environ.get("GROUND0_APPS_DIR", "/usr/share/APPLaunch/applications")
REGISTRY_URL = "https://cardputerzero.github.io/generated/registry.json"
REGISTRY_MAX_AGE = 6 * 3600
ZEROHERO_DIR = "/usr/share/APPLaunch/apps/zerohero"

DESKTOP_CATEGORY = [  # first match wins
    ("Game", "Games"), ("Emulator", "Emulators"), ("AudioVideo", "Media"), ("Audio", "Media"),
    ("Video", "Media"), ("Network", "Radio & Comms"), ("Development", "Development"),
    ("Education", "Education"), ("Security", "Security"), ("Settings", "System Tools"),
    ("System", "System Tools"), ("Utility", "System Tools"),
]


def vtuple(v):
    """A version as numbers, for comparing: "1.0" equals "1.0.0", "0.6.24-rev2" is newer than "-rev1"."""
    parts = []
    for piece in re.split(r"[.\-+~_:]", str(v)):
        digits = re.search(r"\d+", piece)
        parts.append(int(digits.group()) if digits else 0)
    while parts and parts[-1] == 0:
        parts.pop()
    return tuple(parts)


def registry(allow_fetch=True):
    """The store catalog: fresh cache, else a download, else a stale cache, else nothing."""
    try:
        fresh = time.time() - os.path.getmtime(REGISTRY_CACHE) < REGISTRY_MAX_AGE
    except OSError:
        fresh = False
    if not fresh and allow_fetch:
        tmp = REGISTRY_CACHE + ".part"
        try:
            subprocess.run(["curl", "-fsSL", "--retry", "2", "--max-time", "40", "-o", tmp, REGISTRY_URL],
                           check=True, timeout=120, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            json.load(open(tmp))          # refuse a truncated download
            os.replace(tmp, REGISTRY_CACHE)
        except Exception:
            try:
                os.remove(tmp)
            except OSError:
                pass
    # The catalog is somebody else's file and may change shape: anything that
    # is not a list of app records is treated as "no catalog", never as an error.
    try:
        data = json.load(open(REGISTRY_CACHE))
    except Exception:
        return []
    apps = data.get("apps", []) if isinstance(data, dict) else data
    return [a for a in apps if isinstance(a, dict)] if isinstance(apps, list) else []


def desktop_fields(path):
    fields = {}
    try:
        for line in open(path, errors="replace"):
            key, sep, value = line.partition("=")
            if sep and key in ("Name", "Exec", "Categories") and key not in fields:
                fields[key] = value.strip()
    except OSError:
        pass
    return fields


def build_index():
    os.makedirs(CACHE, exist_ok=True)
    apps = registry()
    by_uuid, by_pkg = {}, {}
    for a in apps:
        if isinstance(a.get("uuid"), str) and a["uuid"]:
            by_uuid[a["uuid"]] = a
        download = a.get("download")
        package = download.get("package") if isinstance(download, dict) else None
        if isinstance(package, str) and package:    # apps without a package name must not match each other
            by_pkg[package] = a

    store = {}   # desktop path -> (uuid, package, installed version)
    try:
        installed = json.load(open(os.path.join(STORE_STATE, "installed.json")))
    except Exception:
        installed = {}
    if not isinstance(installed, dict):
        installed = {}
    for uuid, meta in installed.items():
        if not isinstance(meta, dict):
            continue
        files = meta.get("files")
        for path in files if isinstance(files, list) else []:
            if isinstance(path, str) and path.endswith(".desktop"):
                package = meta.get("package")
                store[os.path.realpath(path)] = (str(uuid), package if isinstance(package, str) else "",
                                                 str(meta.get("version") or ""))

    lines = []
    for desk in sorted(glob.glob(os.path.join(APPS_DIR, "*.desktop"))):
        f = desktop_fields(desk)
        if not f.get("Exec"):
            continue
        uuid, package, have = store.get(os.path.realpath(desk), ("", "", ""))
        entry = by_uuid.get(uuid) or (by_pkg.get(package) if package else None)
        category, new = "", ""
        if entry:
            categories = entry.get("categories")
            if isinstance(categories, str):
                category = categories
            elif isinstance(categories, list) and categories and isinstance(categories[0], str):
                category = categories[0]
            latest = str(entry.get("version") or "")
            if have and latest and vtuple(latest) > vtuple(have):
                new = latest
            if isinstance(entry.get("uuid"), str) and entry["uuid"]:
                uuid = entry["uuid"]
        if not category:
            wanted = f.get("Categories", "")
            category = next((ours for theirs, ours in DESKTOP_CATEGORY if theirs in wanted), "Other")
        clean = lambda s: str(s).replace("\t", " ").replace("\n", " ")
        lines.append("\t".join(clean(x) for x in (f["Exec"], category, uuid, have, new, f.get("Name", ""))))

    text = "\n".join(lines) + "\n"
    try:
        if open(INDEX).read() == text:      # unchanged: leave the file (and its date) alone,
            return len(lines)               # so the launcher does not reload for nothing
    except OSError:
        pass
    tmp = INDEX + ".part"
    with open(tmp, "w") as out:
        out.write(text)
    os.replace(tmp, INDEX)
    return len(lines)


def log(message):
    with open(LOG, "a") as out:
        out.write(message.rstrip("\n") + "\n")


def apply_updates(uuids):
    os.makedirs(CACHE, exist_ok=True)
    open(LOG, "w").close()
    if not os.path.isfile(os.path.join(ZEROHERO_DIR, "zerohero_core.py")):
        log("RESULT failed 0Hero is not installed")
        return 4
    sys.path.insert(0, ZEROHERO_DIR)
    try:
        import zerohero_core as hero
        if hero.store_busy() and not hero.repair_pending():
            log("RESULT failed the Store has an unfinished transaction")
            return 3
        if not hero.can_sudo():
            log("RESULT failed needs passwordless sudo")
            return 4
        log("STATUS checking for updates")
        wanted = [u for u in hero.find_updates() if u.uuid in uuids]
        if not wanted:
            log("RESULT ok nothing to update")
            return 0
        total = len(wanted)

        def show(u):
            position = wanted.index(u) + 1
            if u.status == "working":
                log("STATUS %d/%d %s %s" % (position, total, u.title, u.error or "%s -> %s" % (u.have, u.new)))
            elif u.status == "done":
                log("STATUS %d/%d %s updated" % (position, total, u.title))
            elif u.status == "failed":
                log("STATUS %d/%d %s FAILED: %s" % (position, total, u.title, u.error))

        failed = hero.run_updates(wanted, show)
    except Exception as error:   # never leave the launcher waiting on a missing RESULT line
        log("RESULT failed %s" % str(error)[:120])
        return 2
    try:
        build_index()
    except Exception:
        pass
    if failed:
        log("RESULT failed %d of %d failed: %s" % (len(failed), total, ", ".join(u.title for u in failed)))
        return 1
    log("RESULT ok %d updated" % total)
    return 0


def main(argv):
    if len(argv) >= 1 and argv[0] == "index":
        try:
            print("%d entries" % build_index())
        except Exception as error:      # the launcher keeps the index it has
            print("index not rebuilt: %s" % error, file=sys.stderr)
            return 1
        return 0
    if len(argv) >= 2 and argv[0] == "apply":
        return apply_updates(set(argv[1:]))
    print(__doc__)
    return 64


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
