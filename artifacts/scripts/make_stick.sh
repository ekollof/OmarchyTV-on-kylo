#!/bin/sh
# Populate a FAT32 stick as the TV's flash/recovery stick (U-Boot "hold OK at power-on" channel).
#
#   artifacts/scripts/make_stick.sh [/mnt/tvusb]
#   SKIP_NAND=1 artifacts/scripts/make_stick.sh      # skip the 512 MB full-NAND chunks
#
# The TV only ever reads ONE script: `kylo_usb_update.scr` (must end with a '%' line).
# Everything else on the stick is payload or an alternative script you rename when needed.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
DST=${1:-/mnt/tvusb}
P=$REPO/artifacts/payload
S=$REPO/artifacts/scripts
D=$REPO/artifacts/dumps

[ -d "$DST" ] || { echo "no such mount point: $DST" >&2; exit 1; }

echo "== 1/5 images"
cp -f "$P/apdat0_full.ubifs" "$P/apdat0_orig.ubifs" "$DST/"

echo "== 2/5 active script (apply the hook payload)"
cp -f "$S/apply_apdat0_hook.scr" "$DST/kylo_usb_update.scr"

echo "== 3/5 alternative scripts (rename one to kylo_usb_update.scr to use it)"
for f in restore_apdat0.scr noop_write_apdat0.scr restore_full_nand.scr nand_dump.scr spi_dump.scr; do
    cp -f "$S/$f" "$DST/$f"
done

echo "== 4/5 full NAND chunks"
if [ "${SKIP_NAND:-0}" = "1" ]; then
    echo "   skipped (SKIP_NAND=1)"
else
    i=0
    while [ $i -lt 64 ]; do
        dd if="$D/tv_nand.bin" of="$DST/$(printf 'n%02d.bin' $i)" bs=8M skip=$i count=1 status=none
        i=$((i+1))
    done
    echo "   64 x 8 MB written"
fi

echo "== 5/5 README + checksums"
FULL_MD5=$(md5sum "$P/apdat0_full.ubifs" | cut -d' ' -f1)
ORIG_MD5=$(md5sum "$P/apdat0_orig.ubifs" | cut -d' ' -f1)
HOOK_MD5=$(md5sum "$P/aurora.elf" | cut -d' ' -f1)

cat > "$DST/README.txt" <<EOF
tvhack stick - TELEFUNKEN D65U546A / Vestel MB230 (192.168.178.87)
generated $(date '+%Y-%m-%d %H:%M')

HOW TO USE
  1. TV off (pull the plug), stick in a USB port
  2. hold the OK key on the remote and reconnect power
  3. the TV reads kylo_usb_update.scr and runs it (~1-2 min); it reboots after

ACTIVE SCRIPT: kylo_usb_update.scr  (= apply_apdat0_hook.scr)
  writes apdat0_full.ubifs into the UBI volume apdat0 (/conf) and reads it back
  to apdat0_readback.bin for verification.
  apdat0_full.ubifs  md5 $FULL_MD5   (hook aurora.elf md5 $HOOK_MD5)

ALTERNATIVES (rename the one you want to kylo_usb_update.scr)
  restore_apdat0.scr     writes apdat0_orig.ubifs (stock /conf, no sshd)  md5 $ORIG_MD5
  noop_write_apdat0.scr  writes apdat0_orig.ubifs unchanged + read-back (write-path test)
  restore_full_nand.scr  LAST RESORT: rewrites all 512 MB from n00..n63.bin
  nand_dump.scr          dumps the whole NAND to n00..n63.bin
  spi_dump.scr           dumps the 2 MB SPI flash to spi0..7.bin

NOTES
  - Only apdat0 (/conf) is unsigned; fs0/ap0/ap1 are signature-verified at boot, never write them.
  - Keep scripts under 8 KB and ending with a '%' line.
  - After a restore, the TV is stock again: no ssh, no DIAL, no launcher.
EOF

( cd "$DST" && md5sum apdat0_full.ubifs apdat0_orig.ubifs > md5sum.txt )
sync
echo "== done: $(df -h "$DST" | awk 'NR==2{print $4" free"}')"
ls -la "$DST" | head -12
cat "$DST/md5sum.txt"
