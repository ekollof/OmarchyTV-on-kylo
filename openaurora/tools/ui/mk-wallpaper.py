#!/usr/bin/env python3
"""Convert a still image to the ui.engine OAI1 raw-RGBA asset.

The OmarchyTV reference (https://gitlab.com/piermark/omarchytv) bundles
wallpapers with their own attribution/licence terms. This project does not
redistribute them; import a wallpaper you are allowed to use, for example:

  python3 tools/ui/mk-wallpaper.py \
      /path/to/omarchytv/launcher/app/src/main/assets/wallpapers/\
last-horizon/01-eyes-wide.webp \
      openaurora/ui/wallpaper.rgba --width 960 --height 540

The engine loads <ui.dir>/<name>.rgba and the skin requests the icon name
"wallpaper" (ui.omarchy.wallpaper). OAI1 is a 12-byte header ("OAI1",
uint32 LE width, uint32 LE height) followed by top-down straight RGBA rows.
"""
import argparse
import struct
import sys

from PIL import Image, ImageOps


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("source")
    ap.add_argument("output")
    ap.add_argument("--width", type=int, default=960)
    ap.add_argument("--height", type=int, default=540)
    args = ap.parse_args()
    if args.width < 1 or args.height < 1:
        ap.error("width and height must be positive")

    with Image.open(args.source) as im:
        im = im.convert("RGB")
        # Crop to the panel aspect, then scale. A launcher wallpaper must not
        # letterbox; the browser reference uses object-fit: cover.
        im = ImageOps.fit(im, (args.width, args.height), Image.LANCZOS)
        rgba = im.convert("RGBA").tobytes()

    with open(args.output, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", args.width, args.height))
        f.write(rgba)
    print(f"{args.output}: {args.width}x{args.height} {len(rgba) + 12} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
