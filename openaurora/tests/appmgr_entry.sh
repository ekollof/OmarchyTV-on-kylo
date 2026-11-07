#!/bin/sh
# Host-only tests for the app-manager idtv entry wrapper. No TV, fb, HAL or
# network access: the bundle binaries are shell stubs in a temporary directory.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
wrap=$root/tools/boot/run-aurora.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-appmgr.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

mkdir -p "$tmp/bundle/modules"
stock=$tmp/stock.sh
cat >"$stock" <<'EOF'
#!/bin/sh
printf '%s\n' "$0" >>"$OA_TEST_MARK"
exit 0
EOF
chmod +x "$stock"

# Dry run must not execute anything and must print all three paths.
out=$(OA_DRYRUN=1 OA_STOCK="$stock" "$wrap")
printf '%s\n' "$out" | grep -q "supervise=.*/oa-supervise"
printf '%s\n' "$out" | grep -q "child=.*/openaurora --modules .*/modules --config .*/openaurora.conf"
printf '%s\n' "$out" | grep -q "fallback=$stock"
test ! -e "$tmp/executed"
echo "ok appmgr entry: dry run prints supervisor/child/fallback without execution"

# Incomplete bundle must exec stock.
mark=$tmp/stock.mark
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_MARK="$mark" "$wrap"
test "$(cat "$mark")" = "$stock"
grep -q "missing openaurora" "$tmp/app.log"
echo "ok appmgr entry: incomplete bundle falls back to stock"

# Complete bundle: the wrapper must hand off to oa-supervise with the bundle
# child, watchdog tool and stock fallback. The stub records argv.
cat >"$tmp/bundle/openaurora" <<'EOF'
#!/bin/sh
exit 0
EOF
chmod +x "$tmp/bundle/openaurora"
cat >"$tmp/bundle/oa-supervise" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" >"$OA_TEST_SUPERVISE"
exit 0
EOF
chmod +x "$tmp/bundle/oa-supervise"
printf 'test=1\n' >"$tmp/bundle/openaurora.conf"
touch "$tmp/bundle/modules/x"
cp "$wrap" "$tmp/bundle/run-aurora.sh"
chmod +x "$tmp/bundle/run-aurora.sh"
sup=$tmp/supervise.args
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_SUPERVISE="$sup" \
    "$tmp/bundle/run-aurora.sh"
grep -qx -- '--heartbeat' "$sup"
grep -qx -- '--ready' "$sup"
grep -qx -- '--wdt' "$sup"
grep -qx -- '/buffer/wdtctl' "$sup"
grep -qx -- '--fallback' "$sup"
grep -qx -- "$stock" "$sup"
grep -qx -- '--' "$sup"
grep -qx -- "$tmp/bundle/openaurora" "$sup"
grep -qx -- '--modules' "$sup"
grep -qx -- "$tmp/bundle/modules" "$sup"
grep -qx -- '--config' "$sup"
grep -qx -- "$tmp/bundle/openaurora.conf" "$sup"
grep -qx -- '--sock' "$sup"
grep -qx -- '/tmp/openaurora.sock' "$sup"
echo "ok appmgr entry: complete bundle hands off to oa-supervise"

# A bad manifest must fall back before executing the supervisor.
printf 'deadbeef  openaurora\n' >"$tmp/bundle/manifest.sha256"
rm -f "$tmp/supervise.args" "$tmp/stock.mark"
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_MARK="$tmp/stock.mark" \
    OA_TEST_SUPERVISE="$tmp/supervise.args" "$tmp/bundle/run-aurora.sh"
test "$(cat "$tmp/stock.mark")" = "$stock"
grep -q "manifest verification failed" "$tmp/app.log"
test ! -e "$tmp/supervise.args"
echo "ok appmgr entry: manifest failure falls back to stock"

echo "all appmgr entry tests passed"
