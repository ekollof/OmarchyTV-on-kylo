#!/bin/sh
# Temporary CEC raw RX monitor (2026-11-12). Boots before stock Aurora, opens
# the raw ntkcec device, then waits for /tmp/cec-go before capturing for up to
# 90 s. The trigger lets the owner sleep/resume the Deck at a known time.
# Never calls quit/wait; SIGKILLs the module after the window. A
# self-terminating MCU feeder and independent reaper bound even a killed shell.
set -u
D=/buffer/oa-cec-monitor
LOG="$D/cec.log"
CTL="$D/control"
BOOT=0
[ "${1:-}" = "--boot" ] && BOOT=1

# One-shot guard: a failed/hung boot must not loop the monitor.
if [ -x "$D/run-cec-monitor.sh" ]; then
    mv -f "$D/run-cec-monitor.sh" "$D/run-cec-monitor.sh.done" 2>/dev/null || true
fi

: >"$LOG"
echo "== cec-monitor $(date) boot=$BOOT ==" >>"$LOG"
cd "$D" || exit 1
rm -f "$CTL" /tmp/ntkcec /tmp/cec-go /tmp/cec-stop /tmp/oa-wdt-stop
mknod /tmp/ntkcec c 229 0 2>/dev/null || true

ctl() {
    timeout -t 2 -s KILL "$D/oa-client" "$CTL" "$*" 2>&1 || true
}

# Feeder: 210 beats at 2 s (420 s) covers the armed wait and the capture.
FEED=
if [ "$BOOT" = 1 ] && [ -x "$D/oa-wdtfeed" ]; then
    "$D/oa-wdtfeed" --node /proc/nvtstbcipc --interval 2 --timeout 10 \
        --beats 210 --log "$D/wdt.log" --foreground </dev/null >>"$LOG" 2>&1 &
    FEED=$!
fi

"$D/openaurora" --modules "$D/modules" --config "$D/cec.conf" \
    --sock "$CTL" --log 3 >>"$LOG" 2>&1 &
PID=$!

# Independent reaper: kills only our openaurora if the shell is SIGKILLed.
(
    sleep 420
    CMD=$(cat "/proc/$PID/cmdline" 2>/dev/null || true)
    case "$CMD" in
        *oa-cec-monitor/openaurora*) kill -9 "$PID" 2>/dev/null ;;
    esac
    touch /tmp/oa-wdt-stop 2>/dev/null || true
) </dev/null >/dev/null 2>&1 &
REAPER=$!

cleanup() {
    kill -9 "$PID" 2>/dev/null || true
    i=0
    while [ -e "/proc/$PID" ] && [ $i -lt 20 ]; do
        sleep 0.1
        i=$((i+1))
    done
    [ -n "$FEED" ] && kill "$FEED" 2>/dev/null || true
    [ -n "$REAPER" ] && kill "$REAPER" 2>/dev/null || true
    rm -f /tmp/ntkcec /tmp/cec-go /tmp/cec-stop /tmp/oa-wdt-stop
}
trap 'cleanup; exit 0' INT TERM EXIT

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
    echo "monitor: acquire did not complete" >>"$LOG"
    sync
    cleanup
    trap - INT TERM EXIT
    exit 0
fi

echo "monitor armed; waiting for /tmp/cec-go" >>"$LOG"

# Armed wait: up to 300 s for the owner trigger.
i=0
while [ $i -lt 600 ]; do
    [ -f /tmp/cec-go ] && break
    sleep 0.5
    i=$((i+1))
done
if [ ! -f /tmp/cec-go ]; then
    echo "monitor: no trigger; exiting" >>"$LOG"
    sync
    cleanup
    trap - INT TERM EXIT
    exit 0
fi
rm -f /tmp/cec-go
echo "monitor start $(date)" >>"$LOG"

# Capture up to 90 s; stop early once Active Source plus Inactive/Standby are
# both present, or when /tmp/cec-stop is created.
i=0
while [ $i -lt 180 ]; do
    S=$(ctl cec-state)
    case "$S" in *"rx 0"*) ;; *) echo "state: $S" >>"$LOG" ;; esac
    if grep -q "cec rx .* op 82" "$LOG" 2>/dev/null &&
       { grep -q "cec rx .* op 9d" "$LOG" 2>/dev/null ||
         grep -q "cec rx .* op 36" "$LOG" 2>/dev/null; }; then
        echo "monitor: sleep+resume frames captured" >>"$LOG"
        break
    fi
    [ -f /tmp/cec-stop ] && break
    sleep 0.5
    i=$((i+1))
done
echo "monitor stop $(date)" >>"$LOG"
echo "final state: $(ctl cec-state)" >>"$LOG"
RXLINES=$(grep -c "cec rx" "$LOG" 2>/dev/null || true)
echo "monitor: rx frame lines=$RXLINES" >>"$LOG"
sync

cleanup
trap - INT TERM EXIT
exit 0
