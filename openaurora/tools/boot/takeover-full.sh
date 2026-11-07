#!/bin/sh
# Run from a deployed full-replacement bundle directory.
# start:   temporary takeover (stock stopped, watchdog disabled) under
#          oa-supervise, so reload-suite works and quit returns to stock
#          without rebooting.
# status:  module table and recent log.
# restore: ask the supervisor to exit; it re-arms the watchdog and execs stock.
# This script is intentionally not run by the build or tests.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$here"

case "${1:-}" in
start)
    sha256sum -c manifest.sha256 >/dev/null
    ./openaurora --check --modules ./modules
    /buffer/wdtctl disable
    for p in $(pidof aurora.elf 2>/dev/null || true); do
        kill -TERM "$p" 2>/dev/null || true
    done
    sleep 2
    rm -f /tmp/openaurora.alive /tmp/openaurora.ready \
          /tmp/openaurora.sock /tmp/oa-full.log
    nohup ./oa-supervise \
        --heartbeat /tmp/openaurora.alive --ready /tmp/openaurora.ready \
        --wdt /buffer/wdtctl \
        --fallback /vendor/com.nvt.idtv/aurora.elf \
        -- ./openaurora --modules ./modules --config ./openaurora.conf \
           --sock /tmp/openaurora.sock --log 3 > /tmp/oa-full.log 2>&1 &
    echo "supervisor=$!"
    sleep 8
    ./oa-client /tmp/openaurora.sock list
    ./oa-client /tmp/openaurora.sock display-info \
        "status plat" "status ui_vendor" "status audio" "status source" \
        "status tuner" "status cec" panel-state audio-state tuner-state cec-state
    ;;
status)
    ./oa-client /tmp/openaurora.sock list
    tail -40 /tmp/oa-full.log
    ;;
reload-suite)
    ./oa-client /tmp/openaurora.sock reload-suite
    sleep 5
    ./oa-client /tmp/openaurora.sock list
    ;;
restore)
    ./oa-client /tmp/openaurora.sock quit
    sleep 3
    echo "verify stock:"
    ps -eo pid,ppid,args | grep -E '[a]urora.elf|[o]penaurora|[o]a-supervise' || true
    ;;
*)
    echo "usage: $0 start|status|reload-suite|restore" >&2
    exit 2
    ;;
esac
