#!/bin/sh
# SPDX-License-Identifier: MIT
# Tests for files/ground0-wifi, run against a pretend NetworkManager and a
# pretend /sys, so they need neither root nor a Wi-Fi adapter.
#   sh ground0/tests/test-wifi-helper.sh
HERE=$(cd "$(dirname "$0")" && pwd)
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/bin" "$T/nm" "$T/run" "$T/etc/ground0" "$T/sys/class/net" "$T/bus/usb" "$T/bus/sdio"

# The helper under test, with its fixed paths pointed into the sandbox.
sed -e "s#/sys/class/net#$T/sys/class/net#g" -e "s#/run/ground0#$T/run/ground0#g" \
    -e "s#/etc/ground0#$T/etc/ground0#g" -e "s#^PATH=.*#PATH=$T/bin:/usr/bin:/bin#" \
    "$HERE/../packaging/files/ground0-wifi" > "$T/helper"
chmod +x "$T/helper"       # "on", "off" and "return" run it again for the sync

# --- pretend NetworkManager: one file per fact under $T/nm -------------------
cat > "$T/bin/nmcli" <<EOF
#!/bin/sh
N=$T/nm
EOF
cat >> "$T/bin/nmcli" <<'EOF'
echo "$*" >> "$N/calls"
[ "$1" = --wait ] && shift 2
known() { [ -e "$N/state.$1" ]; }
case "$1 $2" in
"-g GENERAL.STATE") known "$5" || exit 10
    case "$(cat "$N/state.$5")" in 100) echo "100 (connected)";; 30) echo "30 (disconnected)";; 10) echo "10 (unmanaged)";; *) echo "$(cat "$N/state.$5") (other)";; esac ;;
"-g GENERAL.CON-UUID") known "$5" || exit 10; grep "^$(cat "$N/con.$5" 2>/dev/null) " "$N/cons" 2>/dev/null | cut -d' ' -f2 ;;
"-g GENERAL.CONNECTION") known "$5" || exit 10; cat "$N/con.$5" 2>/dev/null ;;
"-g GENERAL.AUTOCONNECT") known "$5" || exit 10; cat "$N/autoconnect.$5" 2>/dev/null ;;
"-g IP4.GATEWAY") echo 192.168.4.1 ;;
"radio wifi") case "$3" in on) echo enabled > "$N/radio";; off) echo disabled > "$N/radio";; *) cat "$N/radio";; esac ;;
"connection delete") grep -q "^$4 " "$N/cons" || exit 10
    grep -v "^$4 " "$N/cons" > "$N/cons.new"; mv "$N/cons.new" "$N/cons"
    for f in "$N"/con.*; do [ "$(cat "$f")" = "$4" ] && { : > "$f"; echo 30 > "$N/state.${f##*.}"; }; done ;;
"connection clone") echo "$6 clone-of-$5" >> "$N/cons" ;;
"connection modify") : ;;
"connection up") known "$6" || exit 10
    [ -e "$N/fail.up.$6" ] && exit 4
    if [ "$3" = id ]; then name=$4; else name=$(grep " $4\$" "$N/cons" | cut -d' ' -f1); fi
    [ -n "$name" ] || exit 10
    echo 100 > "$N/state.$6"; echo "$name" > "$N/con.$6" ;;
"device disconnect") known "$3" || exit 10; echo 30 > "$N/state.$3"; : > "$N/con.$3" ;;
"device connect") known "$3" || exit 10; [ -e "$N/fail.connect.$3" ] && exit 4
    echo 100 > "$N/state.$3"; echo Howards > "$N/con.$3" ;;
