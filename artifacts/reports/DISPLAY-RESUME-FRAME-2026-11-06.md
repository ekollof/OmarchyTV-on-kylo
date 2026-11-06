# Display resume first-frame commit — 2026-11-06

Firmware-derived resume ordering for the fbdev display backend, plus a real
on-TV device-level check under the stock owner. This is not kernel-suspend
validation, panel scanout acceptance or cold startup.

## Firmware evidence

Vendor `idtvResume` @ `0x00752b88` explicitly re-establishes the display after
wake:

```text
752ca6: bl 8af8fc <PLAT_OSD_Plane_Enable>
752caa: bl 79a3bc <ice_OSD_SetSTRBootlogoRestore>
752cb0: bl 857780 <DEV_PANEL_PanelBackLightPowerSet>
752cfc: bl 8af8fc <PLAT_OSD_Plane_Enable>
752d00: bl 79a3bc <ice_OSD_SetSTRBootlogoRestore>
752d06: bl 857780 <DEV_PANEL_PanelBackLightPowerSet>
```

So userspace does not assume the panel simply reappears: it re-enables the OSD
plane and restores a boot/STR frame. Kernel PM separately replays cached OSD
registers and shows the display groups
(OSD-KERNEL-INIT, `_ker_osd_Resume` @ .text+0x243080). `_ker_osd_fb_PanDisplay`
also performs first-pan recovery: it can show a hidden plane and enable a
hidden region, which is the fbdev path our `present()` already uses.

## Implementation

`modules/display/display.c` now keeps the last offscreen scene across suspend
and recommits it after a resume reopen:

1. `m_suspend` restores the pre-ownership snapshot, closes the layer and keeps
   the RGBA scene (`close_layer(c, 1)`). The vendor re-enables/restores the
   display; this retained scene is our fbdev userspace equivalent.
2. `open_layer` reuses the scene when the geometry still matches; a different
   geometry reallocates it. A stop/reload still frees it.
