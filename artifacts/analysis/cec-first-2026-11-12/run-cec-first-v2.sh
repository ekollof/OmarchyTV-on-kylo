#!/bin/sh
# Temporary CEC-first boot helper, second attempt (2026-11-12).
#
# Runs before stock Aurora, opens the raw ntkcec device, sends Give Physical
# Address to the Steam Deck at LA8, and records TX/RX. It never calls quit or
# wait: the test process is SIGKILLed after a bounded observation window, so a
# stuck CEC close cannot hold the boot hook. A self-terminating MCU feeder and
# an independent reaper bound even a killed shell.
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
# rerun the test even if the hook branch was not restored. The current shell
# keeps reading the moved file through its open descriptor.
if [ -x "$D/run-cec-first.sh" ]; then
    mv -f "$D/run-cec-first.sh" "$D/run-cec-first.sh.done" 2>/dev/null || true
fi

echo "== cec-first $(date) boot=$BOOT ==" >>"$LOG"
cd "$D" || exit 1
rm -f "$CTL" /tmp/ntkcec
mknod /tmp/ntkcec c 229 0 2>/dev/null || true

# Bounded control call: a blocked client cannot stall the harness. oa-client
# takes the whole command as one argument, so join the words with $*.
ctl() {
    timeout -t 2 -s KILL "$D/oa-client" "$CTL" "$*" 2>&1 || true
}

# Self-terminating MCU feeder. 20 beats at 2 s (40 s) outlives the 30 s hook
# timeout even if this shell is SIGKILLed, then stops by itself.
FEED=
if [ "$BOOT" = 1 ] && [ -x "$D/oa-wdtfeed" ]; then
    "$D/oa-wdtfeed" --node /proc/nvtstbcipc --interval 2 --timeout 10 \
        --beats 20 --log "$D/wdt.log" --foreground </dev/null >>"$LOG" 2>&1 &
    FEED=$!
fi

"$D/openaurora" --modules "$D/modules" --config "$D/cec.conf" \
    --sock "$CTL" --log 3 >>"$LOG" 2>&1 &
PID=$!

# Independent reaper: survives a SIGKILL of this shell and kills only our
# openaurora (exact cmdline), so stock Aurora can open the raw device.
(
    sleep 20
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

# Give Physical Address to the Deck at LA8. The module queues this while its
# receive job owns the single owner slot and transmits it on the next poll.
R=$(ctl cec-send 0883)
echo "send 0883: $R" >>"$LOG"

# Observe up to 8 s for TX completion and the Deck's Report Physical Address
# (opcode 84) reply. Do not infer success from the send response alone.
i=0
TX=0
RX=0
while [ $i -lt 40 ]; do
    S=$(ctl cec-state)
    case "$S" in *"tx 1"*) TX=1 ;; esac
    case "$S" in *"rx 1"*) RX=1 ;; esac
    [ "$TX" = 1 ] && [ "$RX" = 1 ] && break
    sleep 0.2
    i=$((i+1))
done
echo "final state: $(ctl cec-state)" >>"$LOG"
if grep -q "op 84" "$LOG" 2>/dev/null; then
    echo "cec-first: report-physical-address received" >>"$LOG"
else
    echo "cec-first: no op 84 reply in log" >>"$LOG"
fi
sync

cleanup
trap - INT TERM EXIT
exit 0