"device set") known "$3" || exit 10; echo "$5" > "$N/$4.$3" ;;
*) echo "nmcli stub: unhandled: $*" >&2; exit 99 ;;
esac
exit 0
EOF
# The clock is ours too: "now" in $T/nm/now when a test sets it.
printf '#!/bin/sh\nif [ -e %s/nm/now ]; then cat %s/nm/now; else /bin/date "$@"; fi\n' "$T" "$T" > "$T/bin/date"
for tool in logger ping sleep; do printf '#!/bin/sh\nexit 0\n' > "$T/bin/$tool"; done
printf '#!/bin/sh\nexit 1\n' > "$T/bin/systemctl"     # so "on", "off" and "return" sync in place
chmod +x "$T/bin"/*

# --- the pretend machine -----------------------------------------------------
N=$T/nm; R=$T/run
plug() {        # $1 = name, $2 = usb|sdio, $3 = ifindex; with a 4th word NetworkManager does not know it yet
    mkdir -p "$T/sys/class/net/$1/phy80211" "$T/sys/class/net/$1/device"
    ln -sfn "$T/bus/$2" "$T/sys/class/net/$1/device/subsystem"
    echo 1 > "$T/sys/class/net/$1/type"; echo "$3" > "$T/sys/class/net/$1/ifindex"
    [ -n "$4" ] && return 0
    echo 30 > "$N/state.$1"; : > "$N/con.$1"; echo yes > "$N/autoconnect.$1"
}
unplug() { rm -rf "$T/sys/class/net/$1"; rm -f "$N"/*."$1"; }
# What NetworkManager does by itself: an idle adapter that may autoconnect joins its own profile.
nm_settle() {
    [ -e "$N/state.wlan1" ] || return 0
    if [ "$(cat "$N/state.wlan1")" = 30 ] && [ "$(cat "$N/autoconnect.wlan1")" = yes ]; then
        echo 100 > "$N/state.wlan1"; echo wlan1-Howards > "$N/con.wlan1"
    fi
}
fresh() {
    rm -rf "$N" "$R" "$T/sys/class/net" "$T/etc/ground0"
    mkdir -p "$N" "$R" "$T/sys/class/net" "$T/etc/ground0"
    echo enabled > "$N/radio"
    printf 'Howards uuid-home\nwlan1-Howards uuid-stick\n' > "$N/cons"
    plug wlan0 sdio 2; echo 100 > "$N/state.wlan0"; echo Howards > "$N/con.wlan0"
    : > "$T/etc/ground0/wifi-follow"
}
run() { sh "$T/helper" "$@" > "$T/out" 2>&1; }
st() { cat "$N/state.$1" 2>/dev/null; }
con() { cat "$N/con.$1" 2>/dev/null; }
fails=0; checks=0
is() {          # $1 = what, $2 = got, $3 = wanted
    checks=$((checks + 1))
    [ "$2" = "$3" ] && return 0
    fails=$((fails + 1)); echo "FAIL [$case_name] $1: got '$2', wanted '$3'"; sed 's/^/     | /' "$T/out"
}
has() { checks=$((checks + 1)); [ -e "$2" ] || { fails=$((fails + 1)); echo "FAIL [$case_name] $1: $2 is missing"; }; }
hasnt() { checks=$((checks + 1)); [ ! -e "$2" ] || { fails=$((fails + 1)); echo "FAIL [$case_name] $1: $2 should not exist"; }; }
moved() { is "stick carries Wi-Fi" "$(st wlan1) $(con wlan1)" "100 ground0-dongle"; is "built-in is off" "$(st wlan0)" 30; has "moved marker" "$R/ground0-wifi"; }
on_builtin() { is "built-in carries Wi-Fi" "$(st wlan0) $(con wlan0)" "100 Howards"; hasnt "moved marker" "$R/ground0-wifi"; }

case_name="plug in: tested, then Wi-Fi moves"
fresh; plug wlan1 usb 30; run sync
moved; grep -q "testing it" "$T/out"; is "it was tested" $? 0

case_name="lend: stick idle and kept idle, built-in takes over"
run lend
on_builtin
is "stick is idle" "$(st wlan1)" 30
is "stick may not autoconnect" "$(cat "$N/autoconnect.wlan1")" no
is "lent marker holds the stick" "$(cat "$R/ground0-wifi-lent")" 30
nm_settle; is "no profile takes it back" "$(st wlan1)" 30
run sync; is "a sync while lent leaves it alone" "$(st wlan1)" 30; on_builtin

case_name="return: moves back without a second test"
run return
hasnt "lent marker" "$R/ground0-wifi-lent"
is "stick may autoconnect again" "$(cat "$N/autoconnect.wlan1")" yes
moved; grep -q "testing it" "$T/out"; is "no second test" $? 1

case_name="lend twice in a row, then return"
run lend; run lend; is "stick is idle" "$(st wlan1)" 30; on_builtin
run return; moved

case_name="stick resets while lent: put aside again, tested again afterwards"
run lend; unplug wlan1; plug wlan1 usb 31; nm_settle
is "the new one joined by itself" "$(con wlan1)" wlan1-Howards
run sync
is "put aside again" "$(st wlan1) $(cat "$N/autoconnect.wlan1")" "30 no"
is "lent marker follows it" "$(cat "$R/ground0-wifi-lent")" 31
run return; moved; grep -q "testing it" "$T/out"; is "a different stick is tested" $? 0

case_name="plugged in while the app is open, before NetworkManager knows it"
fresh; run lend
is "nothing to put aside yet" "$(cat "$R/ground0-wifi-lent")" 0
plug wlan1 usb 40 later; run sync
is "not marked done" "$(cat "$R/ground0-wifi-lent")" 0
plug wlan1 usb 40; nm_settle; run sync
is "put aside on the next event" "$(st wlan1) $(cat "$N/autoconnect.wlan1")" "30 no"
is "marked done" "$(cat "$R/ground0-wifi-lent")" 40
on_builtin

case_name="unplugged while lent"
unplug wlan1; run sync; run return
hasnt "lent marker" "$R/ground0-wifi-lent"; on_builtin

case_name="feature off: still lent to the app, and its own profile comes back"
fresh; rm -f "$T/etc/ground0/wifi-follow"; plug wlan1 usb 50; nm_settle
run lend
is "stick is idle" "$(st wlan1) $(cat "$N/autoconnect.wlan1")" "30 no"; on_builtin
run return; nm_settle
is "its own profile is back" "$(con wlan1)" wlan1-Howards; on_builtin

case_name="a stick in monitor mode is not touched"
fresh; plug wlan1 usb 60; echo 803 > "$T/sys/class/net/wlan1/type"; : > "$N/calls"
run lend
grep -q "device disconnect wlan1\|device set wlan1" "$N/calls"; is "no commands sent to it" $? 1
is "marked done" "$(cat "$R/ground0-wifi-lent")" 60

case_name="lend while the stick is marked unstable"
fresh; plug wlan1 usb 70; touch "$N/fail.up.wlan1"; run sync
has "unstable marker" "$R/ground0-wifi-failed"; on_builtin
nm_settle; run lend; is "stick is idle" "$(st wlan1)" 30
run return; has "still unstable afterwards" "$R/ground0-wifi-failed"; on_builtin

case_name="Enter on the row clears a hand-off that was never returned"
fresh; plug wlan1 usb 80; run sync; run lend
run on
hasnt "lent marker" "$R/ground0-wifi-lent"; moved

case_name="Wi-Fi switched off: lend and return change nothing they should not"
fresh; plug wlan1 usb 90; run sync; echo disabled > "$N/radio"
run lend; is "stick is idle" "$(st wlan1)" 30
run return; hasnt "lent marker" "$R/ground0-wifi-lent"

case_name="built-in network out of reach while lent: tried once, not on every event"
fresh; rm -f "$T/etc/ground0/wifi-follow"; plug wlan1 usb 96; nm_settle
echo 30 > "$N/state.wlan0"; : > "$N/con.wlan0"; touch "$N/fail.connect.wlan0"; : > "$N/calls"
run lend
is "stick is idle" "$(st wlan1)" 30
is "one attempt" "$(grep -c "device connect wlan0" "$N/calls")" 1
run sync; run sync
is "still one attempt after more events" "$(grep -c "device connect wlan0" "$N/calls")" 1
hasnt "take-over note" "$R/ground0-wifi-back"
run return; hasnt "lent marker" "$R/ground0-wifi-lent"

case_name="a move that finishes after the hand-off is undone"
fresh; plug wlan1 usb 97; run sync; moved
echo 97 > "$R/ground0-wifi-lent"                  # lent, but Wi-Fi is (again) on the stick
run sync
is "stick is idle" "$(st wlan1) $(cat "$N/autoconnect.wlan1")" "30 no"; on_builtin
run return

case_name="unplug after moving: built-in takes over"
fresh; plug wlan1 usb 95; run sync; moved
unplug wlan1; run sync; on_builtin

# --- keep: join a known network again after NetworkManager gave up -----------
at() { echo "$1" > "$N/now"; }
connects() { grep -c "device connect $1" "$N/calls" 2>/dev/null; }
dropped() { echo 30 > "$N/state.$1"; : > "$N/con.$1"; }

case_name="keep: connected Wi-Fi is left alone"
fresh; at 1000; : > "$N/calls"; run keep
is "no reconnect" "$(connects wlan0)" 0; hasnt "no down marker" "$R/ground0-wifi-down"

case_name="keep: dropped, NetworkManager gets a minute first"
dropped wlan0; at 1000; run keep
is "not yet" "$(connects wlan0)" 0; has "down marker" "$R/ground0-wifi-down"
at 1030; run keep; is "still not yet at 30s" "$(connects wlan0)" 0

case_name="keep: after a minute it joins again"
at 1060; run keep
is "one reconnect" "$(connects wlan0)" 1; on_builtin
hasnt "marker cleared" "$R/ground0-wifi-down"; grep -q "joined Howards again" "$T/out"; is "logged" $? 0

case_name="keep: nothing known in range, then every five minutes"
fresh; : > "$N/calls"; dropped wlan0; : > "$N/fail.connect.wlan0"
at 2000; run keep; at 2060; run keep
is "tried once" "$(connects wlan0)" 1; grep -q "every 5 minutes" "$T/out"; is "said so" $? 0
at 2120; run keep; at 2300; run keep; is "not again within five minutes" "$(connects wlan0)" 1
at 2361; run keep; is "again after five minutes" "$(connects wlan0)" 2
rm -f "$N/fail.connect.wlan0"; at 2662; run keep; on_builtin; hasnt "marker cleared" "$R/ground0-wifi-down"

case_name="keep: Wi-Fi switched off is left off"
fresh; : > "$N/calls"; dropped wlan0; echo disabled > "$N/radio"
at 3000; run keep; at 3100; run keep
is "no reconnect" "$(connects wlan0)" 0; hasnt "no marker" "$R/ground0-wifi-down"

case_name="keep: a radio disconnected on purpose stays disconnected"
fresh; : > "$N/calls"; dropped wlan0; echo no > "$N/autoconnect.wlan0"
at 4000; run keep; at 4100; run keep
is "no reconnect" "$(connects wlan0)" 0

case_name="keep: monitor mode is not touched"
fresh; : > "$N/calls"; dropped wlan0; echo 803 > "$T/sys/class/net/wlan0/type"
at 5000; run keep; at 5100; run keep
is "no reconnect" "$(connects wlan0)" 0

case_name="keep: during a dongle test nothing happens"
fresh; : > "$N/calls"; dropped wlan0; : > "$R/ground0-wifi-trial"
at 6000; run keep; at 6100; run keep
is "no reconnect" "$(connects wlan0)" 0

case_name="keep: with Wi-Fi moved to the stick, the stick is the one rejoined"
fresh; plug wlan1 usb 30; run sync; moved; : > "$N/calls"
dropped wlan1; at 7000; run keep; at 7060; run keep
is "stick rejoined" "$(connects wlan1)" 1; is "built-in left alone" "$(connects wlan0)" 0

case_name="keep: a stick lent to an app is not touched"
fresh; plug wlan1 usb 30; run sync; run lend; : > "$N/calls"
at 8000; run keep; at 8060; run keep
is "stick stays idle" "$(st wlan1)" 30; is "no reconnect of the stick" "$(connects wlan1)" 0; on_builtin

if [ "$fails" = 0 ]; then echo "ground0-wifi: $checks checks passed"; else echo "ground0-wifi: $fails of $checks checks FAILED"; exit 1; fi
