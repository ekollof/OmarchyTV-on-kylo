#!/bin/sh
# Merge the raw apdat0 partition image into a full NAND image.
#
#   artifacts/scripts/make_nand_image.sh [out.bin]
#
# The result is a 512 MB image you can write with dd to a NAND chip (programmer) or keep as a
# complete restore image. Only the apdat0 partition is replaced; every other partition (incl. the
# signed fs0/ap0/ap1 and the TPS/vhash data) is byte-identical to the pristine dump.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
SRC=$REPO/artifacts/dumps/tv_nand.bin
IMG=$REPO/artifacts/payload/apdat0_mtd13.img
OUT=${1:-$REPO/artifacts/dumps/tv_nand_hacked.bin}
OFF=0x0E340000          # mtd13 "apdat0" offset in NAND (from /proc/mtd)
SIZE=0x00A00000         # mtd13 size (10 MB, 40 x 256 KB PEBs)

python3 - "$SRC" "$IMG" "$OUT" "$OFF" "$SIZE" <<'PY'
import sys, os, hashlib
src, img, out, off, size = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4], 0), int(sys.argv[5], 0)
i = open(img, "rb").read()
if len(i) != size:
    sys.exit(f"partition image is {len(i)} bytes, expected {size}")
if i[:4] != b"UBI#":
    sys.exit("partition image does not start with the UBI magic")

print("copying base image ...", os.path.getsize(src), "bytes")
with open(out, "wb") as o, open(src, "rb") as s:
    while True:
        b = s.read(8 << 20)
        if not b: break
        o.write(b)

with open(out, "r+b") as f:
    f.seek(off)
    f.write(i)
    f.flush()

h = hashlib.md5(open(out, "rb").read()).hexdigest()
print(f"wrote {out} ({os.path.getsize(out)} bytes, md5 {h})")
print(f"  apdat0 merged at 0x{off:08x} .. 0x{off+size:08x}")
PY
