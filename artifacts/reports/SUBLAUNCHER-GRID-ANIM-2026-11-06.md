# SVGGrid layout and SVGButton animation geometry — 2026-11-06

Offline instruction-level decode of the remaining 300 ms launcher/sub-launcher
geometry. No TV access and no screen integration.

## SVGGrid layout

`SVGGrid::renderBackground` (0x010c9ed0) computes the counts at
`+0x1dc/+0x1e0`; `SVGGrid::getButtonPosition` (0x010ca8ac) places buttons.

Fields recovered from the instructions:

| field | meaning |
|---|---|
| +0x1dc | row count (computed) |
| +0x1e0 | column count (computed) |
| +0x1e4 | row pitch (Y) |
| +0x1e8 | column pitch (X) |
| +0x1ec | button height |
| +0x1f4 | Y offset |
| +0x1f8 | button height − row pitch |
| +0x1fc | X offset |
| +0x200 | button width − column pitch |

The count formulas are:

```
rows = (area_height - y_offset - (button_height - row_pitch)) / row_pitch
cols = (area_width  - x_offset - (button_width  - col_pitch )) / col_pitch
```

and the placement is:

```
x = column * col_pitch + x_offset
y = row    * row_pitch + y_offset
right  = x + col_pitch + (button_width - col_pitch)
bottom = y + button_height
```

The `+0x1f8/+0x200` fields are therefore the “button minus pitch” terms that
make the division produce the inclusive count (`(span - first - size)/pitch + 1`)
without a separate add. This matches the instructions at `0x10ca194..0x10ca1dc`.

## SVGButton 300 ms animation

`SVGButton::onAnimationProgress` (0x010b0cdc) forwards the **elapsed
milliseconds** argument (not normalized progress) to
`SVGButton::animRectDecode` (0x010b0854). The decoder computes:

```
p = elapsed_ms / (duration_seconds * 1000), clamped above to 1
scale_x = p * to_x + (1-p) * from_x
scale_y = p * to_y + (1-p) * from_y
```

`AnimationHelper::applyScalingAnimation` (0x00eeff50) does the same linear blend;
`applyMotionAnimation` (0x00eefd20) applies the linear from/to offset to the base
point and truncates with `vcvt.s32.f32`.

The scaled rectangle is anchored at the **button window top-left**, not at the
named rectangle's stored x/y:

```
scaled rect = (window_x, window_y,
               window_x + trunc(scalerect_w * scale_x),
               window_y + trunc(scalerect_h * scale_y))
```

`animRectDecode` takes the width/height from the named `#scalerect` rectangle,
adds the window origin, and resizes the button surface. `SVGImage::
setAnimationScalingRatio` (0x010c924c) stores the two ratios at `+0xb4/+0xb8`.
For `sublauncheritemFA` the record is `#scalerect 240x140`, scale 1→1.35,
`#glyph` x 0→−42 and y 0→−24 over 300 ms; `launcheritemFA/FI` are 229x132,
1→1.32 and glyph x/y −34/−37.

## Implementation and test

Added pure `common/launcher_anim.c` / `include/openaurora/launcher_anim.h`:

- `oa_launcher_grid_counts`
- `oa_launcher_scale_rect`
- `oa_launcher_motion_point`

`tests/launcher_anim.c` checks the count formulas, the sublauncher 240x140
scale at 0/0.5/1 (340x340, 382x364, 424x389 from origin 100,200), and glyph
motion at 0/0.5/1 (50,60 → 29,48 → 8,36):

```
ok launcher animation: grid counts, top-left scalerect and linear glyph motion
```

## Limits

- The rules are implemented as pure geometry, not yet wired into a screen.
  The category rail must not use these records; the portal card row is still an
  invented layout. Integration needs the vendor sub-launcher content and a
  real transition capture.
- The `#glyph` base point and the exact button window for each screen come from
  the caller; this report recovers the transform math, not the screen content.
- A real vendor 300 ms transition was not captured. The next acceptance gate is
  a high-cadence fb1/HDMI capture of a focus change (or an owner-provided video)
  at the sub-launcher/apps grid, then compare the pulse frames.
