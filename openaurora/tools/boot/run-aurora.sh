#!/bin/sh
# App-manager idtv entry point for the replacement bundle.
#
# This file is not installed by the build or by takeover-full.sh. It is the
# intended replacement for /mtd_apexe/board/run_aurora.sh once the owner
# authorizes persistent boot integration. It starts oa-supervise in this
# directory, which disables the watchdog, runs openaurora, and execs the vendor
# aurora.elf on failure or quit. If the bundle is incomplete it execs stock.
#
# OA_DRYRUN=1 prints the command and exits 0 without executing anything.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
stock=${OA_STOCK:-/vendor/com.nvt.idtv/aurora.elf}
log=${OA_APPLOG:-/tmp/oa-appmgr.log}

if [ "${OA_DRYRUN:-0}" = 1 ]; then
    echo "supervise=$here/oa-supervise"
    echo "child=$here/openaurora --modules $here/modules --config $here/openaurora.conf --sock /tmp/openaurora.sock"
    echo "fallback=$stock"
    exit 0
fi

# Keep app-manager stderr/stdout from disappearing into its process group.
exec >>"$log" 2>&1
echo "=== $(date) run-aurora $here"

[ -x "$here/openaurora" ] || { echo "run-aurora: missing openaurora"; exec "$stock"; }
[ -x "$here/oa-supervise" ] || { echo "run-aurora: missing oa-supervise"; exec "$stock"; }
[ -r "$here/openaurora.conf" ] || { echo "run-aurora: missing config"; exec "$stock"; }
[ -d "$here/modules" ] || { echo "run-aurora: missing modules"; exec "$stock"; }

if [ -r "$here/manifest.sha256" ] && command -v sha256sum >/dev/null 2>&1; then
    if ! (cd "$here" && sha256sum -c manifest.sha256 >/dev/null); then
        echo "run-aurora: manifest verification failed"
        exec "$stock"
    fi
fi

rm -f /tmp/openaurora.alive /tmp/openaurora.ready /tmp/openaurora.sock
exec "$here/oa-supervise" \
    --heartbeat /tmp/openaurora.alive --ready /tmp/openaurora.ready \
    --wdt /buffer/wdtctl --fallback "$stock" \
    -- "$here/openaurora" --modules "$here/modules" \
       --config "$here/openaurora.conf" --sock /tmp/openaurora.sock
