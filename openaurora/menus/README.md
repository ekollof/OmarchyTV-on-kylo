# Menu documents

The files here are runtime menu trees. `mod_ui_menu` loads `ui_menu.file`
(default `/buffer/openaurora/menus/sub_launcher.json`) at start and publishes
the read-only `ui.menu` service. Reloading the module replaces the document,
so new branches, entries and actions can be added without rebuilding the UI.

`sub_launcher.json` is generated from the shipped firmware by
`tools/re/menu-tree.py --emit-json`; manual edits are allowed and are preserved
unless the decoder is rerun.

Schema:

- `dialogs[]`: `id`, `class`, `name`, `svg`, `list` `[x,y,w,h]`, `item`
  (`w`, `h`, `scalerect`, `glyph`, `text`, `anim_ms`, `scale`,
  `glyph_delta`), optional `layout`, and `branches[]`.
- `layout`: checked runtime geometry, for example the p0/03 picture-shortcut
  row `card [201,276,235,130]`, `pitch 250`, item cell `[199,274,240,140]`.
  This is measured from the vendor capture, not derived from resource-space
  coordinates; `source` records the oracle.
- `branches[]`: `launcher_type`, `name`, `items[]`, optional `excluded`.
- `items[]`: `id`, `key`, optional `action`, optional `dynamic` source note,
  and `values`. A dynamic value is a string marker; concrete values are
  `{ "key": "...", "action": "..." }` entries.
- `action` is an opaque string for the consumer, for example
  `setting:ENERGY_SAVING`, `set:ENERGY_SAVING=OFF`, or `open:settings`.
  The JSON layer does not assign meaning; the UI/settings dispatcher does.
  The current vendor skin implements `open:portal|sources|settings|
  settings_list|sub_launcher|apps|channels` through
  `oa_portal_iface.menu_action` v2; `setting:`/`set:` are still `ENOTSUP`
  there and are the next dispatcher.

`tools/qemu-sub-launcher-check.sh` builds the ARM stack, runs the real
modules under `qemu-arm-static` with the offline framebuffer provider, and
compares the static sub-launcher row to `settings_top.rgba` (AE=0) plus the
300 ms card bounds. Use it when the TV is off limits.
