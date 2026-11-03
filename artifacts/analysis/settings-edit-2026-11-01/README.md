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

## fb1 evidence (fixed build + firmware Y0)

Native ARM runs, `mod_ui_vendor 36ef5a61...` (fixed paint order), then
`eb3768ca...` (Y0 186), shipped font pack,
`ui.screen=settings_list ui.text=1 ui.sel=1 ui.edit=1`.

| capture | sha256 | edit_init | row-1 ink |
|---|---|---|---|
| `tv-fb1-edited-row-1-fixed.png` | `5c61bd16...` | 1=49 | 283 px, x660..703 (44 px, two glyphs) |
| `tv-fb1-edited-row-1-single-digit.png` | `4ab5e059...` | 1=7 | 121 px, x660..679 (20 px, one glyph) |
| `tv-fb1-edited-row-1-y186.png` | `e3fd5209...` | 1=49 | bar fill y256..321 = row frame (20,186+70,750,66) |
| `tv-fb1-edited-row-1-clean-bar.png` | `d2502833...` | 1=49 | bar edges pixel-clean x20..769 y256..321; diff vs vendor confined to row 1 (22 stray px elsewhere, pre-existing threshold noise) |
| `tv-fb1-edited-row-1-clean-edges.png` | `e129c33e...` | 1=49 | bottom edge clean break 321/322 full-width after erase-clip fix; owner-confirmed artifact gone |

The Y0=186 run proves the firmware-derived bar position on hardware:
outside the erase rects the orange fill spans exactly y256..321 (66 px) at
row 1, i.e. frame y = 186+70·1 with the decoded listBox+inset origin. Digit
widths (44 px vs 20 px at the same origin x660) prove the visible digits
are the live blob.

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

### Restore protocol (2026-11-02 incident)

Never stage a run as save → paint → kill-without-restore → save again: the
second save overwrites the stock backup with painted pixels, and a later
restore then replays our own frame (this happened; the screen showed the
run-1 frame until the owner cleared it with the remote). Rules: timestamp
every fb1 backup (`fb1-<utc>.bak`), never save over an existing backup,
verify the backup differs from any staged paint before overwriting anything,
and always restore in the same session that painted (save → run → capture →
restore → remove staging → verify).
