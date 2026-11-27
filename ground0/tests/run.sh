#!/bin/sh
# SPDX-License-Identifier: MIT
# Ground0's own tests. Needs a C++17 compiler; no root, no Cardputer, no Wi-Fi adapter.
#   sh ground0/tests/run.sh
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
UI="$HERE/../../projects/APPLaunch/main/ui"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
${CXX:-g++} -std=c++17 -Wall -Wextra -I"$UI" "$HERE/test_settings.cpp" "$UI/ground0_settings.cpp" -o "$OUT/test_settings"
${CXX:-g++} -std=c++17 -Wall -Wextra -I"$UI" "$HERE/test_apps.cpp" "$UI/ground0_apps.cpp" -o "$OUT/test_apps"
"$OUT/test_apps"
"$OUT/test_settings"
sh "$HERE/test-wifi-helper.sh"
python3 "$HERE/test_helper.py"
