#!/usr/bin/env python3
"""Extract the p0/03 Beeld settings bands as OAI1 blobs.

The capture predates the BGRA/R-B correction, so channels are swapped on the
way out; the live UI is RGBA and the display converts to BGRA. Three bands cover
all content: rail 0,180,176,660 (through the fifth tile's bottom edge at 838),
top tiles 0,240,1920,180 and the category row plus the selected tab's caption
0,440,1920,260 (the "Beeld" label at 261,671). The remaining screen is the
ordinary background, which the capture stores as (14,14,13) and therefore is
(13,14,14) after the same correction.

    tools/extract-settings-bands.py CAPTURE OUTDIR
"""
import struct
import sys
from pathlib import Path
from PIL import Image

BANDS = {
    "settings_rail": (0, 180, 176, 660),
    "settings_top": (0, 240, 1920, 180),
    "settings_cat": (0, 440, 1920, 260),
}


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    im = Image.open(sys.argv[1]).convert("RGB")
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for name, (x, y, w, h) in BANDS.items():
        crop = im.crop((x, y, x + w, y + h))
        r, g, b = crop.split()
        true = Image.merge("RGBA", (b, g, r, Image.new("L", crop.size, 255)))
        with open(out / f"{name}.rgba", "wb") as f:
            f.write(b"OAI1")
            f.write(struct.pack("<II", w, h))
            f.write(true.tobytes())
        print(f"{name}.rgba {w}x{h} from {x},{y}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
