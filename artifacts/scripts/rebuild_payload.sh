#!/bin/sh
# Rebuild artifacts/payload/apdat0_full.ubifs from the TV's live /conf.
#
# The image matches what actually runs (hook, dropbear, keys, sqlite3, privacy DB),
# so it is the one to put on the FAT32 stick as kylo_usb_update.scr + image.
#
#   artifacts/scripts/rebuild_payload.sh [--dry-run]
#
# Requires: tools/tvssh.sh (TV awake/reachable), mkfs.ubifs, ubireader_extract_files,
# passwordless sudo (needed to keep numeric owners 101 = idtv, 0 = root).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
STAGE=/tmp/opencode/apdat_files
OUT=$REPO/artifacts/payload/apdat0_full.ubifs
FULL=$((31 * 253952))   # 31 LEBs x 253952 = 7872512
UBIREADER=${UBIREADER:-$REPO/artifacts/workbench/venv/bin/ubireader_extract_files}
UBIREADER_PYTHON=${UBIREADER_PYTHON:-$REPO/artifacts/workbench/venv/bin/python3}

echo "== 1/5 snapshot /conf from the TV"
sudo rm -rf "$STAGE"
mkdir -p "$STAGE"
"$REPO/tools/tvssh.sh" 'tar -C /conf -cf - .' > /tmp/opencode/apdat_conf.tar
sudo tar -xpf /tmp/opencode/apdat_conf.tar -C "$STAGE"
sudo rm -f "$STAGE/tvhack.log" "$STAGE/tvhack.log.old"
# runtime state that must not ship in a base image:
sudo rm -f "$STAGE/launcher_ok" "$STAGE/launcher_fail"      # launcher success marker / streak
# Recovery images must boot the vendor UI, never resume a development takeover.
sudo rm -f "$STAGE/oa_boot" "$STAGE/oa_ok" "$STAGE/oa_fail" \
           "$STAGE/oa_trace_enable" "$STAGE/oa_hold" "$STAGE/oa_hold_preload"
[ "${KEEP_LAUNCHER_FLAG:-0}" = "1" ] || sudo rm -f "$STAGE/launcher_enable"
echo "launcher flag in image: $([ -f "$STAGE/launcher_enable" ] && echo enabled || echo disabled)" 
rm -f /tmp/opencode/apdat_conf.tar
sudo du -sk "$STAGE"

echo "== 2/5 sanity: db integrity + expected payload files"
for db in "$STAGE"/sqlite_db/settingsDB.db "$STAGE"/sqlite_db/settingsDBBackup.db; do
    result=$(sqlite3 "$db" 'pragma integrity_check;')
    echo "$(basename "$db"): $result"
    [ "$result" = ok ] || { echo 'invalid database snapshot -- aborting'; exit 1; }
done
for f in remotelf/aurora.elf remotelf/keepalive.sh remotelf/hosts remotelf/dropbear remotelf/authorized_keys remotelf/hostkey_rsa \
         remotelf/hostkey_ed25519 remote.txt sqlite3; do
    sudo test -e "$STAGE/$f" || { echo "MISSING $f -- aborting"; exit 1; }
done
[ -e "$STAGE/no_force_net" ] && echo "note: no_force_net present (self-heal disabled in this image)"
[ -e "$STAGE/no_dial" ] && echo "note: no_dial present (DIAL disabled in this image)"

echo "== 3/5 mkfs.ubifs"
sudo mkfs.ubifs -r "$STAGE" -m 4096 -e 253952 -c 31 -x zlib -o /tmp/opencode/apdat0_new.ubifs

echo "== 4/5 pad to $FULL bytes"
sudo python3 - "$FULL" <<'EOF'
import sys
full = int(sys.argv[1])
d = open('/tmp/opencode/apdat0_new.ubifs', 'rb').read()
if len(d) > full:
    sys.exit(f"image {len(d)} > volume {full} -- /conf too big")
open('/tmp/opencode/apdat0_full.ubifs', 'wb').write(d + b'\xff' * (full - len(d)))
print("payload", len(d), "padded", full)
EOF

echo "== 5/5 round-trip verify"
sudo rm -rf /tmp/opencode/verify_img
"$UBIREADER_PYTHON" "$UBIREADER" -o /tmp/opencode/verify_img /tmp/opencode/apdat0_full.ubifs >/dev/null 2>&1
sudo diff -r --no-dereference "$STAGE" /tmp/opencode/verify_img && echo "round-trip OK"

if [ "${1:-}" = "--dry-run" ]; then
    echo "dry run: NOT installing; image left at /tmp/opencode/apdat0_full.ubifs"
    md5sum /tmp/opencode/apdat0_full.ubifs
    exit 0
fi

sudo cp /tmp/opencode/apdat0_full.ubifs "$OUT"
sudo cp "$STAGE/remotelf/aurora.elf" "$REPO/artifacts/payload/aurora.elf"
sudo chown "$(id -u):$(id -g)" "$OUT" "$REPO/artifacts/payload/aurora.elf"
echo "== installed:"
md5sum "$OUT" "$REPO/artifacts/payload/aurora.elf"
echo "now update the md5 in TODO.md, MEMORY.md, artifacts/payload/README.md, AGENTS.md"
