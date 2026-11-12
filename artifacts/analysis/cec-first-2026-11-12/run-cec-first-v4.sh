#!/bin/sh
# Temporary CEC-first boot helper, third attempt (2026-11-12).
#
# Runs before stock Aurora, opens the raw ntkcec device, observes the bus for
# the Deck's logical address, then sends Give Physical Address to LA8, LA4 and
# broadcast. It never calls quit or wait: the test process is SIGKILLed after a
# bounded window, so a stuck CEC close cannot hold the boot hook. A
# self-terminating MCU feeder and an independent reaper bound even a killed
# shell.
#
#   run-cec-first.sh --boot   # start the MCU feeder during the boot test
#   run-cec-first.sh          # manual rehearsal (no feeder)
set -u
D=/buffer/oa-cec-first
LOG="$D/cec.log"
CTL="$D/control"
BOOT=0
[ "${1:-}" = "--boot" ] && BOOT=1

# One-shot guard: if this boot dies before cleanup the next boot must not
# rerun the test even if the hook branch was not restored.
if [ -x "$D/run-cec-first.sh" ]; then
    mv -f "$D/run-cec-first.sh" "$D/run-cec-first.sh.done" 2>/dev/null || true
fi

: >"$LOG"
echo "== cec-first $(date) boot=$BOOT ==" >>"$LOG"
cd "$D" || exit 1
rm -f "$CTL" /tmp/ntkcec
mknod /tmp/ntkcec c 229 0 2>/dev/null || true

# Bounded control call: oa-client takes the whole command as one argument.
ctl() {
    timeout -t 2 -s KILL "$D/oa-client" "$CTL" "$*" 2>&1 || true
}

# Self-terminating MCU feeder. 30 beats at 2 s (60 s) outlives the 40 s hook
# timeout even if this shell is SIGKILLed, then stops by itself.
FEED=
if [ "$BOOT" = 1 ] && [ -x "$D/oa-wdtfeed" ]; then
    "$D/oa-wdtfeed" --node /proc/nvtstbcipc --interval 2 --timeout 10 \
        --beats 30 --log "$D/wdt.log" --foreground </dev/null >>"$LOG" 2>&1 &
    FEED=$!
fi

"$D/openaurora" --modules "$D/modules" --config "$D/cec.conf" \
    --sock "$CTL" --log 3 >>"$LOG" 2>&1 &
PID=$!

# Independent reaper: survives a SIGKILL of this shell and kills only our
# openaurora (exact cmdline), so stock Aurora can open the raw device.
(
    sleep 30
    CMD=$(cat "/proc/$PID/cmdline" 2>/dev/null || true)
    case "$CMD" in
        *oa-cec-first/openaurora*) kill -9 "$PID" 2>/dev/null ;;
    esac
    touch /tmp/oa-wdt-stop 2>/dev/null || true
) </dev/null >/dev/null 2>&1 &
REAPER=$!

# No blocking wait: SIGKILL the module process and poll its /proc entry for at
# most 2 s. Never call the CEC close path under a boot hook.
cleanup() {
    kill -9 "$PID" 2>/dev/null || true
    i=0
    while [ -e "/proc/$PID" ] && [ $i -lt 20 ]; do
        sleep 0.1
        i=$((i+1))
    done
    [ -n "$FEED" ] && kill "$FEED" 2>/dev/null || true
    [ -n "$REAPER" ] && kill "$REAPER" 2>/dev/null || true
    rm -f /tmp/ntkcec /tmp/oa-wdt-stop
}
trap 'cleanup; exit 0' INT TERM EXIT

# Wait for the control socket and the raw CEC acquire (bounded).
i=0
while [ $i -lt 80 ]; do
    [ -S "$CTL" ] && break
    sleep 0.1
    i=$((i+1))
done
OPEN=0
i=0
while [ $i -lt 100 ]; do
    S=$(ctl cec-state)
    echo "$S" >>"$LOG"
    case "$S" in *"open 1"*) OPEN=1; break ;; esac
    sleep 0.1
    i=$((i+1))
done
if [ "$OPEN" != 1 ]; then
    echo "cec-first: acquire did not complete" >>"$LOG"
    sync
    cleanup
    trap - INT TERM EXIT
    exit 0
fi

# Phase 1: observe the bus. CEC devices poll periodically, so any captured
# frame tells us which logical address is actually active before we transmit.
echo "observe start" >>"$LOG"
i=0
while [ $i -lt 15 ]; do
    S=$(ctl cec-state)
    case "$S" in *"rx 0"*) ;; *) break ;; esac
    sleep 0.2
    i=$((i+1))
done
echo "observe state: $(ctl cec-state)" >>"$LOG"

# Phase 2: announce the TV at LA0 / PA 0.0.0.0, then poll and query the
# device that ACKed in boot 4 (LA4). The stock VPL path performs a logical
# address claim/announce; the raw path only sets the controller LA, so send
# the announcement explicitly before expecting replies.
send_wait() {
    frame=$1
    delay=$2
    echo "send $frame: $(ctl cec-send "$frame")" >>"$LOG"
    sleep "$delay"
    echo "state after $frame: $(ctl cec-state)" >>"$LOG"
}
send_wait 0f84000000 1
send_wait 04 1
send_wait 0483 5
send_wait 0446 5
send_wait 048f 5

echo "final state: $(ctl cec-state)" >>"$LOG"
RXLINES=$(grep -c "cec rx" "$LOG" 2>/dev/null || true)
echo "cec-first: rx frame lines=$RXLINES" >>"$LOG"
if grep -q "cec rx .* op 84" "$LOG" 2>/dev/null; then
    echo "cec-first: report-physical-address received" >>"$LOG"
else
    echo "cec-first: no op 84 reply in log" >>"$LOG"
fi
sync

cleanup
trap - INT TERM EXIT
exit 0
