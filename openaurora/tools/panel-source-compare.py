#!/usr/bin/env python3
"""Compare a recorder panel capture crop with an OAI1 panel source.

usage: panel-source-compare.py RAW WIDTH HEIGHT OAI1_PANEL OUT_PNG [X Y]

The recorder writes 1920x1080 straight RGBA. The OAI1 panel is cropped at
X,Y (default 0,0). Prints AE/MAE/changed over RGB and writes the cropped
capture as PNG.
"""
import struct
import sys

import numpy as np
from PIL import Image


def main():
    if len(sys.argv) not in (6, 8):
        raise SystemExit(__doc__)
    raw_path, width, height, panel_path, out_path = sys.argv[1:6]
    x = int(sys.argv[6]) if len(sys.argv) == 8 else 0
    y = int(sys.argv[7]) if len(sys.argv) == 8 else 0
    width, height = int(width), int(height)
    raw = np.fromfile(raw_path, dtype=np.uint8)
    if raw.size != width * height * 4:
        raise SystemExit(f"raw size {raw.size} != {width*height*4}")
    raw = raw.reshape(height, width, 4)

    with open(panel_path, "rb") as f:
        data = f.read()
    if data[:4] != b"OAI1":
        raise SystemExit("panel source is not OAI1")
    pw, ph = struct.unpack_from("<II", data, 4)
    pixels = np.frombuffer(data[12:], dtype=np.uint8)
    if pixels.size != pw * ph * 4:
        raise SystemExit(f"panel pixels {pixels.size} != {pw*ph*4}")
    panel = pixels.reshape(ph, pw, 4)
    if x < 0 or y < 0 or x + pw > width or y + ph > height:
        raise SystemExit(f"panel {pw}x{ph} at {x},{y} exceeds {width}x{height}")
    crop = raw[y:y + ph, x:x + pw]
    diff = np.abs(crop.astype(np.int16) - panel.astype(np.int16))
    changed = np.any(diff != 0, axis=2).sum()
    print(f"panel: AE={diff[:,:,:3].mean():.6f} MAE8={diff[:,:,:3].mean():.6f} "
          f"max={diff[:,:,:3].max()} changed={changed}/{pw*ph}")
    Image.fromarray(crop[:, :, :3]).save(out_path)
    print(f"{out_path} {pw}x{ph}")


if __name__ == "__main__":
    main()
