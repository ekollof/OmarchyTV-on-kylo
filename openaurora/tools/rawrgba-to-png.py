#!/usr/bin/env python3
"""Convert a raw RGBA recorder capture to PNG.

--over-black composites straight-alpha recorder pixels over an opaque black
background, matching the fb1/OSD result; without it the RGBA capture is kept.
"""
import sys
from PIL import Image

args = sys.argv[1:]
over_black = bool(args and args[0] == "--over-black")
if over_black:
    args = args[1:]
if len(args) != 4:
    raise SystemExit(f"usage: {sys.argv[0]} [--over-black] RAW WIDTH HEIGHT PNG")
raw, width, height, png = args[0], int(args[1]), int(args[2]), args[3]
with open(raw, "rb") as f:
    data = f.read()
if len(data) != width * height * 4:
    raise SystemExit(f"size mismatch: {len(data)} != {width*height*4}")
image = Image.frombytes("RGBA", (width, height), data)
if over_black:
    background = Image.new("RGBA", image.size, (0, 0, 0, 255))
    image = Image.alpha_composite(background, image).convert("RGB")
image.save(png)
print(f"{png} {width}x{height}")
