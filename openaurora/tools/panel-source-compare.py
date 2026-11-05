#!/usr/bin/env python3
"""Compare a recorder panel capture crop with an OAI1 panel source.

usage: panel-source-compare.py RAW WIDTH HEIGHT OAI1_PANEL OUT_PNG

The recorder writes 1920x1080 straight RGBA. The OAI1 panel is
780x1080 (or its header size) at the capture origin. Prints AE/MAE/changed
over RGB and writes the cropped capture as PNG.
"""
import struct
import sys

import numpy as np
from PIL import Image


def main():
    if len(sys.argv) != 6:
        raise SystemExit(__doc__)
    raw_path, width, height, panel_path, out_path = sys.argv[1:]
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
    if pw > width or ph > height:
        raise SystemExit("panel is larger than the capture")
    crop = raw[:ph, :pw]
    diff = np.abs(crop.astype(np.int16) - panel.astype(np.int16))
    changed = np.any(diff != 0, axis=2).sum()
    print(f"panel: AE={diff[:,:,:3].mean():.6f} MAE8={diff[:,:,:3].mean():.6f} "
          f"max={diff[:,:,:3].max()} changed={changed}/{pw*ph}")
    Image.fromarray(crop[:, :, :3]).save(out_path)
    print(f"{out_path} {pw}x{ph}")


if __name__ == "__main__":
    main()
