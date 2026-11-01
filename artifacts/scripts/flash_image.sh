#!/bin/sh
# Write the raw apdat0 partition image, either into a host file/NAND image or straight to the TV.
#
#   artifacts/scripts/flash_image.sh host <img> <target> [offset]   # e.g. tv_nand.bin 0x0E340000
#   artifacts/scripts/flash_image.sh tv <img> [--yes]               # write mtd13 on the TV and reboot
#
# Target facts (from /proc/mtd on the TV):
#   mtd13 "apdat0"  size 0x00A00000 (10 MB, 40 x 256 KB PEBs)  at NAND offset 0x0E340000
#   it is the only UNSIGNED volume -> safe to flash; fs0/ap0/ap1 are signature-checked at boot.
#
# The TV mode detaches the UBI device first, so the mounted /conf is gone until the reboot that
# follows. Everything is logged; the image is verified before writing.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
IMG_DEFAULT=$REPO/artifacts/payload/apdat0_mtd13.img
TV=${TV:-192.168.178.87}

usage() {
    sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
    exit 1
}

[ $# -ge 2 ] || usage
MODE=$1; IMG=$2; shift 2

[ -f "$IMG" ] || { echo "no such image: $IMG" >&2; exit 1; }
SIZE=$(wc -c < "$IMG")
[ "$SIZE" -eq 10485760 ] || { echo "unexpected image size $SIZE (want 10485760)" >&2; exit 1; }
head -c 4 "$IMG" | grep -q 'UBI#' || { echo "image does not start with UBI#" >&2; exit 1; }
echo "image: $IMG ($SIZE bytes, md5 $(md5sum "$IMG" | cut -d' ' -f1))"

case "$MODE" in
  host)
    TARGET=$1; OFFSET=${2:-0}
    [ -n "$TARGET" ] || usage
    echo "writing $IMG -> $TARGET at offset $OFFSET"
    python3 - "$IMG" "$TARGET" "$OFFSET" <<'PY'
import sys, os
img, target, off = sys.argv[1], sys.argv[2], int(sys.argv[3], 0)
data = open(img, "rb").read()
if not os.path.exists(target):
    sys.exit(f"{target} does not exist (create/truncate it first)")
with open(target, "r+b") as f:
    f.seek(off)
    f.write(data)
    f.flush()
print(f"ok: {len(data)} bytes written at 0x{off:08x}")
PY
    ;;

  tv)
    [ "$1" = "--yes" ] || { echo "refusing to touch the TV without --yes" >&2; echo "(this detaches UBI and reboots it)" >&2; exit 1; }
    echo "== copying image to the TV"
    "$HERE/../tools/tvssh.sh" 'cat > /tmp/apdat0_mtd13.img' < "$IMG"
    echo "== verifying on the TV"
    "$HERE/../tools/tvssh.sh" 'md5sum /tmp/apdat0_mtd13.img; wc -c < /tmp/apdat0_mtd13.img'
    echo "== detaching UBI, writing mtd13, rebooting"
    "$HERE/../tools/tvssh.sh" '
        sync
        for d in /dev/ubi13*; do [ -e "$d" ] && ubidetach -d 13 2>/dev/null || true; done
        sleep 1
        dd if=/tmp/apdat0_mtd13.img of=/dev/mtd13 bs=64k conv=fsync 2>&1 | tail -2
        sync
        rm -f /tmp/apdat0_mtd13.img
        echo "flash done; rebooting"
        nohup sh -c "sleep 2; reboot -f" >/dev/null 2>&1 &
    '
    echo "== waiting for the TV to come back"
    sleep 45
    "$HERE/../tools/tvssh.sh" 'echo up; grep -a launcher /conf/tvhack.log | tail -3'
    ;;
  *)
    usage
    ;;
esac
