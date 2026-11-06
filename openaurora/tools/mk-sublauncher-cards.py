#!/usr/bin/env python3
"""Extract the three p0/03 picture-shortcut cards from settings_top.rgba.

The cards are the vendor's own pixels, cut at the measured runtime row
geometry (201/451/701, band y=36, 235x130). The resulting OAI1 blobs are
parity assets, not synthesized UI. The row model itself lives in
menus/sub_launcher.json; this tool only materializes the captured card
surfaces for the offline layout renderer.

usage: mk-sublauncher-cards.py settings_top.rgba OUTDIR
"""
import os
import struct
import sys

# Band is 1920x180 placed at screen y=240. Cards sit at band y=36.
CARDS = [(201, 36, 235, 130), (451, 36, 235, 130), (701, 36, 235, 130)]


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    data = open(sys.argv[1], "rb").read()
    if data[:4] != b"OAI1":
        sys.exit("expected an OAI1 settings_top.rgba")
    width, height = struct.unpack_from("<II", data, 4)
    pixels = data[12:]
    if len(pixels) != width * height * 4:
        sys.exit("pixel payload does not match OAI1 dimensions")
    os.makedirs(sys.argv[2], exist_ok=True)
    for index, (x, y, w, h) in enumerate(CARDS):
        if x + w > width or y + h > height:
            sys.exit("card rectangle outside the source")
        out = bytearray()
        for row in range(h):
            start = ((y + row) * width + x) * 4
            out += pixels[start:start + w * 4]
        path = os.path.join(sys.argv[2], f"sublauncher_card_{index}.rgba")
        with open(path, "wb") as f:
            f.write(b"OAI1" + struct.pack("<II", w, h) + out)
        print(f"{path} {w}x{h}")

        # Clean background for live text: replace bright/text pixels with the
        # row's median dark background. Preserve the outer 2 px card edge.
        clean = bytearray(out)
        for row in range(h):
            dark = []
            for col in range(w):
                o = (row * w + col) * 4
                r, g, b = out[o], out[o + 1], out[o + 2]
                if max(r, g, b) <= 50:
                    dark.append((r, g, b))
            if dark:
                dark.sort()
                bg = dark[len(dark) // 2]
            else:
                bg = (33, 21, 28)
            for col in range(2, w - 2):
                o = (row * w + col) * 4
                # The card interior is flat; the only non-background pixels
                # are text and its anti-aliasing. Flatten the whole interior
                # to the row median and keep the outer 2 px edge.
                clean[o] = bg[0]
                clean[o + 1] = bg[1]
                clean[o + 2] = bg[2]
                clean[o + 3] = 255
        path = os.path.join(sys.argv[2], f"sublauncher_card_{index}_bg.rgba")
        with open(path, "wb") as f:
            f.write(b"OAI1" + struct.pack("<II", w, h) + clean)
        print(f"{path} {w}x{h}")


if __name__ == "__main__":
    main()
