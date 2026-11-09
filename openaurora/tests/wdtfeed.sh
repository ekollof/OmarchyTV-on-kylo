#!/bin/sh
# Host-only tests for oa-wdtfeed. The IPC node is a regular file, so no
# /proc/nvtstbcipc, MCU or TV access happens.
set -eu
tool=${1:?usage: wdtfeed.sh PATH/oa-wdtfeed}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-wdtfeed.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

# One beat writes exactly the recovered FIPC_set_watch_dog packet.
: >"$tmp/node"
"$tool" --foreground --once --node "$tmp/node" --log "$tmp/log" --timeout 10
test -f "$tmp/node"
packet=$(od -An -tu1 -N6 "$tmp/node" | tr -s ' ' | sed 's/^ //;s/ $//')
test "$packet" = "0 174 0 10 1 174"
grep -q "beat 1, watchdog 10s" "$tmp/log"
echo "ok wdtfeed: one beat writes the 6-byte 0xae set packet"

# A beats limit exits cleanly after that many beats.
: >"$tmp/node3"
"$tool" --foreground --beats 3 --interval 1 --node "$tmp/node3" --log "$tmp/log3" --timeout 7
grep -q "beat 3, watchdog 7s" "$tmp/log3"
test "$(grep -c '^oa-wdtfeed: beat' "$tmp/log3")" -eq 3
echo "ok wdtfeed: beats limit stops after the requested count"

# A stop file stops feeding before the first beat.
: >"$tmp/node4"
: >"$tmp/stop"
"$tool" --foreground --beats 5 --interval 1 --node "$tmp/node4" --log "$tmp/log4" --stop "$tmp/stop"
grep -q "stop file" "$tmp/log4"
test "$(grep -c '^oa-wdtfeed: beat' "$tmp/log4" || true)" -eq 0
echo "ok wdtfeed: stop file prevents further beats"

# Invalid arguments and a missing node fail clearly.
if "$tool" --interval 0 --once >/dev/null 2>&1; then
    echo "interval 0 accepted" >&2
    exit 1
fi
if "$tool" --node "$tmp/missing/node" --once >/dev/null 2>&1; then
    echo "missing node accepted" >&2
    exit 1
fi
echo "ok wdtfeed: invalid arguments and missing node are rejected"

echo "all wdtfeed tests passed"
