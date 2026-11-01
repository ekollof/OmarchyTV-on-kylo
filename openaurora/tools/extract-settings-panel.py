#!/usr/bin/env python3
"""Extract the vertical picture-settings panel as an OAI1 blob.

The vendor capture predates the BGRA/R-B correction, so channels are swapped on
the way out; the live UI is RGBA and the display converts to BGRA. The panel is
the left 780x1080 of shot-settings.png; the rest of that capture is black, which
the screen clears. Rows follow SettingItemsListBoxRowItems (460) at a 65..66 px
pitch; the tabs are Beeld/Geluid.

    tools/extract-settings-panel.py CAPTURE OUT.rgba
"""
import struct
import sys
from pathlib import Path
from PIL import Image

PANEL = (0, 0, 780, 1080)


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    im = Image.open(sys.argv[1]).convert("RGB")
    x, y, w, h = PANEL
    crop = im.crop((x, y, x + w, y + h))
    r, g, b = crop.split()
    true = Image.merge("RGBA", (b, g, r, Image.new("L", crop.size, 255)))
    out = Path(sys.argv[2])
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", w, h))
        f.write(true.tobytes())
    print(f"{out}: {w}x{h} from {x},{y}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
