#!/usr/bin/env python3
"""Stage OAI1 blobs for an offline stock-layout apps-grid render.

Sources:
  * background/title/market from artifacts/ui-assets (device theme assets);
  * first nine app tiles from downloaded launcher thumbnails;
  * the remaining visible tile icons are cropped from the stock oracle because
    those third-party images are not present in the extracted assets. Those
    cells measure layout/chrome only, not independently downloaded content.
"""
import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Stock first-page order from the 2026-11-05 capture: Markt then 14 app tiles.
THUMBS = [
    "launcher_app_thumb61",   # Netflix
    "launcher_app_thumb336",  # Prime Video
    "launcher_app_thumb368",  # YouTube
    "launcher_app_thumb495",  # tabii
    "launcher_app_thumb356",  # Canal Digitaal
    "launcher_app_thumb355",  # TV Vlaanderen
    "launcher_app_thumb354",  # Télésat
    "launcher_app_thumb385",  # Digiturk Play
    "launcher_app_thumb476",  # GAIN
]
ORACLE_CELLS = [(2, 0), (2, 1), (2, 2), (2, 3), (2, 4)]


def write_oai1(dst: Path, im: Image.Image) -> None:
    im = im.convert("RGBA")
    with open(dst, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", im.width, im.height))
        f.write(im.tobytes())


def render_title(font_path: Path, text: str, size: int) -> Image.Image:
    font = ImageFont.truetype(str(font_path), size)
    bbox = font.getbbox(text)
    im = Image.new("RGBA", (bbox[2] - bbox[0], bbox[3] - bbox[1]),
                   (255, 255, 255, 0))
    ImageDraw.Draw(im).text((-bbox[0], -bbox[1]), text, font=font,
                            fill=(255, 255, 255, 255))
    return im


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--stock", default=str(root / "artifacts/protocol/stock-apps-grid.png"))
    ap.add_argument("--ui-assets", default=str(root / "artifacts/ui-assets"))
    ap.add_argument("--thumbs", default=str(root / "artifacts/workbench/thumbs"))
    ap.add_argument("--font", default=str(root / "artifacts/workbench/rootfs/vendor/fonts/DefaultProportional.ttf"))
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    ui = Path(args.ui_assets)
    thumbs = Path(args.thumbs)
    stock = Image.open(args.stock).convert("RGB")

    write_oai1(out / "background_fullscreen.rgba",
               Image.open(ui / "background_fullscreen.png"))
    write_oai1(out / "apps_title.rgba",
               render_title(Path(args.font), "Apps", 54))
    write_oai1(out / "live_00.rgba", Image.open(ui / "market_animated.png"))
    write_oai1(out / "markt_label.rgba",
               stock.crop((183, 355, 493, 411)))
    for i, name in enumerate(THUMBS, start=1):
        src = thumbs / name
        if not src.exists():
            print(f"missing thumbnail {src}", flush=True)
            return 1
        write_oai1(out / f"live_{i:02d}.rgba", Image.open(src))
    for i, (row, col) in enumerate(ORACLE_CELLS, start=10):
        x = 183 + col * 310 + 7
        y = 180 + row * 230 + 6
        write_oai1(out / f"live_{i:02d}.rgba",
                   stock.crop((x, y, x + 294, y + 169)))
    for i in range(15, 20):
        im = Image.new("RGBA", (310, 174), (32, 32, 32, 255))
        write_oai1(out / f"live_{i:02d}.rgba", im)
    print(f"staged {len(list(out.glob('*.rgba')))} blobs -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
