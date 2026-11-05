# Stock Apps-grid focus pulse capture — 2026-11-06

Read-only fb1 region capture of a real stock Apps grid focus change, using the
DIAL virtual remote for the key. The purpose was to decide whether the
recovered 300 ms `#scalerect`/`#glyph` animation is used by the full-screen
Apps grid.

## Setup

- `fb-region-sampler` opened `/dev/fb1` `O_RDONLY`/`PROT_READ`; no pan/region/
  MAU/input writes.
- Region `150,150 700x280` covering the Markt and Netflix cells.
- Wait 120 s, requested interval 8 ms, capture 600 ms. Actual sampling interval
  was ~37 ms because the tool copies and hashes the 700x280 patch each sample.
- The stock screen was Netflix focused; one DIAL Left moved focus to Markt.

## Result

26 patches were saved. Frames 0..9 were stable at `yoffset=0`; one change
appeared at `t=53ms` with `yoffset=1080`:

```
changed=45,745  bbox=33,30 620x230  mean=19.523
```

Frames 10..25 (`t=53..621ms`) all had the **same hash**
`4e3b84ed5a114c62`; no subsequent scale/glyph pulse. The single change is the
Markt/Netflix focus swap (border/backdrop + label moves).

```
pre  t=-366..-36ms  hash=43b053b860530a6a
post t=53..621ms    hash=4e3b84ed5a114c62
```

## Conclusion

The stock full-screen Apps grid focus change is an **instant single redraw**,
not a 300 ms scaled-tile pulse. The `launcheritemFA/FI` 300 ms records and the
`#scalerect`/`#glyph` geometry must **not** be wired into this screen. The
`SubLauncherDialog`/`sublauncheritemFA` resources belong to a different
settings sub-launcher; that screen still needs a capture.

The first pre/post PNGs saved from the sampler were raw BGRA decoded as RGBA
and appeared blue-shifted. The stored PNGs were corrected by swapping R/B; the
hash/bbox comparison is channel-order independent and the negative result is
unchanged.

Evidence: `artifacts/analysis/apps-pulse-2026-11-06/` (`manifest.txt`,
`analysis.txt`, `analysis.json`, corrected `pre.png`/`post.png`).
`openaurora/tools/fb-region-sampler.c` and
`openaurora/tools/fb-pulse-compare.py` are the capture/analysis tools.

## Limits

- Apps grid only; no sub-launcher/settings submenu capture.
- ~37 ms effective sampling means a very short pulse could be missed, but a
  vendor 300 ms pulse over 16 consecutive identical frames would not.
- The DIAL key produced the stock transition; no physical-remote timing claim.
- TV state was restored; the 20 MB sampler output was removed, sampler left
  staged under `/buffer/oa-fb-sampler` for the next screen.
