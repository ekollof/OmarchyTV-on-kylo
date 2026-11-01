#!/usr/bin/env python3
"""Convert vendor UI PNGs into the flat RGBA blobs openaurora blits.

Format (little-endian):  "OAI1" | u32 width | u32 height | width*height*4 bytes RGBA

    tools/icons-to-raw.py <outdir> <png> [<png> ...]

The icons come from artifacts/ui-assets/ (extracted from /vendor2/dynamic_ui by the firmware
theme pack), so the native UI draws the vendor's own artwork.
"""
import struct
import sys
from pathlib import Path

from PIL import Image


def convert(src: Path, outdir: Path) -> Path:
    im = Image.open(src).convert("RGBA")
    w, h = im.size
    dst = outdir / (src.stem + ".rgba")
    with open(dst, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", w, h))
        f.write(im.tobytes())
    return dst


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    outdir = Path(sys.argv[1])
    outdir.mkdir(parents=True, exist_ok=True)
    for a in sys.argv[2:]:
        p = convert(Path(a), outdir)
        print(f"  {p.name}: {p.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
