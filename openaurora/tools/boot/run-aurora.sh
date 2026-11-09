#!/bin/sh
# App-manager idtv entry point for the replacement bundle.
#
# This file is not installed by the build or by takeover-full.sh. It is the
# intended replacement for /mtd_apexe/board/run_aurora.sh once the owner
# authorizes persistent boot integration.
#
# Staged cold boot: the wrapper refuses to start the replacement unless the
# pre-init recovery is present (boot-attempt gate + external watchdog feeder).
# It selects openaurora-stageN.conf from /conf/oa_stage, records the attempt,
# starts the feeder, then hands off to oa-supervise --wdt-keep --gate /conf.
# A failed or hung boot leaves the attempt token; the next boot counts it and
# three unproven attempts disable the native path.
#
# OA_DRYRUN=1 prints the commands and exits 0 without executing anything.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
stock=${OA_STOCK:-/vendor/com.nvt.idtv/aurora.elf}
log=${OA_APPLOG:-/tmp/oa-appmgr.log}
gate=${OA_GATE:-/conf}
node=${OA_WDT_NODE:-/proc/nvtstbcipc}

stage=0
if [ -r "$gate/oa_stage" ]; then
    stage=$(cat "$gate/oa_stage" 2>/dev/null || echo 0)
fi
case $stage in
0|1|2|3|4) ;;
*) stage=0 ;;
esac
cfg=$here/openaurora-stage$stage.conf
[ -r "$cfg" ] || cfg=$here/openaurora.conf

if [ "${OA_DRYRUN:-0}" = 1 ]; then
    echo "gate=$here/oa-boot-gate.sh $gate prepare"
    echo "feeder=$here/oa-wdtfeed --node $node --stop $gate/oa_wdt_stop"
    echo "supervise=$here/oa-supervise"
    echo "child=$here/openaurora --modules $here/modules --config $cfg --sock /tmp/openaurora.sock"
    echo "fallback=$stock"
    exit 0
fi

# Keep app-manager stderr/stdout from disappearing into its process group.
exec >>"$log" 2>&1
echo "=== $(date) run-aurora $here stage=$stage"

[ -x "$here/openaurora" ] || { echo "run-aurora: missing openaurora"; exec "$stock"; }
[ -x "$here/oa-supervise" ] || { echo "run-aurora: missing oa-supervise"; exec "$stock"; }
[ -x "$here/oa-boot-gate.sh" ] || { echo "run-aurora: missing boot gate"; exec "$stock"; }
[ -x "$here/oa-wdtfeed" ] || { echo "run-aurora: missing watchdog feeder"; exec "$stock"; }
[ -r "$cfg" ] || { echo "run-aurora: missing config"; exec "$stock"; }
[ -d "$here/modules" ] || { echo "run-aurora: missing modules"; exec "$stock"; }

if [ -r "$here/manifest.sha256" ] && command -v sha256sum >/dev/null 2>&1; then
    if ! (cd "$here" && sha256sum -c manifest.sha256 >/dev/null); then
        echo "run-aurora: manifest verification failed"
        exec "$stock"
    fi
fi

rm -f "$gate/oa_wdt_stop"
if ! "$here/oa-boot-gate.sh" "$gate" prepare; then
    echo "run-aurora: boot gate blocked"
    exec "$stock"
fi
if ! "$here/oa-boot-gate.sh" "$gate" start "$stage"; then
    echo "run-aurora: cannot record the boot attempt"
    exec "$stock"
fi

# The feeder must be running before the risky hardware init. It daemonizes and
# exits 0 only after the IPC node opened, so a failure here is fail-closed.
if ! "$here/oa-wdtfeed" --node "$node" --interval 3 --timeout 10 \
        --stop "$gate/oa_wdt_stop" --log /conf/tvhack.log; then
    echo "run-aurora: watchdog feeder failed to start"
    exec "$stock"
fi

rm -f /tmp/openaurora.alive /tmp/openaurora.ready /tmp/openaurora.sock
exec "$here/oa-supervise" \
    --heartbeat /tmp/openaurora.alive --ready /tmp/openaurora.ready \
    --wdt /buffer/wdtctl --fallback "$stock" \
    --gate "$gate" --wdt-keep \
    -- "$here/openaurora" --modules "$here/modules" \
       --config "$cfg" --sock /tmp/openaurora.sock
