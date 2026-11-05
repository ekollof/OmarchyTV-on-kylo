# Full portal takeover with the physical remote — 2026-11-05

Owner authorization: **"we have access to the TV again, so we can test on the
hardware."** The set was taken over temporarily, the real remote was used, both
TV-side reloads were exercised, and stock was restored by watchdog reboot.
This is not cold startup, a persistent install or a boot-hook change.

## Pre-state

- Stock `aurora.elf` PID 1782 live (2186 was the earlier zombie); appmgr 1580.
- `/conf/sqlite_db/settingsDB.db` source `m_current_source=5`; it stayed 5.
- fb1 1920x2160, pan 0,1080; `/conf/oa_boot` and `/conf/oa_fail` absent;
  `/buffer` 186.6 MB free.

## Staging

`/buffer/oa-takeover-20261105`, uploaded as one tar (SHA256
`24cbe3e5…c37a7fa`), extracted and verified by `SHA256SUMS` before execution.
It contained the ABI 3 core, `oa-client`, eight modules, the full workbench UI
asset set and a read-only copy of the live settings DB.

```
plat_stub  display_fb  ui_engine  settings  service_db
image_decode  ir  ui_vendor
```

The config used `ui.dir`/`settings.db` from the stage, `display.restore_snapshot=1`,
`display.set_region=1`, `ir.blast=1`, `ir.rc_protocol=4`, `ir.device=` and the
live DB. The display took a full snapshot before painting.

## Result

- `display_fb` opened `/dev/fb1` 1920x1080 32bpp, saved 16,588,800 bytes and
  set the region. `ui_vendor` painted the vendor Startpagina portal:
  `5 rail items, 12 cards, band 0 rail 2 card 0`.
- The **physical remote** produced 29 BLAST press events:
  Right 22, Left 21, Down 19, Up 20. The portal repainted between band 1
  cards and rails 2, 3 and 4:

  ```
  BLAST code=00003056 ... -> rc=22 Right
  BLAST code=00003055 ... -> rc=21 Left
  BLAST code=00003053 ... -> rc=19 Down
  BLAST code=00003854 ... -> rc=20 Up
  painted portal: ... band 0 rail 3 card 0
  painted portal: ... band 0 rail 4 card 0
  ```

- `reload ir` over the real socket stopped/unloaded/loaded the module,
  closed and reopened `/dev/video28` with `v4l2_close`/`v4l2_open`, and kept
  decoding keys. No hang.
- `reload ui_vendor` stopped/unloaded/loaded the module, restored its 5-byte
  state, republished `ui.portal`, repainted the portal and continued receiving
  remote keys.
- `portal_0.png` / `portal_1.png` are the initial fb1 frames;
  `portal-rail4_0.png` is the physically navigated rail-4 frame.

## Repaint cost (open defect)

The user reported that the animation is very slow. The log confirms it: 65
portal paints, median interval **617 ms**, while the rail highlight is an
11-frame/30 ms animation. The full-portal repaint therefore stretches the
highlight to roughly 6.8 s and makes every remote step feel delayed.

This is a software `display_fb`/portal cost, not the remote: BLAST events arrive
normally and `reload` completes. The present path converts a 1920x1080 RGBA
scene to BGRA, mirrors both fb frames, then `msync`s the whole 16.5 MB mapping
before `FBIOPAN_DISPLAY`. The fix is being profiled and implemented separately;
until then this run does not claim animation parity.

## Restore

`stop display_fb` needed the known second attempt (`cleanup pending` then
`ok`); `quit` unloaded the remaining modules and removed the socket. The fb1
capture after stop is blank, matching the pre-takeover frames. Staging and the
tar were removed, then `/buffer/wdtctl set 10` rebooted the set.

After boot:

```
aurora.elf PID 1769 (2206 zombie), appmgr 1569
fb1 virtual 1920,2160 pan 0,0
/conf/oa_boot and /conf/oa_fail absent
/buffer/oa-takeover-20261105 absent
source m_current_source=5 (unchanged)
```

The remote and stock UI are back under stock Aurora.

## Evidence

`artifacts/analysis/takeover-full-2026-11-05/`:

- `pre-state.txt`, `stop-stock.txt`, `start-core.txt`, `start-stack.txt`
- `run.log` (full module log), `status.txt`, `reload-ir.txt`,
  `reload-ui-vendor.txt`, `stop-stack.txt`, `stop-display.txt`, `quit.txt`
- `portal_0.png`, `portal_1.png`, `portal-nav_0.png`, `portal-nav_1.png`,
  `portal-rail4_0.png`, `restored_0.png`, `restored_1.png`
- `post-state.txt`, `reboot.txt`

## Limits

- The physical remote was exercised on the portal only; settings/apps/source
  screens were not re-run with the physical remote in this session.
- No cold startup, boot hook, native scanout, HDMI/CEC or audio acceptance.
- The source stayed 5 and was not changed by this run.
- The repaint/animation cost above is an unresolved project defect.
