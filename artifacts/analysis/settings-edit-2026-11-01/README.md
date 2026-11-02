# Lab slider value editing on fb1 (2026-11-02)

First interactive settings write in openaurora: Left/Right on slider rows
adjusts a per-process shadow value and re-renders the digit blob live.
Explicitly NOT vendor parity: ranges 0..100 and step 1 are invented
placeholders (firmware per-setting ranges/steps are not decoded), the
slider bar graphics stay static, type-2 selection rows and Select
sub-dialogs are untouched, and nothing is written to the settings database
(ui_vendor has no set_int call; the store is process memory only).

## What was implemented

`openaurora/modules/ui_vendor/ui_vendor.c`:

- `ctx.edit_val[4]`: shadow values for slider rows 1..4, seeded from the
  static fitted digit strings at text load.
- Blob rendering factored into `render_text_blob(c, i, text)`; load and
  edit paths share it.
- `move_key` handles Left (21) / Right (22) on `SCREEN_SETTINGS_LIST`
  only when `ui.edit=1` (default 0, lab gate like `ui.launch_anim`) and the
  selected row is 1..4: clamp 0..100, re-render blob `9+sel`, repaint.
- `ui.edit_init="r=v,..."` lab preset (rows 1..4, values 0..100) for
  captures without key injection; anything else ignored.

## Host evidence

`tests/ui_settings_text.c` (config `ui.edit=1`) drives real keys through
the engine: Down,Down selects row 1 (Contrast); Right×5 changes 95→100
(blob widens 16→20 px at the same origin); Left×200 clamps at 0 (blob
narrows to 12 px); row 2 keeps "50". `make check-ui-settings-text` passes.

## fb1 evidence

Native ARM run, PID 22926, `mod_ui_vendor 9c0c4d4e...`, config
`ui.screen=settings_list ui.text=1 ui.sel=1 ui.edit=1 ui.edit_init=1=49`
with the shipped BinSVG font pack. Capture
`tv-fb1-edited-row-1.png` (sha256 `33c719a1...`):

- Differs from the live unfocused baseline only in the row-1 band
  (x20..769, y253..318; 48,584 px) — focus bar plus edited digits.
- Row-1 digit ink bounding box x662..705, y270..304: 380 dark-red
  (#7E000A) pixels on the #FE9419 bar, vs 378 white pixels (unfocused
  "95" on panel red) at the same box in the row-4 reference capture.
  Same box, different glyphs ("49" vs "95") and focused ink.

fb1 backed up/restored (16,588,800 bytes, offset 0,1080), staging removed,
no helper left, /tmp back to 2%. TV otherwise untouched: no settings-DB
write, no vendor interaction, stock Aurora PIDs unchanged.
