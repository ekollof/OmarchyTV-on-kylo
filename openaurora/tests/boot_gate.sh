#!/bin/sh
# Host-only tests for the boot-attempt gate. No TV, /conf, /buffer or HAL.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
gate=$root/tools/boot/oa-boot-gate.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-boot-gate.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/conf"

run()
{
    # Capture combined output and exit status without letting set -e abort.
    out=$(sh "$gate" "$tmp/conf" "$@" 2>&1) && rc=0 || rc=$?
}

# Empty state: no enable file, native blocked, no token written.
run prepare
test "$rc" -eq 1
printf '%s\n' "$out" | grep -q "native blocked"
test ! -e "$tmp/conf/oa_try"
echo "ok boot gate: absent enable blocks the native path"

# Enable, then prepare allows and writes nothing.
: >"$tmp/conf/oa_boot"
run prepare
test "$rc" -eq 0
printf '%s\n' "$out" | grep -q "native allowed (fail=0)"
test ! -e "$tmp/conf/oa_try"
echo "ok boot gate: enabled boot is allowed"

# start writes the token and the stage.
run start 2
test "$rc" -eq 0
test -f "$tmp/conf/oa_try"
grep -qx "stage=2" "$tmp/conf/oa_try"
grep -qx "2" "$tmp/conf/oa_stage"
echo "ok boot gate: start records stage and token"

# An uncleared token means the previous boot never reached stability.
run prepare
test "$rc" -eq 0
grep -qx "1" "$tmp/conf/oa_bootfail"
test ! -e "$tmp/conf/oa_try"
printf '%s\n' "$out" | grep -q "did not reach stability (1/3)"
echo "ok boot gate: uncleared token increments the failure count"

# Three unproven boots remove the enable file and block the path.
run start 1
run prepare
grep -qx "2" "$tmp/conf/oa_bootfail"
run start 1
run prepare
run prepare
test "$rc" -eq 1
test ! -e "$tmp/conf/oa_boot"
printf '%s\n' "$out" | grep -q "native blocked"
echo "ok boot gate: three unproven boots disable the native path"

# clear resets the streak, removes the token and records success.
: >"$tmp/conf/oa_boot"
printf '9\n' >"$tmp/conf/oa_bootfail"
run start 0
run clear 0
test "$rc" -eq 0
test ! -e "$tmp/conf/oa_try"
grep -qx "0" "$tmp/conf/oa_bootfail"
grep -q "^stage=0 " "$tmp/conf/oa_last_ok"
echo "ok boot gate: clear resets the streak and records the last success"

# oa_safe always wins over an enabled boot.
: >"$tmp/conf/oa_safe"
run prepare
test "$rc" -eq 1
printf '%s\n' "$out" | grep -q "safe=1"
rm -f "$tmp/conf/oa_safe"
echo "ok boot gate: oa_safe blocks the native path"

# Garbage counters are treated as zero.
printf 'nonsense\n' >"$tmp/conf/oa_bootfail"
run prepare
test "$rc" -eq 0
printf '%s\n' "$out" | grep -q "native allowed (fail=0)"
echo "ok boot gate: malformed counter is treated as zero"

# Invalid stage is rejected without writing a token.
run start 7
test "$rc" -eq 2
test ! -e "$tmp/conf/oa_try"
echo "ok boot gate: invalid stage is rejected"

# Status reports all state.
run status
test "$rc" -eq 0
printf '%s\n' "$out" | grep -q "boot=1 safe=0 fail=0 try=0 stage=0"
echo "ok boot gate: status reports the current state"

echo "all boot gate tests passed"
