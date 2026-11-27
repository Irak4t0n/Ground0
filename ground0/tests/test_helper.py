#!/usr/bin/python3
# SPDX-FileCopyrightText: 2026 Irak4t0n
# SPDX-License-Identifier: MIT
"""Tests for ground0-helper.py's index: it reads the Store's files, which can
change shape with any Store update, and must never fail on them."""
import importlib.util
import json
import os
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
HELPER = os.path.join(HERE, "..", "..", "projects", "APPLaunch", "APPLaunch", "share", "ground0", "ground0-helper.py")
work = tempfile.mkdtemp(prefix="g0-helper-")
apps, cache, store = (os.path.join(work, d) for d in ("apps", "cache", "store"))
for d in (apps, cache, store):
    os.makedirs(d)
os.environ.update(GROUND0_APPS_DIR=apps, GROUND0_CACHE_DIR=cache, GROUND0_STORE_STATE=store)
spec = importlib.util.spec_from_file_location("ground0_helper", HELPER)
helper = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helper)

checks = fails = 0


def check(what, got, wanted):
    global checks, fails
    checks += 1
    if got != wanted:
        fails += 1
        print("FAIL %s: got %r, wanted %r" % (what, got, wanted))


def desktop(name, **fields):
    path = os.path.join(apps, name + ".desktop")
    with open(path, "w") as out:
        out.write("[Desktop Entry]\n" + "".join("%s=%s\n" % kv for kv in fields.items()))
    return path


def registry(data):
    with open(os.path.join(cache, "registry.json"), "w") as out:
        out.write(data if isinstance(data, str) else json.dumps(data))


def installed(data):
    with open(os.path.join(store, "installed.json"), "w") as out:
        out.write(data if isinstance(data, str) else json.dumps(data))


def index():
    helper.build_index()
    rows = {}
    for line in open(os.path.join(cache, "index.tsv")).read().splitlines():
        f = line.split("\t")
        rows[f[5]] = {"exec": f[0], "category": f[1], "uuid": f[2], "have": f[3], "new": f[4]}
    return rows


alpha = desktop("alpha", Name="Alpha", Exec="/usr/bin/alpha", Categories="Game;")
beta = desktop("beta", Name="Beta", Exec="/usr/bin/beta --x")
desktop("noexec", Name="NoExec")
desktop("tabbed", Name="Tab\tbed", Exec="/usr/bin/tabbed")

# Versions
check("1.0 equals 1.0.0", helper.vtuple("1.0") == helper.vtuple("1.0.0"), True)
check("1.10 newer than 1.9", helper.vtuple("1.10") > helper.vtuple("1.9"), True)
check("rev2 newer than rev1", helper.vtuple("0.6.24-rev2") > helper.vtuple("0.6.24-rev1"), True)
check("odd version text", helper.vtuple("banana"), ())

# The normal shape
registry({"apps": [
    {"uuid": "u-alpha", "version": "1.2.0", "categories": ["Games"], "download": {"package": "alpha"}},
    {"uuid": "u-beta", "version": "2.0", "categories": ["Security"], "download": {"package": "beta"}},
]})
installed({"u-alpha": {"files": [alpha], "package": "alpha", "version": "1.1.0"},
           "u-beta": {"files": [beta], "package": "beta", "version": "2.0.0"}})
rows = index()
check("update found", rows["Alpha"]["new"], "1.2.0")
check("category from the store", rows["Alpha"]["category"], "Games")
check("same version in two spellings is not an update", rows["Beta"]["new"], "")
check("entry without Exec is left out", "NoExec" in rows, False)
check("a tab in a name cannot break the columns", rows["Tab bed"]["exec"], "/usr/bin/tabbed")
check("unknown app gets a default category", rows["Tab bed"]["category"], "Other")

# Unchanged content: the file is not rewritten
path = os.path.join(cache, "index.tsv")
os.utime(path, (1000, 1000))
helper.build_index()
check("not rewritten when unchanged", int(os.path.getmtime(path)), 1000)

# Store files in shapes they do not have today
for label, data in [
    ("registry is a list", [{"uuid": "u-alpha", "version": "9", "categories": "Games", "download": None}]),
    ("registry entries are not records", {"apps": ["alpha", 7, None, {"uuid": 5, "categories": [None]}]}),
    ("registry apps is a record", {"apps": {"alpha": {}}}),
    ("registry is a number", 12),
    ("registry is not JSON", "{ not json"),
]:
    registry(data)
    try:
        rows = index()
        check(label, sorted(rows), ["Alpha", "Beta", "Tab bed"])
    except Exception as error:
        check(label, "raised %r" % error, "no error")
registry([{"uuid": "u-alpha", "version": "9", "categories": "Games", "download": None}])
check("a category given as text", index()["Alpha"]["category"], "Games")

registry({"apps": []})
for label, data in [
    ("installed is a list", [1, 2]),
    ("installed files is text", {"u-alpha": {"files": alpha, "package": 5, "version": None}}),
    ("installed entry is text", {"u-alpha": "gone"}),
    ("installed is not JSON", "nope"),
]:
    installed(data)
    try:
        rows = index()
        check(label, rows["Alpha"]["category"], "Games")    # from the .desktop Categories
    except Exception as error:
        check(label, "raised %r" % error, "no error")

# No Store at all
os.remove(os.path.join(store, "installed.json"))
os.remove(os.path.join(cache, "registry.json"))
helper.REGISTRY_URL = "file:///nonexistent/registry.json"
check("no Store files", sorted(index()), ["Alpha", "Beta", "Tab bed"])

if fails:
    print("helper: %d of %d checks FAILED" % (fails, checks))
    sys.exit(1)
print("helper: %d checks passed" % checks)
