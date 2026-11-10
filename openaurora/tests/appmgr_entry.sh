#!/bin/sh
# Host-only tests for the app-manager idtv entry wrapper. No TV, fb, HAL or
# network access: the bundle binaries are shell stubs in a temporary directory.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
wrap=$root/tools/boot/run-aurora.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-appmgr.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

mkdir -p "$tmp/bundle/modules" "$tmp/gate"
export OA_GATE=$tmp/gate
stock=$tmp/stock.sh
cat >"$stock" <<'EOF'
#!/bin/sh
printf '%s\n' "$0" >>"$OA_TEST_MARK"
exit 0
EOF
chmod +x "$stock"

# Dry run must not execute anything and must print the staged boot pieces.
out=$(OA_DRYRUN=1 OA_STOCK="$stock" "$wrap")
printf '%s\n' "$out" | grep -q "gate=.*/oa-boot-gate.sh .* prepare"
printf '%s\n' "$out" | grep -q "feeder=.*/oa-wdtfeed --node"
printf '%s\n' "$out" | grep -q "supervise=.*/oa-supervise"
printf '%s\n' "$out" | grep -q "child=.*/openaurora --modules .*/modules --config .*/openaurora.conf"
printf '%s\n' "$out" | grep -q "fallback=$stock"
test ! -e "$tmp/executed"
echo "ok appmgr entry: dry run prints gate, feeder, supervisor, child and fallback"

# Incomplete bundle must exec stock.
mark=$tmp/stock.mark
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_MARK="$mark" "$wrap"
test "$(cat "$mark")" = "$stock"
grep -q "missing openaurora" "$tmp/app.log"
echo "ok appmgr entry: incomplete bundle falls back to stock"

# Complete bundle stubs. The gate and feeder record their arguments.
cat >"$tmp/bundle/openaurora" <<'EOF'
#!/bin/sh
exit 0
EOF
cat >"$tmp/bundle/oa-supervise" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" >"$OA_TEST_SUPERVISE"
exit 0
EOF
cat >"$tmp/bundle/oa-boot-gate.sh" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >>"$OA_TEST_GATE"
[ -f "$OA_TEST_GATE_BLOCK" ] && exit 1
exit 0
EOF
cat >"$tmp/bundle/oa-wdtfeed" <<'EOF'
#!/bin/sh
printf '%s\n' "$*" >"$OA_TEST_FEEDER"
[ -f "$OA_TEST_FEEDER_FAIL" ] && exit 1
exit 0
EOF
chmod +x "$tmp/bundle/openaurora" "$tmp/bundle/oa-supervise" \
    "$tmp/bundle/oa-boot-gate.sh" "$tmp/bundle/oa-wdtfeed"
printf 'test=1\n' >"$tmp/bundle/openaurora.conf"
touch "$tmp/bundle/modules/x"
cp "$wrap" "$tmp/bundle/run-aurora.sh"
chmod +x "$tmp/bundle/run-aurora.sh"

sup=$tmp/supervise.args
gate_log=$tmp/gate.args
feeder_log=$tmp/feeder.args
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_SUPERVISE="$sup" \
    OA_TEST_GATE="$gate_log" OA_TEST_FEEDER="$feeder_log" \
    "$tmp/bundle/run-aurora.sh"
grep -qx -- "--heartbeat" "$sup"
grep -qx -- "--ready" "$sup"
grep -qx -- "--wdt" "$sup"
grep -qx -- "/buffer/wdtctl" "$sup"
grep -qx -- "--fallback" "$sup"
grep -qx -- "$stock" "$sup"
grep -qx -- "--gate" "$sup"
grep -qx -- "$tmp/gate" "$sup"
grep -qx -- "--wdt-keep" "$sup"
grep -qx -- "--" "$sup"
grep -qx -- "$tmp/bundle/openaurora" "$sup"
grep -qx -- "--modules" "$sup"
grep -qx -- "$tmp/bundle/modules" "$sup"
grep -qx -- "--config" "$sup"
grep -qx -- "$tmp/bundle/openaurora.conf" "$sup"
grep -qx -- "--sock" "$sup"
grep -qx -- "/tmp/openaurora.sock" "$sup"
grep -qx -- "$tmp/gate prepare" "$gate_log"
grep -qx -- "$tmp/gate start 0" "$gate_log"
grep -q -- "--stop $tmp/gate/oa_wdt_stop" "$feeder_log"
test ! -e "$tmp/gate/oa_wdt_stop"
echo "ok appmgr entry: complete bundle runs the gate, feeder and supervisor"

