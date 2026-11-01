#!/usr/bin/env python3
"""Reconstruct focused settings rows from the FA state resources.

Evidence used:
  - SettingListBoxRowFA (479): the row background is a #FE9419 3-slice that
    SVGListBoxRow::renderBackground stretches horizontally over the row
    (HorizontalMaskScaleTransform), with 33 px caps and a 66 px height.
  - settingListTextboxFA (488) and settingListButtonFA (496) textPos:
    focused ink is #7E000A; the unfocused state is #FFFFFF.
  - The captured panel rows have a 70 px pitch with the text centered; the
    disabled "Beeld zoomen" row (index 6, grey in the capture) is skipped.
The row position (x=20, w=750, centered 66 px bar) and the bar centering are a
reconstruction from that geometry, not a captured focused-row image.
UI-GLYPH-DRAW-2026-11-01 also recovers fontBold on the FA label and a changed
textPos height. This recoloring preserves captured glyph shapes and therefore
does not reproduce or validate focused text type/size/layout.

    tools/render-settings-focus.py PANEL_PNG OUTDIR --reference ROW OUT.png
"""
import struct
import sys
from pathlib import Path
from PIL import Image

BG = (190, 30, 45)          # panel #BE1E2D, corrected capture
BAR = (254, 148, 25)        # SettingListBoxRowFA #FE9419
INK = (126, 0, 10)          # settingListTextboxFA/ButtonFA textPos #7E000A
ROW_X, ROW_W, ROW_H = 20, 750, 66
ROW_CENTERS = [216 + 70 * i for i in range(9)]
SKIP = {6}                  # "Beeld zoomen" is disabled (grey) in the capture


def blend(a, b, t):
    return tuple(round(a[i] * t + b[i] * (1.0 - t)) for i in range(3))


def render_row(panel, index):
    y0 = ROW_CENTERS[index] - ROW_H // 2
    out = Image.new("RGBA", (ROW_W, ROW_H), BAR + (255,))
    px = out.load()
    src = panel.load()
    bg = BG
    for y in range(ROW_H):
        for x in range(ROW_W):
            p = src[ROW_X + x, y0 + y]
            if p == bg:
                continue
            # White ink blended with the panel background lies on the line
            # bg -> white. Recolor exactly that line to the FA ink; keep
            # sliders, knobs and icons as captured.
            a = (p[1] - bg[1]) / (255.0 - bg[1])
            if a < 0.04:
                continue
            pred_r = bg[0] + (255 - bg[0]) * a
            pred_b = bg[2] + (255 - bg[2]) * a
            if abs(p[0] - pred_r) <= 12 and abs(p[2] - pred_b) <= 12:
                px[x, y] = blend(INK, BAR, a) + (255,)
            else:
                px[x, y] = p + (255,)
    return out, y0


def write_oai1(im, path):
    with open(path, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", im.width, im.height))
        f.write(im.tobytes())


def main() -> int:
    if len(sys.argv) not in (4, 6):
        print(__doc__, file=sys.stderr)
        return 2
    panel = Image.open(sys.argv[1]).convert("RGB")
    outdir = Path(sys.argv[2])
    outdir.mkdir(parents=True, exist_ok=True)
    rows = {}
    for i in range(9):
        if i in SKIP:
            continue
        row, y0 = render_row(panel, i)
        rows[i] = (row, y0)
        write_oai1(row, outdir / f"settings_focus_{i}.rgba")
        print(f"settings_focus_{i}.rgba 750x66 row y0={y0}")
    if len(sys.argv) == 6 and sys.argv[3] == "--reference":
        index = int(sys.argv[4])
        ref = Image.new("RGB", (1920, 1080), (0, 0, 0))
        ref.paste(panel, (0, 0))
        row, y0 = rows[index]
        ref.paste(row.convert("RGB"), (ROW_X, y0))
        ref.save(sys.argv[5])
        print(f"reference -> {sys.argv[5]} (row {index})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
