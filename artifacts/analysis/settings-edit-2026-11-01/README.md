# Lab slider value editing on fb1 (2026-11-02)

Shadow-value editing for the four slider rows: Left/Right changes a
per-process value and re-renders the digit blob. Explicitly NOT vendor
parity:
- range 0..100 is an INVENTED placeholder (per-setting bounds come from
  runtime provider virtuals; no static table exists);
- step 1 matches the slidebar ctor default (`setSliderStep` has no caller),
  so it is firmware-consistent, not invented;
- slider bar graphics stay static; type-2 rows and Select sub-dialogs are
  untouched; nothing is written to the settings database.

## Implementation (`openaurora/modules/ui_vendor/ui_vendor.c`)

- `ctx.edit_val[4]`: shadow values for rows 1..4, seeded from the static
  digit strings once text loads.
- `render_text_blob(c, i, text)` renders TWO blobs per entry: normal ink and
  focused ink (#7E000A), because focused rows sit on the #FE9419 bar.
- `move_key`: Left (21)/Right (22) on `SCREEN_SETTINGS_LIST` when `ui.edit=1`
  (default 0) and the selected row is 1..4; clamps 0..100, re-renders blob
  `9+sel`, repaints.
- `ui.edit_init="r=v,..."`: strict bounded parse (digits only, row 1..4,
  value 0..100); malformed or trailing-junk tokens are ignored.
- **Paint order fix (adversarial review 2026-11-02):** the focus overlay
  (`settings_focus_N`) is a fully opaque 750x66 band that carries the
  captured row text. It is now composited BEFORE live text and the erase
  rectangles use the bar colour for the focused row. Previously text was
  drawn first and every live glyph in the selected row was hidden.

## Host evidence

`tests/ui_settings_text.c` (config `ui.edit=1`) drives real keys: Down,Down
selects row 1; Right×5 takes 95→100 (blob 16→20 px); Left×200 clamps at 0
(12 px); row 2 keeps "50". Ordering assertions require the overlay blit to
precede the row's text blits and require bar-coloured erase fills.
Reverting the paint order (temporarily, verified) makes the test fail at the
ordering assertion, so the regression is real.

## fb1 evidence (fixed build)

Native ARM runs, `mod_ui_vendor 36ef5a61...`, shipped font pack,
`ui.screen=settings_list ui.text=1 ui.sel=1 ui.edit=1`.

| capture | sha256 | edit_init | row-1 ink |
|---|---|---|---|
| `tv-fb1-edited-row-1-fixed.png` | `5c61bd16...` | 1=49 | 283 px, x660..703 (44 px, two glyphs) |
| `tv-fb1-edited-row-1-single-digit.png` | `4ab5e059...` | 1=7 | 121 px, x660..679 (20 px, one glyph) |

Both start at the same origin x660; the two-glyph value is 44 px wide and
the one-glyph value 20 px, proving the visible digits come from the live
blob and not from the overlay's captured glyphs.

### Superseded capture

`tv-fb1-edited-row-1.png` (sha256 `33c719a1...`, edit_init 1=49) is the
BEFORE-fix run. Its 380 dark-red pixels at x662..705 were the overlay's own
captured glyphs, not the edited value: the blob was painted underneath an
opaque overlay. The earlier claim that this capture showed "49" was wrong
and is withdrawn. Mutually, the mask differs from the fixed capture at
IoU 0.105, so the two runs are not the same image.

fb1 backed up/restored (16,588,800 bytes, offset 0,1080) for every run,
staging removed, no helper left, /tmp back to 2%. No settings-DB write, no
vendor interaction, stock Aurora PIDs unchanged.