# A stage config is preferred when present.
printf '1\n' >"$tmp/gate/oa_stage"
printf 'test=1\n' >"$tmp/bundle/openaurora-stage1.conf"
rm -f "$sup" "$gate_log"
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_SUPERVISE="$sup" \
    OA_TEST_GATE="$gate_log" OA_TEST_FEEDER="$feeder_log" \
    "$tmp/bundle/run-aurora.sh"
grep -qx -- "$tmp/gate start 1" "$gate_log"
grep -qx -- "$tmp/bundle/openaurora-stage1.conf" "$sup"
rm -f "$tmp/gate/oa_stage" "$tmp/bundle/openaurora-stage1.conf"
echo "ok appmgr entry: oa_stage selects the staged config"

# Hardware-only stages use the same oa_stage selector and stagehwN config.
printf 'hw2\n' >"$tmp/gate/oa_stage"
printf 'test=1\n' >"$tmp/bundle/openaurora-stagehw2.conf"
rm -f "$sup" "$gate_log"
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_SUPERVISE="$sup" \
    OA_TEST_GATE="$gate_log" OA_TEST_FEEDER="$feeder_log" \
    "$tmp/bundle/run-aurora.sh"
grep -qx -- "$tmp/gate start hw2" "$gate_log"
grep -qx -- "$tmp/bundle/openaurora-stagehw2.conf" "$sup"
rm -f "$tmp/gate/oa_stage" "$tmp/bundle/openaurora-stagehw2.conf"
echo "ok appmgr entry: hardware-only oa_stage selects the hw config"

# A blocked gate must exec stock without starting the feeder or supervisor.
: >"$tmp/gate.block"
rm -f "$mark" "$sup" "$feeder_log"
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_MARK="$mark" \
    OA_TEST_SUPERVISE="$sup" OA_TEST_GATE="$gate_log" \
    OA_TEST_FEEDER="$feeder_log" OA_TEST_GATE_BLOCK="$tmp/gate.block" \
    "$tmp/bundle/run-aurora.sh"
test "$(cat "$mark")" = "$stock"
grep -q "boot gate blocked" "$tmp/app.log"
test ! -e "$sup"
test ! -e "$feeder_log"
rm -f "$tmp/gate.block"
echo "ok appmgr entry: blocked gate falls back before any hardware start"

# A failed feeder is fail-closed: stock, no supervisor.
: >"$tmp/feeder.fail"
rm -f "$mark" "$sup"
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_MARK="$mark" \
    OA_TEST_SUPERVISE="$sup" OA_TEST_GATE="$gate_log" \
    OA_TEST_FEEDER="$feeder_log" OA_TEST_FEEDER_FAIL="$tmp/feeder.fail" \
    "$tmp/bundle/run-aurora.sh"
test "$(cat "$mark")" = "$stock"
grep -q "watchdog feeder failed to start" "$tmp/app.log"
test ! -e "$sup"
rm -f "$tmp/feeder.fail"
echo "ok appmgr entry: feeder failure falls back to stock"

# A bad manifest must fall back before executing the gate or supervisor.
printf 'deadbeef  openaurora\n' >"$tmp/bundle/manifest.sha256"
rm -f "$tmp/supervise.args" "$tmp/stock.mark" "$mark" "$sup" "$gate_log"
OA_STOCK="$stock" OA_APPLOG="$tmp/app.log" OA_TEST_MARK="$mark" \
    OA_TEST_SUPERVISE="$sup" OA_TEST_GATE="$gate_log" \
    OA_TEST_FEEDER="$feeder_log" "$tmp/bundle/run-aurora.sh"
test "$(cat "$mark")" = "$stock"
grep -q "manifest verification failed" "$tmp/app.log"
test ! -e "$sup"
test ! -e "$gate_log"
echo "ok appmgr entry: manifest failure falls back to stock"

echo "all appmgr entry tests passed"
