#!/bin/sh
# SPDX-License-Identifier: MIT
# Build the ground0 .deb from a cross-compiled launcher binary.
# usage: build-deb.sh <launcher binary> <version> [output directory]
set -e
BIN=$1; VERSION=$2; OUT=${3:-.}
[ -f "$BIN" ] && [ -n "$VERSION" ] || { echo "usage: $0 <launcher binary> <version> [output directory]"; exit 2; }
HERE=$(cd "$(dirname "$0")" && pwd)
SHARE="$HERE/../../projects/APPLaunch/APPLaunch/share"
ROOT=$(mktemp -d)
chmod 0755 "$ROOT"
install -D -m 0755 "$BIN" "$ROOT/usr/share/APPLaunch/bin/M5CardputerZero-APPLaunch"
install -D -m 0755 "$HERE/files/ground0-run" "$ROOT/usr/lib/ground0/ground0-run"
install -D -m 0755 "$HERE/files/ground0" "$ROOT/usr/bin/ground0"
install -D -m 0644 "$HERE/files/10-ground0.conf" "$ROOT/usr/lib/systemd/user/APPLaunch.service.d/10-ground0.conf"
install -D -m 0440 "$HERE/files/sudoers" "$ROOT/etc/sudoers.d/ground0"
install -D -m 0644 "$SHARE/ground0/ground0-helper.py" "$ROOT/usr/share/ground0/ground0-helper.py"
mkdir -p "$ROOT/usr/share/ground0/fonts" "$ROOT/usr/share/doc/ground0" "$ROOT/DEBIAN"
cp "$SHARE"/font/ground0/* "$ROOT/usr/share/ground0/fonts/"
chmod 0644 "$ROOT"/usr/share/ground0/fonts/*
install -m 0644 "$HERE/copyright" "$ROOT/usr/share/doc/ground0/copyright"
install -m 0644 "$HERE/../NOTICE.md" "$ROOT/usr/share/doc/ground0/NOTICE.md"
mkdir -p "$ROOT/usr/share/doc/ground0/licenses"
for f in M5Stack-SDK-MIT.txt LVGL-MIT.txt eventpp-Apache-2.0.txt tree.hh-GPL-3.0-only.txt; do
    install -m 0644 "$HERE/../../LICENSES/$f" "$ROOT/usr/share/doc/ground0/licenses/$f"
done
install -m 0644 "$HERE/../../LICENSE" "$ROOT/usr/share/doc/ground0/licenses/launcher-MIT.txt"
install -m 0644 "$HERE/../../ext_components/Sigslot/LICENSE" "$ROOT/usr/share/doc/ground0/licenses/sigslot-MIT.txt"
install -m 0644 "$HERE/../../ext_components/Miniaudio/LICENSE" "$ROOT/usr/share/doc/ground0/licenses/miniaudio.txt"
sed "s/@VERSION@/$VERSION/" "$HERE/debian/control.in" > "$ROOT/DEBIAN/control"
echo "Installed-Size: $(du -sk "$ROOT/usr" | cut -f1)" >> "$ROOT/DEBIAN/control"
echo "/etc/sudoers.d/ground0" > "$ROOT/DEBIAN/conffiles"
for script in preinst postinst postrm; do install -m 0755 "$HERE/debian/$script" "$ROOT/DEBIAN/$script"; done
dpkg-deb --root-owner-group -Zxz --build "$ROOT" "$OUT/ground0_${VERSION}_arm64.deb"
