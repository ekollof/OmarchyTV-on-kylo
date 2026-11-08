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
    # Editable Lua widget data (the script itself is in ./lua and may be
    # overridden by ui.lua.script in openaurora.conf).
    mkdir -p /buffer/openaurora/lua-data
    # Snapshot the optical/SPDIF controls while stock is still running.
    if [ -x ./oa-alsactl ] && [ -r /vendor/etc/alsa.conf ]; then
        mkdir -p /buffer/oa-audio-before
        if ALSA_CONFIG_PATH=/vendor/etc/alsa.conf ./oa-alsactl --card 0 \
                save /buffer/oa-audio-before/card0.txt; then
            grep -E '^(nvt opu hw volume|main out source select|main out source connect|spdif bypass|hw out mute)[[:space:]]' \
                /buffer/oa-audio-before/card0.txt \
                > /buffer/oa-audio-before/optical.txt || true
            echo "audio snapshot: $(wc -l < /buffer/oa-audio-before/optical.txt) optical controls"
        fi
    fi
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
    # Snapshot the Settings columns this run may change. Restore them before
    # quit so the stock app re-reads them on restart. The DB copy is a manual
    # recovery aid; it is not restored automatically.
    mkdir -p /buffer/oa-settings-before
    : > /buffer/oa-settings-before/values.txt
    for key in m_volume m_current_source m_cec_enabled; do
        value=$(./oa-client /tmp/openaurora.sock settings-get "$key" 2>/dev/null || true)
        case "$value" in
            ''|*[!0-9-]*) ;;
            *) echo "$key $value" >> /buffer/oa-settings-before/values.txt ;;
        esac
    done
    cp -f /conf/sqlite_db/settingsDB.db \
        /buffer/oa-settings-before/settingsDB.db 2>/dev/null || true
    echo "settings snapshot: $(wc -l < /buffer/oa-settings-before/values.txt) columns"
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
    if [ -s /buffer/oa-settings-before/values.txt ]; then
        while read -r key value; do
            if ./oa-client /tmp/openaurora.sock settings-set "$key" "$value"; then
                echo "settings restore $key=$value -> ok"
            else
                echo "settings restore $key=$value -> failed" >&2
            fi
        done < /buffer/oa-settings-before/values.txt
    fi
    ./oa-client /tmp/openaurora.sock quit
    sleep 5
    if [ -x ./oa-alsactl ] && [ -s /buffer/oa-audio-before/optical.txt ] &&
       [ -r /vendor/etc/alsa.conf ]; then
        if ALSA_CONFIG_PATH=/vendor/etc/alsa.conf ./oa-alsactl --card 0 \
                restore /buffer/oa-audio-before/optical.txt; then
            echo "optical control restore -> ok"
        else
            echo "optical control restore -> failed" >&2
        fi
        # Any nonzero OPU hardware attenuation (observed 0 12000 12000)
        # silences the optical soundbar. Healthy playback uses 0 0 0.
        if ALSA_CONFIG_PATH=/vendor/etc/alsa.conf ./oa-alsactl --card 0 \
                setv "nvt opu hw volume" 0 0 0; then
            echo "opu volume restore -> ok"
        else
            echo "opu volume restore -> failed" >&2
        fi
        ALSA_CONFIG_PATH=/vendor/etc/alsa.conf ./oa-alsactl --card 0 \
            get "nvt opu hw volume" || true
    fi
    echo "verify stock:"
    ps -eo pid,ppid,args | grep -E '[a]urora.elf|[o]penaurora|[o]a-supervise' || true
    ;;
*)
    echo "usage: $0 start|status|reload-suite|restore" >&2
    exit 2
    ;;
esac
