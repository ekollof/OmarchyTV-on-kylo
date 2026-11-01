#!/usr/bin/env python3
"""Extract the source-selection ("Bronlijst") text as OAI1 blobs from the
vendor capture. The panel and focus bar are flat colours, so white glyph alpha
is recovered as (pixel - background) / (255 - background). The blobs are the
vendor's own ink; the module draws them over its own flat panel/focus bar.

    tools/extract-source-selection.py /path/p0/06-source-list-bronlijst.png OUTDIR
"""
import struct
import sys
from pathlib import Path
from PIL import Image

PANEL = (45, 30, 190)
BAR = (25, 148, 254)
THRESHOLD = 16


def write_rgba(dst: Path, im: Image.Image) -> None:
    with open(dst, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", im.width, im.height))
        f.write(im.tobytes())


def near(px, c, t=THRESHOLD):
    return max(abs(px[0] - c[0]), abs(px[1] - c[1]), abs(px[2] - c[2])) <= t


def ink(px):
    """Text ink is anything that is neither the panel nor the focus bar."""
    return not near(px, PANEL) and not near(px, BAR)


def line_bbox(im, y0, y1, x0=40, x1=770):
    left, top, right, bottom = x1, y1, -1, -1
    px = im.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if ink(px[x, y]):
                left = min(left, x); right = max(right, x)
                top = min(top, y); bottom = max(bottom, y)
    return None if right < 0 else (left, top, right, bottom)


def extract(im, box, bg):
    px = im.load()
    left, top, right, bottom = box
    out = Image.new("RGBA", (right - left + 1, bottom - top + 1), (255, 255, 255, 0))
    op = out.load()
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            p = px[x, y]
            a = 0.0
            for c in range(3):
                den = 255 - bg[c]
                if den > 0:
                    a = max(a, (p[c] - bg[c]) / den)
            a = max(0.0, min(1.0, a))
            op[x - left, y - top] = (255, 255, 255, int(round(a * 255)))
    return out


def rows(im, y0, y1):
    """Group the text-ink rows into (top, bottom) bands."""
    px = im.load()
    bands = []
    on = False
    start = 0
    for y in range(y0, y1):
        hit = any(ink(px[x, y]) for x in range(40, 770, 2))
        if hit and not on:
            start = y; on = True
        elif not hit and on:
            bands.append((start, y - 1)); on = False
    if on:
        bands.append((start, y1 - 1))
    return bands


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    im = Image.open(sys.argv[1]).convert("RGB")
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    title_band = line_bbox(im, 35, 125)
    if title_band:
        blob = extract(im, title_band, PANEL)
        write_rgba(out / "src_title.rgba", blob)
        print(f"src_title {blob.width}x{blob.height} at ({title_band[0]},{title_band[1]})")

    bands = rows(im, 130, 850)
    print(f"{len(bands)} row bands: {bands}")
    for i, (t, b) in enumerate(bands[:9]):
        bb = line_bbox(im, t, b + 1)
        if not bb:
            continue
        bg = BAR if t < 220 else PANEL
        blob = extract(im, bb, bg)
        write_rgba(out / f"src_{i}.rgba", blob)
        print(f"src_{i} {blob.width}x{blob.height} at ({bb[0]},{bb[1]})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
