#!/usr/bin/env python3
"""Convert a raw RGBA recorder capture to PNG."""
import sys
from PIL import Image

if len(sys.argv) != 5:
    raise SystemExit(f"usage: {sys.argv[0]} RAW WIDTH HEIGHT PNG")
raw, width, height, png = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4]
with open(raw, "rb") as f:
    data = f.read()
if len(data) != width * height * 4:
    raise SystemExit(f"size mismatch: {len(data)} != {width*height*4}")
Image.frombytes("RGBA", (width, height), data).save(png)
print(f"{png} {width}x{height}")
