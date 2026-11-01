#!/usr/bin/env python3
"""Grab the TV screen over ssh and save it as PNG.

The MB230 exposes its OSD layers as /dev/fb0..4. Nothing stops root from
reading them, so we get a real screenshot without any on-screen UI:

    fb0  OSD_NORMAL1  1920x3240  32bpp  (3 x 1080 frames)
    fb1  OSD_NORMAL2  1920x2160  32bpp  (2 x 1080 frames)
    fb2  OSD_NORMAL3  1280x1440  16bpp  (2 x 720 frames)
    fb3  OSD_CURSOR1   256x512   32bpp
    fb4  OSD_CURSOR2    32x64    32bpp

The launcher / menus are drawn into double-buffered fb1, full-screen app output into
fb0. An empty (all-zero) layer means "nothing drawn on this layer right now".

    tools/tvshot.py                      # fb1 (OSD_NORMAL2), all frames
    tools/tvshot.py --layer 0 --frames 1
    tools/tvshot.py --out /tmp/opencode/shot.png

Exit code 2 means every requested frame was blank.
Raw 32bpp OSD bytes are BGRA, not RGBA. 16bpp uses RGB565. Saved PNGs are
ordinary RGB images; their channels have already been decoded and must not be swapped again.
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TV_HOST = os.environ.get("TV_HOST", "192.168.178.87")
TV_KEY = os.environ.get("TV_KEY", os.path.expanduser("~/.ssh/id_ed25519"))

# frame height per layer name, used to split the triple-buffered mapping
FRAME_H = {
    "OSD_NORMAL1": 1080,
    "OSD_NORMAL2": 1080,
    "OSD_NORMAL3": 720,
    "OSD_CURSOR1": 256,
    "OSD_CURSOR2": 32,
}


def ssh(cmd, binary=False):
    args = [
        "/usr/bin/ssh", "-F", "/dev/null", "-T", "-i", TV_KEY,
        "-o", "IdentitiesOnly=yes", "-o", "StrictHostKeyChecking=no",
        "-o", "UserKnownHostsFile=/dev/null", "-o", "BatchMode=yes",
        f"root@{TV_HOST}", cmd,
    ]
    r = subprocess.run(args, capture_output=True)
    if r.returncode != 0:
        sys.exit(f"ssh failed: {r.stderr.decode(errors='replace').strip()}")
    return r.stdout if binary else r.stdout.decode(errors="replace")


def sysfs(layer, name):
    out = ssh(f"cat /sys/class/graphics/fb{layer}/{name} 2>/dev/null").strip()
    if not out:
        sys.exit(f"cannot read /sys/class/graphics/fb{layer}/{name}")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--layer", type=int, default=1, help="framebuffer index (default 1)")
    ap.add_argument("--frames", type=int, default=0, help="frames to save (default: all)")
    ap.add_argument("--out", default="/tmp/opencode/tvshot.png", help="output PNG (frame N gets _N)")
    ap.add_argument("--no-wake", action="store_true", help="skip WoL / port probe")
    ap.add_argument("--format", default="", choices=["RGBA", "BGRA"],
                    help="32bpp raw pixel order (default: BGRA on all measured OSD layers)")
    args = ap.parse_args()

    if not args.no_wake:
        subprocess.run([sys.executable, os.path.join(HERE, "ensure_awake.py"),
                        "--host", TV_HOST, "--quiet"], check=False)

    name = sysfs(args.layer, "name")
    w, h = (int(x) for x in sysfs(args.layer, "virtual_size").split(","))
    bpp = int(sysfs(args.layer, "bits_per_pixel"))
    fh = FRAME_H.get(name, h)
    nframes = max(1, h // fh)
    if args.frames:
        nframes = min(nframes, args.frames)

    total = w * h * bpp // 8
    raw = ssh(f"dd if=/dev/fb{args.layer} bs=65536 count={total // 65536 + 1} "
              f"2>/dev/null | head -c {total}", binary=True)
    if len(raw) < total:
        sys.exit(f"short read: {len(raw)} < {total}")

    try:
        from PIL import Image
    except ImportError:
        sys.exit("Pillow not installed (pip install pillow)")

    base, ext = os.path.splitext(args.out)
    # All measured 32bpp OSD layers are BGRA in memory: a test pattern with byte0=255 showed as
    # blue on the panel, byte1 as green, byte2 as red (tools/colorbars, 2026-10-30).  The old
    # "fb1 is RGBA" assumption in this tool produced red/blue-swapped captures.
    if args.format and bpp != 32:
        sys.exit("--format applies only to 32bpp captures; 16bpp uses RGB565")
    order = args.format or ("BGRA" if bpp == 32 else "RGB565")
    formats = {16: ("RGB", "BGR;16"), 32: ("RGBA", order)}
    if bpp not in formats:
        sys.exit(f"unsupported bpp {bpp}")
    mode, rawmode = formats[bpp]
    stride = w * bpp // 8

    blank = 0
    for i in range(nframes):
        off = i * fh * stride
        img = Image.frombytes(mode, (w, fh), raw[off:off + fh * stride],
                              "raw", rawmode).convert("RGB")
        path = f"{base}_{i}{ext or '.png'}"
        img.save(path)
        extrema = img.getextrema()
        is_blank = all(lo == hi for lo, hi in extrema)
        blank += is_blank
        print(f"fb{args.layer} ({name}) frame {i}/{nframes - 1}: {path} "
              f"{'BLANK' if is_blank else extrema}")
    if blank == nframes:
        sys.exit(2)


if __name__ == "__main__":
    main()
