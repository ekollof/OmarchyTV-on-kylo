#!/usr/bin/env python3
"""Extract the source-selection ("Bronlijst") text as OAI1 blobs from the
vendor capture. The panel and focus bar are flat colours, so the glyph alpha is
recovered from the distance to the background, and the real ink colour is kept
(the focused row is dark navy on the bar, the other rows are white).

Each row is stored twice: src_N (unfocused, white, native size) and src_N_f
(focused, dark, 34/28 larger), bottom-aligned by the module.

    tools/extract-source-selection.py /path/p0/06-source-list-bronlijst.png OUTDIR
"""
import struct
import sys
from pathlib import Path
from PIL import Image

CAP_PANEL = (45, 30, 190)   # legacy p0/06 capture, R/B swapped
CAP_BAR = (25, 148, 254)    # legacy p0/06 focus bar, R/B swapped
WHITE = (255, 255, 255)
FOCUS_INK = (126, 0, 10)    # true #7E000A: (10,0,126) in the swapped capture
THRESHOLD = 16
FOCUS_SCALE = 34.0 / 28.0


def write_rgba(dst: Path, im: Image.Image) -> None:
    with open(dst, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", im.width, im.height))
        f.write(im.tobytes())


def near(px, c, t=THRESHOLD):
    return max(abs(px[0] - c[0]), abs(px[1] - c[1]), abs(px[2] - c[2])) <= t


def ink(px):
    return not near(px, CAP_PANEL) and not near(px, CAP_BAR)


def line_bbox(im, y0, y1, x0=40, x1=770):
    left, top, right, bottom = x1, y1, -1, -1
    px = im.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if ink(px[x, y]):
                left = min(left, x); right = max(right, x)
                top = min(top, y); bottom = max(bottom, y)
    return None if right < 0 else (left, top, right, bottom)


def rows(im, y0, y1):
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


def mask_and_ink(im, box, bg):
    """Alpha mask (L) and the strongest ink colour in the box."""
    px = im.load()
    left, top, right, bottom = box
    w, h = right - left + 1, bottom - top + 1
    out = Image.new("L", (w, h), 0)
    op = out.load()
    best = bg
    bestd = 0.0
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            p = px[x, y]
            d = sum((p[c] - bg[c]) ** 2 for c in range(3)) ** 0.5
            if d > bestd:
                bestd = d; best = p
    if bestd == 0:
        return out, bg
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            p = px[x, y]
            d = sum((p[c] - bg[c]) ** 2 for c in range(3)) ** 0.5
            op[x - left, y - top] = max(0, min(255, int(round(d / bestd * 255))))
    return out, best


def colored(mask, color):
    im = Image.new("RGBA", mask.size, color + (0,))
    im.putalpha(mask)
    return im


def scaled(mask, factor):
    w = max(1, int(round(mask.width * factor)))
    h = max(1, int(round(mask.height * factor)))
    return mask.resize((w, h), Image.LANCZOS)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    im = Image.open(sys.argv[1]).convert("RGB")
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)

    title = line_bbox(im, 35, 125)
    if title:
        mask, _ = mask_and_ink(im, title, CAP_PANEL)
        blob = colored(mask, WHITE)
        write_rgba(out / "src_title.rgba", blob)
        print(f"src_title {blob.width}x{blob.height} at ({title[0]},{title[1]})")

    bands = rows(im, 130, 850)
    print(f"{len(bands)} row bands: {bands}")
    for i, (t, b) in enumerate(bands[:9]):
        bb = line_bbox(im, t, b + 1)
        if not bb:
            continue
        bg = CAP_BAR if t < 220 else CAP_PANEL
        mask, found = mask_and_ink(im, bb, bg)
        # The capture has row 0 in the focused style; bring every row to the
        # same two sizes so the module can switch focus without guessing.
        if t < 220:
            u = scaled(mask, 1 / FOCUS_SCALE)
            f = mask
            y_unfocused = bb[1] + (mask.height - u.height)
        else:
            u = mask
            f = scaled(mask, FOCUS_SCALE)
            y_unfocused = bb[1]
        write_rgba(out / f"src_{i}.rgba", colored(u, WHITE))
        write_rgba(out / f"src_{i}_f.rgba", colored(f, FOCUS_INK))
        print(f"src_{i} {u.width}x{u.height} at ({bb[0]},{y_unfocused}); "
              f"src_{i}_f {f.width}x{f.height}; ink was {found}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
