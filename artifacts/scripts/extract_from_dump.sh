#!/bin/sh
# Extract any file from the pristine NAND dump - no need to wake the TV.
#
#   artifacts/scripts/extract_from_dump.sh <volume>:<path> [-o <outdir>]
#
#   artifacts/scripts/extract_from_dump.sh vendor:/lib/dfb/directfb-1.7-1/gfxdrivers/libdirectfb_tdga.so
#   artifacts/scripts/extract_from_dump.sh vendor2:/dynamic_ui -o artifacts/workbench/out
#   artifacts/scripts/extract_from_dump.sh root:/etc/init.d
#   artifacts/scripts/extract_from_dump.sh --list-volumes
#
# volumes (all inside mtd12 "fs0", a UBI device with 4 volumes):
#   root    = vol fs0   -> /            (squashfs rootfs)
#   vendor  = vol ap0   -> /vendor      (squashfs, aurora.elf, libs)
#   vendor2 = vol ap1   -> /vendor2     (squashfs, dynamic_ui, html apps)
#   nvt_sign            -> the 3 RSA signature slots (not a filesystem)
#
# First run carves the partition out of dumps/tv_nand.bin and unpacks the volume into a cache
# (artifacts/workbench/rootfs/<volume>); later runs are instant. Requires ubireader + unsquashfs.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
DUMP=${DUMP:-$REPO/artifacts/dumps/tv_nand.bin}
WORK=${WORK:-$REPO/artifacts/workbench/rootfs}
UBIREADER=${UBIREADER:-$REPO/artifacts/workbench/venv/bin/ubireader_extract_images}
UBIREADER_PYTHON=${UBIREADER_PYTHON:-$REPO/artifacts/workbench/venv/bin/python3}
FS0_OFF=0x2040000          # mtd12 "fs0" in NAND
FS0_SIZE=0x0c300000

vol_of() {
    case "$1" in
        root)   echo fs0 ;;
        vendor) echo ap0 ;;
        vendor2) echo ap1 ;;
        *)      echo "" ;;
    esac
}

list_volumes() {
    echo "volumes: root(fs0) vendor(ap0) vendor2(ap1)"
}

[ $# -ge 1 ] || { sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 1; }
[ "$1" = "--list-volumes" ] && { list_volumes; exit 0; }

TARGET=$1; shift
[ $# -ge 2 ] && [ "$1" = "-o" ] && OUT=$2 || OUT=${OUT:-$REPO/artifacts/workbench/out}

VOL=${TARGET%%:*}; PATH_IN=${TARGET#*:}
NAME=$(vol_of "$VOL")
[ -n "$NAME" ] || { echo "unknown volume '$VOL' ($(list_volumes))" >&2; exit 1; }
[ "$PATH_IN" = "$TARGET" ] && PATH_IN=/      # "vendor" with no path = list the root
SRC=$WORK/$VOL

if [ ! -d "$SRC" ]; then
    mkdir -p "$WORK"
    IMG=$(find "$WORK" -name "*_vol-$NAME.ubifs" 2>/dev/null | head -1)
    if [ -z "$IMG" ]; then
        if [ ! -f "$WORK/fs0_ubi.img" ]; then
            [ -f "$DUMP" ] || { echo "no dump at $DUMP" >&2; exit 1; }
            echo "== carving mtd12 (fs0) from $DUMP"
            python3 - "$DUMP" "$WORK/fs0_ubi.img" "$FS0_OFF" "$FS0_SIZE" <<'PY'
import sys
src, out, off, size = sys.argv[1], sys.argv[2], int(sys.argv[3], 0), int(sys.argv[4], 0)
with open(src, "rb") as s, open(out, "wb") as o:
    s.seek(off)
    o.write(s.read(size))
PY
        fi
        echo "== extracting UBI volumes"
        "$UBIREADER_PYTHON" "$UBIREADER" -o "$WORK/ubi" "$WORK/fs0_ubi.img" >/dev/null
        find "$WORK/ubi" -name '*.ubifs' -exec mv {} "$WORK/" \;
    fi
    IMG=$WORK/$(ls "$WORK" | grep "_vol-$NAME.ubifs$" | head -1)
    [ -f "$IMG" ] || { echo "volume image for $NAME not found" >&2; exit 1; }
    echo "== unsquashfs $(basename "$IMG") -> $SRC"
    unsquashfs -q -f -no-xattrs -d "$SRC" "$IMG" >/dev/null
fi

if [ -e "$SRC$PATH_IN" ]; then
    if [ -d "$SRC$PATH_IN" ]; then
        echo "# $VOL$PATH_IN (directory, listing)"
        ls -l "$SRC$PATH_IN" | head -50
    else
        mkdir -p "$OUT"
        cp -f "$SRC$PATH_IN" "$OUT/"
        echo "$OUT/$(basename "$PATH_IN")"
        md5sum "$OUT/$(basename "$PATH_IN")"
    fi
else
    echo "not found: $VOL:$PATH_IN (try: find $SRC -name '$(basename "$PATH_IN")')" >&2
    exit 1
fi
