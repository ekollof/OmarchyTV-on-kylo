# Native UI assets (`ui.dir`)

The native OmarchyTV skin (`mod_ui_omarchy`) reads OAI1 assets from this
directory. The engine's `ui.dir` is `./ui` in the native bundle.

OAI1 is deliberately simple: the 12-byte header `OAI1`, little-endian
`uint32 width`, little-endian `uint32 height`, then top-down straight RGBA
(`width * height * 4` bytes). `oa_ui_icon` holds the decoded buffer.

## Wallpaper

No wallpaper artwork is committed here. The OmarchyTV reference
(https://gitlab.com/piermark/omarchytv) ships wallpapers under their own
attribution and permission terms; import one you are allowed to use:

```sh
python3 tools/ui/mk-wallpaper.py \
    /path/to/omarchytv/launcher/app/src/main/assets/wallpapers/last-horizon/01-eyes-wide.webp \
    ui/wallpaper.rgba --width 960 --height 540
```

`openaurora.conf` sets `ui.omarchy.wallpaper = wallpaper`; if the file is
missing, the skin falls back to the theme gradient. The reference's current
default is the `last-horizon` palette with Erik Johansson's "Eyes Wide"
artwork (used with the author's permission in OmarchyTV; not MIT).

## Fonts

The skin uses the TV's EF41 bitmap font pack through `ui.font_index` /
`ui.font_pack`; the reference's JetBrains Mono TTF is not converted yet.
