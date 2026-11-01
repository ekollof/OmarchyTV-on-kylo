#!/bin/sh
# Export this firmware's verified rail APNG into the UI's lossless OAI1 blobs.
# Vendor bytes/output stay local under artifacts/workbench, outside git history.
# Usage: tools/export-rail-assets.sh /absolute/new/output-directory
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
[ "$#" -eq 1 ] || { echo "usage: $0 /absolute/new/output-directory" >&2; exit 2; }
out=$1
case "$out" in /*) ;; *) echo 'output must be an absolute new directory' >&2; exit 2;; esac
pack=$repo/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_img.bin
index=$repo/artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_img.txt
name=mainlauncher_item_highlight_animated.png
[ -f "$pack" ] && [ -f "$index" ] || { echo 'extract vendor2:/dynamic_ui offline first' >&2; exit 1; }
offset=$(awk -v name="$name" '
    NF >= 6 {
        if ($2 == name) {
            if ($3 != "0x4936d1be" || $4 != 210 || $5 != 210 || $6 != 5001) exit 2
            print offset+0; found=1; exit
        }
        if ($6 !~ /^[0-9]+$/) exit 2
        offset += $6
    }
    END { if (!found) exit 1 }
' "$index")
mkdir -- "$out" # refuse to overwrite an earlier export or owner assets
work=$(mktemp -d "$out/.rail-export.XXXXXX")
trap 'rm -rf -- "$work"' EXIT HUP INT TERM
dd if="$pack" of="$work/source.apng" bs=65536 skip="$offset" count=5001 iflag=skip_bytes,count_bytes status=none
expected=d49c1fe5822ea814d7a97234c13e39ae8977557024d9667f3c538a170e0f4267
actual=$(sha256sum "$work/source.apng")
[ "${actual%% *}" = "$expected" ] || { echo 'target APNG hash mismatch' >&2; exit 1; }
ffprobe -v error -show_entries stream=width,height,pix_fmt -of csv=p=0 "$work/source.apng" > "$work/profile"
[ "$(cat "$work/profile")" = '210,210,rgba' ] || { echo 'unexpected APNG profile' >&2; exit 1; }
ffprobe -v error -show_entries frame=duration_time -of csv=p=0 "$work/source.apng" > "$work/durations"
awk '$0 != "0.030000" {bad=1} END {exit bad || NR != 11}' "$work/durations"
ffmpeg -v error -ignore_loop 1 -i "$work/source.apng" -threads 1 -fps_mode passthrough -f rawvideo -pix_fmt rgba "$work/frames.rgba"
[ "$(wc -c < "$work/frames.rgba")" -eq 1940400 ] || { echo 'decoded frame count/size mismatch' >&2; exit 1; }
# Independent first-frame decoder checks orientation, channels and alpha bytes.
magick "$work/source.apng[0]" -depth 8 "RGBA:$work/first-magick.rgba"
dd if="$work/frames.rgba" of="$work/first-ffmpeg.rgba" bs=176400 count=1 status=none
cmp "$work/first-magick.rgba" "$work/first-ffmpeg.rgba"
frame=0
while [ "$frame" -lt 11 ]; do
    file=$(printf '%s/rail_hl%02d.rgba' "$out" "$((frame+1))")
    # OAI1 + little-endian uint32(210),uint32(210), then straight RGBA bytes.
    printf 'OAI1\322\000\000\000\322\000\000\000' > "$file"
    dd if="$work/frames.rgba" bs=176400 skip="$frame" count=1 status=none >> "$file"
    [ "$(wc -c < "$file")" -eq 176412 ]
    frame=$((frame+1))
done
{
    printf 'firmware image pack: %s\nindex offset: %s\nAPNG SHA256: %s\n' "$pack" "$offset" "$expected"
    printf 'profile: 210x210, RGBA8, 11 frames, 30ms, loop; OAI1 pixels remain RGBA\n'
    printf 'first frame: independent ffmpeg/ImageMagick bytes identical\n'
    sha256sum "$pack" "$index"
    ffmpeg -version | head -1
    magick -version | head -1
} > "$out/PROVENANCE.txt"
(cd "$out" && sha256sum rail_hl*.rgba) > "$out/SHA256SUMS"
printf 'exported11 verified OAI1 frames to %s\n' "$out"
