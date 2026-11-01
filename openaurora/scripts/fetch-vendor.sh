#!/bin/sh
# Fetch the vendor libraries we link against, straight out of the firmware dump.
#
#   openaurora/scripts/fetch-vendor.sh            # into openaurora/vendor/
#   OUT=/somewhere openaurora/scripts/fetch-vendor.sh
#
# The firmware is fixed for us (we do not update it), so these libraries are a stable
# link target rather than a moving dependency. They are Vestel/Novatek copyright:
# extract them locally, do not redistribute.
#
# Needs the NAND dump (artifacts/dumps/tv_nand.bin) and the offline extractor.
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
PROJ=$(cd "$HERE/.." && pwd)
TVHACK=$(cd "$PROJ/.." && pwd)
OUT=${OUT:-$PROJ/vendor}
EXTRACT="$TVHACK/artifacts/scripts/extract_from_dump.sh"

# name=volume:path  (libNDAL3 needs globals from libNDAL_EX, e.g. gs32Ndp_PQ_fd,
# and ALSA symbols; the ALSA lib lives one directory deeper in the image)
LIBS="libNDAL3.so=vendor:/lib/libNDAL3.so
libNDAL_EX.so=vendor:/lib/libNDAL_EX.so
libasound.so.2=vendor:/lib/libs/libasound.so.2
libsqlite3.so.0=vendor:/lib/libs/libsqlite3.so.0
libmhash.so.2=vendor:/lib/libmhash.so.2
libnteec.so=vendor:/lib/libnteec.so
libv4l2.so.0=vendor:/lib/libv4l2.so.0
libv4lconvert.so.0=vendor:/lib/libv4lconvert.so.0"

mkdir -p "$OUT/lib" "$OUT/tmp"

for entry in $LIBS; do
    lib=${entry%%=*}
    src=${entry#*=}
    printf 'fetching %s ... ' "$lib"
    if "$EXTRACT" "$src" -o "$OUT/tmp" >/dev/null 2>&1 && [ -f "$OUT/tmp/$lib" ]; then
        cp "$OUT/tmp/$lib" "$OUT/lib/$lib"
        printf 'ok (%s bytes)\n' "$(wc -c < "$OUT/lib/$lib")"
    else
        printf 'FAILED\n'
        exit 1
    fi
done

rm -rf "$OUT/tmp"

# linker name for -lsqlite3 (the firmware ships only the soname)
[ -f "$OUT/lib/libsqlite3.so.0" ] && ln -sf libsqlite3.so.0 "$OUT/lib/libsqlite3.so"

# Keep the extracted-library header identical to the maintained, checked ABI.
cp "$PROJ/include/openaurora/vendor_pwm.h" "$OUT/lib/vendor-hal.h"

printf 'vendor libs in %s/lib\n' "$OUT"
ls -l "$OUT/lib"
