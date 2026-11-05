#!/usr/bin/env python3
"""Stage OAI1 blobs for an offline stock-layout apps-grid render.

With --db, use the live LauncherItem rows (parent0, type2, non-deleted,
non-empty thumbnail) and the original downloaded thumbnail names. The special
"Markt" tile is inserted by the capture test as a project row named `markt`.
Without --db, stage the fixed 25-item comparison corpus.
"""
import argparse
import sqlite3
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

THUMBS = [
    "launcher_app_thumb61", "launcher_app_thumb336",
    "launcher_app_thumb368", "launcher_app_thumb495",
    "launcher_app_thumb356", "launcher_app_thumb355",
    "launcher_app_thumb354", "launcher_app_thumb385",
    "launcher_app_thumb476",
]


def write_oai1(dst: Path, im: Image.Image) -> None:
    im = im.convert("RGBA")
    with open(dst, "wb") as f:
        f.write(b"OAI1")
        f.write(struct.pack("<II", im.width, im.height))
        f.write(im.tobytes())


def render_text(font_path: Path, text: str, size: int,
                color=(255, 255, 255, 255)) -> Image.Image:
    font = ImageFont.truetype(str(font_path), size)
    bbox = font.getbbox(text)
    im = Image.new("RGBA", (bbox[2] - bbox[0], bbox[3] - bbox[1]),
                   (255, 255, 255, 0))
    ImageDraw.Draw(im).text((-bbox[0], -bbox[1]), text, font=font, fill=color)
    return im


def label_strip(font_path: Path, text: str) -> Image.Image:
    im = Image.new("RGBA", (310, 56), (247, 148, 30, 255))
    txt = render_text(font_path, text, 28)
    im.alpha_composite(txt, ((310 - txt.width) // 2, (56 - txt.height) // 2))
    return im


def live_rows(db: Path):
    con = sqlite3.connect(db)
    rows = con.execute(
        "SELECT m_application_thumb_file_name FROM LauncherItem"
        " WHERE m_parent_id=0 AND m_is_deleted=0 AND m_is_out_of_sync=0"
        " AND m_item_type=2 ORDER BY m_order_index,m_id").fetchall()
    con.close()
    names = []
    for (blob,) in rows:
        if not blob:
            continue
        name = bytes(blob).split(b"\0", 1)[0].decode("utf-8", "replace")
        if name:
            names.append(name)
    return names


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--db")
    ap.add_argument("--thumbs", default="/tmp/opencode/live-thumbs")
    ap.add_argument("--ui-assets", default=str(root / "artifacts/ui-assets"))
    ap.add_argument("--font", default=str(root / "artifacts/workbench/rootfs/vendor/fonts/DefaultProportional.ttf"))
    args = ap.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    ui = Path(args.ui_assets)
    thumbs = Path(args.thumbs)

    write_oai1(out / "background_fullscreen.rgba",
               Image.open(ui / "background_fullscreen.png"))
    write_oai1(out / "apps_title.rgba",
               render_text(Path(args.font), "Apps", 54))
    write_oai1(out / "markt.rgba", Image.open(ui / "market_animated.png"))
    write_oai1(out / "markt_label.rgba",
               label_strip(Path(args.font), "Markt"))

    if args.db:
        names = live_rows(Path(args.db))
        for name in names:
            src = thumbs / name
            if not src.exists():
                print(f"missing live thumbnail {src}", flush=True)
                continue
            write_oai1(out / (name + ".rgba"), Image.open(src))
        print(f"staged {len(names)} live thumbnails -> {out}")
    else:
        for i, name in enumerate(THUMBS, start=1):
            src = thumbs / name
            if not src.exists():
                print(f"missing thumbnail {src}", flush=True)
                return 1
            write_oai1(out / f"live_{i:02d}.rgba", Image.open(src))
        for i in range(10, 20):
            write_oai1(out / f"live_{i:02d}.rgba",
                       Image.new("RGBA", (310, 174), (32, 32, 32, 255)))
        print(f"staged fixed corpus -> {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
