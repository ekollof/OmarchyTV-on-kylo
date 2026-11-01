#!/usr/bin/env python3
"""Run the screenshot CLI with synthetic framebuffer reads; never access the TV."""
import contextlib
import importlib.util
import io
import pathlib
import sys
import tempfile
from unittest.mock import patch

from PIL import Image

tool = pathlib.Path(__file__).resolve().parents[1] / "tvshot.py"
spec = importlib.util.spec_from_file_location("tvshot", tool)
tvshot = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tvshot)


def capture(bpp, raw, expected, pixel_format=None):
    values = {"name": "SYNTHETIC", "virtual_size": f"{len(expected)},1",
              "bits_per_pixel": str(bpp)}

    def fake_read(command, binary=False):
        assert binary and command.startswith("dd if=/dev/fb1 ")
        return raw

    with tempfile.TemporaryDirectory(prefix="tvshot-decode-") as directory:
        path = pathlib.Path(directory) / "shot.png"
        args = [str(tool), "--no-wake", "--out", str(path)]
        if pixel_format:
            args += ["--format", pixel_format]
        with patch.object(sys, "argv", args), \
                patch.object(tvshot, "sysfs", side_effect=lambda layer, key: values[key]), \
                patch.object(tvshot, "ssh", side_effect=fake_read), \
                patch.object(tvshot.subprocess, "run", side_effect=AssertionError("network/wake forbidden")), \
                contextlib.redirect_stdout(io.StringIO()):
            tvshot.main()
        with Image.open(path.with_name("shot_0.png")) as image:
            assert image.mode == "RGB"
            assert [image.getpixel((x, 0)) for x in range(image.width)] == expected


colours = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (17, 34, 51)]
bgra = bytes(component for r, g, b in colours for component in (b, g, r, 128))
rgba = bytes(component for r, g, b in colours for component in (r, g, b, 128))
capture(32, bgra, colours)
capture(32, bgra, colours, "BGRA")
capture(32, rgba, colours, "RGBA")
capture(16, bytes.fromhex("00f8 e007 1f00 ffff"), colours[:3] + [(255, 255, 255)])
print("ok: default/explicit BGRA, explicit RGBA, and RGB565 produce correct RGB PNGs offline")
