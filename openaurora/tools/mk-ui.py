#!/usr/bin/env python3
"""Build the OAI1 blobs the native UI blits.

Everything comes from the device itself:

  * rail glyphs  — cut out of the *device's own* portal framebuffer
                   (artifacts/protocol/shot-launcher.png): white line icons on a coloured
                   tile, so alpha is recovered as the distance from the tile colour;
  * app cards    — /buffer/downloaded_images/launcher_app_thumb<N> pulled from the set
                   (the same PNGs the stock portal draws on its tiles);
  * labels       — rendered with a font out of /vendor/fonts; DefaultProportional.ttf is
                   DirectFB's own default face and matches the portal title closest.

Usage:
    tools/mk-ui.py --out ../artifacts/workbench/uiraw [--shot PNG] [--thumbs DIR] [--font TTF]

The blobs are flat RGBA ("OAI1" + u32 w + u32 h + w*h*4), so the module needs no PNG
decoder — see modules/ui_fb/ui_fb.c for the geometry that consumes them.
"""
import argparse
import struct
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Rail glyphs as measured on the reference capture: (y0, y1) of the icon band.
RAIL_BANDS = [(239, 296), (364, 415), (486, 563), (637, 688), (748, 809)]
RAIL_X = (24, 146)  # the tile itself; the focus backdrop is wider and a shade lighter

# LauncherItem rows with m_parent_id=1 (Home page), in m_order_index order,
# mapped to the cached thumbnail file the portal downloaded for them.
CARDS = [
    ("card_netflix", "launcher_app_thumb61"),
    ("card_prime", "launcher_app_thumb336"),
    ("card_youtube", "launcher_app_thumb368"),
    ("card_tabii", "launcher_app_thumb495"),
    ("card_canaldigitaal", "launcher_app_thumb356"),
    ("card_tvvlaanderen", "launcher_app_thumb355"),
    ("card_telesat", "launcher_app_thumb354"),
    ("card_digiturk", "launcher_app_thumb385"),
    ("card_gain", "launcher_app_thumb476"),
]


def write_rgba(dst: Path, w: int, h: int, data: bytes) -> None:
    with open(dst, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", w, h))
        f.write(data)


def save_image(dst: Path, im: Image.Image) -> None:
    im = im.convert("RGBA")
    write_rgba(dst, im.width, im.height, im.tobytes())


def glyph_alpha(tile: Image.Image, base: tuple[int, int, int]) -> Image.Image:
    """Recover a white-on-transparent glyph from a white glyph on a solid tile."""
    px = tile.load()
    out = Image.new("RGBA", tile.size, (255, 255, 255, 0))
    op = out.load()
    # projection of (px - base) onto (white - base)
    vec = [255 - c for c in base]
    denom = sum(v * v for v in vec) or 1
    for y in range(tile.height):
        for x in range(tile.width):
            p = px[x, y][:3]
            t = sum((p[i] - base[i]) * vec[i] for i in range(3)) / denom
            a = int(max(0.0, min(1.0, t)) * 255)
            if a > 24:  # the focused tile sits on a slightly lighter backdrop; ignore it
                op[x, y] = (255, 255, 255, a)
    return out


def most_common_color(im: Image.Image) -> tuple[int, int, int]:
    counts: dict[tuple[int, int, int], int] = {}
    for p in im.convert("RGB").getdata():
        counts[p] = counts.get(p, 0) + 1
    return max(counts.items(), key=lambda kv: kv[1])[0]


def build_rail(out: Path, shot: Path) -> None:
    ref = Image.open(shot).convert("RGB")
    for i, (y0, y1) in enumerate(RAIL_BANDS, start=1):
        # exactly the tile band: neighbouring tiles are a different colour, so any margin
        # would be classified as glyph
        box = (RAIL_X[0], y0, RAIL_X[1], y1 + 1)
        tile = ref.crop(box)
        base = most_common_color(tile)
        glyph = glyph_alpha(tile, base)
        # crop to the glyph so the module can centre it without knowing the band
        bb = glyph.getbbox()
        if bb:
            glyph = glyph.crop(bb)
        save_image(out / f"rail{i}.rgba", glyph)
        print(f"  rail{i}.rgba  {glyph.width}x{glyph.height}  (tile base #{base[0]:02x}{base[1]:02x}{base[2]:02x})")


def build_cards(out: Path, thumbs: Path) -> None:
    for name, fname in CARDS:
        p = thumbs / fname
        if not p.exists():
            print(f"  {name}: missing {p}", file=sys.stderr)
            continue
        save_image(out / f"{name}.rgba", Image.open(p))
        print(f"  {name}.rgba  (from {fname})")


def build_labels(out: Path, font: Path, size: int, labels: list[str]) -> None:
    f = ImageFont.truetype(str(font), size)
    for spec in labels:
        name, text = spec.split(":", 1)
        b = f.getbbox(text)
        # crop to the ink exactly, so the module can position by the first ink pixel
        im = Image.new("RGBA", (b[2] - b[0], b[3] - b[1]), (255, 255, 255, 0))
        d = ImageDraw.Draw(im)
        d.text((-b[0], -b[1]), text, font=f, fill=(255, 255, 255, 255))
        save_image(out / f"{name}.rgba", im)
        print(f"  {name}.rgba  '{text}' {im.width}x{im.height}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--shot", default="artifacts/protocol/shot-launcher.png")
    workbench = Path(__file__).resolve().parents[2] / "artifacts" / "workbench"
    ap.add_argument("--thumbs", default=str(workbench / "thumbs"))
    ap.add_argument("--font", default=str(workbench / "rootfs/vendor/fonts/DefaultProportional.ttf"))
    ap.add_argument("--size", type=int, default=54)
    ap.add_argument("--label", action="append", default=[])
    args = ap.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    if args.label:
        build_labels(out, Path(args.font), args.size, args.label)
    if Path(args.shot).exists():
        build_rail(out, Path(args.shot))
    else:
        print(f"no reference capture at {args.shot}", file=sys.stderr)
    if Path(args.thumbs).is_dir():
        build_cards(out, Path(args.thumbs))
    else:
        print(f"no thumbnails at {args.thumbs}", file=sys.stderr)

    total = sum(p.stat().st_size for p in out.glob("*.rgba"))
    print(f"{len(list(out.glob('*.rgba')))} blobs, {total} bytes -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