3. `m_resume` opens/reuses the layer, saves the resume snapshot **before**
   changing the window (mirroring start, so stop restores the driver's state),
   re-applies `set_region`, marks the whole scene dirty and calls `present()`.
   The ioctl commit goes through `FBIOPAN_DISPLAY`, i.e. the recovered
   first-pan buffer bind/show path, and logs
   `resume: first frame committed (yoffset N)`.
4. If that frame fails, it restores the snapshot and closes while keeping the
   scene, so a later resume retries. If the restore also fails, fd/snapshot are
   retained and the log says the restore is pending.
5. A suspended instance with no fd/map/snapshot but a retained scene frees that
   heap synchronously in quiesce, keeping reload-while-suspended immediate and
   avoiding a worker just to free memory.

The resume snapshot ordering fixed a real regression found by the existing
fake-fb socket test: saving after `set_region` had captured our window and stop
then restored our region instead of the stock one.

## Driver-state verification

A successful `FBIOPAN_DISPLAY` return is not proof the driver accepted the
frame. New opt-in `display.verify_scanout=1` re-reads the live state after a
first frame (start self-test and resume) and requires:

- `FBIOGET_VSCREENINFO` yoffset == the frame just committed;
- `OSD_REG_RD` region words 2/3 == our window size and 4/5 == 0,0 when
  `display.set_region` is enabled;
- `OA_FB_MAU_RD` still equals the pre-ownership snapshot MAU.

A mismatch logs `scanout mismatch` with both values and rejects that first
frame; the resume path restores the snapshot, closes and keeps the scene for a
retry. Success logs
`scanout verified at start|resume: yoffset N region WxH at X,Y mau M`.
This is a driver-state check, not panel scanout proof.

## Verification

New `check-display-resume-frame` uses a fake driver that **clears its mapping on
every open**, proving the resume frame comes from the retained scene rather
than stale storage. It asserts region re-application, a new pan commit, yoffset,
pixel content, failure restore/close, and retry. A one-shot wrong yoffset after
a successful pan also proves `display.verify_scanout` rejects the frame,
restores/closes and retries cleanly:

```text
ok display resume frame: reopen clears the map, retained scene recommits, region reapplied, failure retries
```

Existing display suites pass, including the full snapshot restoration and the
reload-while-suspended deferred-activation path:

```text
ok display DSO: firmware suspended-pan error propagates; suspend/reload defers hardware until resume
ok actual fake-fbdev display_fb IPC: async gate, start/suspend/resume/reload/stop/reuse, full snapshot restoration
```

Focused ASan+UBSan passes the new test (`detect_leaks=1`, `halt_on_error=1`).
Full suite ran visibly and ended `all tests passed`; QEMU sub-launcher
value/live/launcher checks pass.

## On-TV device-level check — 2026-11-06

Staged under `/buffer/oa-display-resume`: hash-verified ARM `openaurora`
`43c7c7be…`, `oa-client` `f3d20a61…`, `mod_display_fb.so` `8d3ef8cb…`; config
`display.fb=/dev/fb1`, `display.restore_snapshot=1`, `display.test=1`,
`display.mirror=1`, `display.set_region=1`, `display.pan=0`. Stock Aurora was
not stopped.

```text
start: full snapshot saved (16588800 bytes, mau 0); region -> 1920x1080 (ok)
       present scene=5ms mirror=25ms sync=0ms pan=8ms; self-test band painted
stop/start: immediate readback row10 frame1 = 255 0 255 255 (magenta)
suspend: ok (snapshot restored, layer closed)
resume:  region -> 1920x1080 (ok)
         present scene=66ms mirror=233ms sync=1ms pan=0ms rect=0,0 1920x1080
         resume: first frame committed (yoffset 1080)
```

A 5 ms read-only sampler across resume caught the magenta band in the OSD
backing on 4 consecutive samples (`31..34` of 250), in the frame the driver
panned to (yoffset 1080); all other samples were zero because the stock owner
repaints the layer. This is device-level evidence that the retained scene and
the first-pan commit reached OSD storage. It is not proof that the panel showed
that frame: stock Aurora still owns and flips the layer.

Stock before/after captures differ by ~459k channel values, but two captures of
the untouched stock screen 2 s apart differ by ~470k — the stock screen is
dynamic, so visual equality cannot prove snapshot restoration here. Staging was
removed; no helper remained; stock PIDs stayed 1711/1781 (zombie 2204).

### Start/resume driver-state verification — 2026-11-06

A second staged run (`/buffer/oa-display-verify`, ARM `mod_display_fb.so`
`fc4780db…`, `display.verify_scanout=1`) checked the live driver state:

```text
start:  self-test band painted
        /dev/fb1: scanout verified at start: yoffset 0 region 1920x1080 at 0,0 mau 0
suspend -> resume:
        resume region -> 1920x1080 (ok)
        present rect 0,0 1920x1080 pan=32ms
        /dev/fb1: scanout verified at resume: yoffset 0 region 1920x1080 at 0,0 mau 0
        resume: first frame committed (yoffset 0)
```

So the driver reported the committed pan, our window size/origin and an
unchanged MAU on the real device for both first frames. This is still a
driver-state check under the stock owner, not an independent panel-scanout
observation. Staging was removed, no helper remained, stock PIDs stayed
1711/1781.

## Limits

- The TV was not suspended: `suspend`/`resume` here are the host's hook-only
  lifecycle commands, not kernel PM or MCU standby. `idtvResume` evidence covers
  ordering, not our kernel-PM interaction.
- Stock Aurora remained the display owner and repainted over the band; no stable
  visible-frame capture, panel scanout cadence or independent scanout is proven.
- No cold boot, MCU/watchdog authority or clean handback was exercised.
- Real audio/CEC/tuner/PVR/VIN hardware and broad UI parity remain open.
