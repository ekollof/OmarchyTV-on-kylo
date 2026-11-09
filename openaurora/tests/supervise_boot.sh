#!/bin/sh
# Host-only tests for oa-supervise --wdt-keep/--gate. The watchdog tool, stock
# fallback and child are local stubs: no TV, /buffer, /conf or HAL access.
set -eu
sup=${1:?usage: supervise_boot.sh PATH/oa-supervise PATH/supervise_child}
child=${2:?usage: supervise_boot.sh PATH/oa-supervise PATH/supervise_child}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-supervise-boot.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/gate"

wdt=$tmp/wdt
stock=$tmp/stock
cat >"$wdt" <<'EOF'
#!/bin/sh
echo "$@" >> "$WDT_LOG"
exit 0
EOF
cat >"$stock" <<'EOF'
#!/bin/sh
echo stock >> "$STOCK_LOG"
exit 0
EOF
chmod +x "$wdt" "$stock"
export WDT_LOG=$tmp/wdt.log STOCK_LOG=$tmp/stock.log

# 1) --wdt-keep must never disable, and stability must clear the boot attempt.
: >"$tmp/gate/oa_try"
printf '2\n' >"$tmp/gate/oa_bootfail"
"$sup" --heartbeat "$tmp/hb" --ready "$tmp/ready" --wdt "$wdt" \
    --fallback "$stock" --gate "$tmp/gate" --wdt-keep \
    --startup-ms 3000 --timeout-ms 1000 --stable-ms 300 \
    -- "$child" "$tmp/hb" "$tmp/ready" healthy >"$tmp/sup1.log" 2>&1 &
pid=$!
i=0
while [ ! -f "$tmp/gate/oa_last_ok" ] && [ $i -lt 100 ]; do
    sleep 0.1
    i=$((i + 1))
done
test -f "$tmp/gate/oa_last_ok"
kill -TERM "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
grep -q "watchdog kept armed" "$tmp/sup1.log"
grep -q "boot attempt cleared" "$tmp/sup1.log"
test ! -e "$tmp/gate/oa_try"
grep -qx "0" "$tmp/gate/oa_bootfail"
test -f "$tmp/gate/oa_wdt_stop"
test "$(grep -c '^disable' "$tmp/wdt.log" || true)" -eq 0
grep -qx "set 10" "$tmp/wdt.log"
grep -q stock "$tmp/stock.log"
echo "ok supervise boot: --wdt-keep never disables, gate clears on stability"

# 2) Default mode still disables the watchdog before starting the child.
tmp2=$tmp/t2
mkdir -p "$tmp2/gate"
WDT_LOG=$tmp2/wdt.log STOCK_LOG=$tmp2/stock.log
export WDT_LOG STOCK_LOG
"$sup" --heartbeat "$tmp2/hb" --ready "$tmp2/ready" --wdt "$wdt" \
    --fallback "$stock" --gate "$tmp2/gate" \
    --startup-ms 1000 --timeout-ms 500 --stable-ms 100 \
    -- "$child" "$tmp2/hb" "$tmp2/ready" exit >"$tmp2/sup.log" 2>&1 || true
grep -qx "disable" "$tmp2/wdt.log"
grep -qx "set 10" "$tmp2/wdt.log"
grep -q stock "$tmp2/stock.log"
echo "ok supervise boot: default mode disables the watchdog and falls back"

# 3) A third recorded failure removes the enable file.
tmp3=$tmp/t3
mkdir -p "$tmp3/gate"
: >"$tmp3/gate/oa_boot"
printf '2\n' >"$tmp3/gate/oa_fail"
WDT_LOG=$tmp3/wdt.log STOCK_LOG=$tmp3/stock.log
export WDT_LOG STOCK_LOG
"$sup" --heartbeat "$tmp3/hb" --ready "$tmp3/ready" --wdt "$wdt" \
    --fallback "$stock" --fail "$tmp3/gate/oa_fail" \
    --enable "$tmp3/gate/oa_boot" --gate "$tmp3/gate" \
    --startup-ms 300 --timeout-ms 500 --stable-ms 100 \
    -- "$child" "$tmp3/hb" "$tmp3/ready" hang >"$tmp3/sup.log" 2>&1 || true
grep -q "failure streak 3/3" "$tmp3/sup.log"
test ! -e "$tmp3/gate/oa_boot"
grep -q stock "$tmp3/stock.log"
echo "ok supervise boot: three failures remove the enable file"

echo "all supervise boot tests passed"
