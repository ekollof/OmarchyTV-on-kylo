# Cold fb1 repair through the vendor SetPar path — 2026-11-07

Status: offline implementation and fake-fb regression only. No TV access,
takeover or scanout claim.

## Why this exists

`OSD-KERNEL-INIT` established that `ntkdriver_nondovi.ko` registers the five
framebuffers during `KER_OSD_DrvInit`, independently of Aurora. Registration
does not guarantee that a plane's `fb_var` is mode-configured or that a
userspace first pan will produce visible output. On a cold boot `/dev/fb1`
can exist while its var still has zero dimensions/format; the previous display
open path then mapped a zero-length region and failed.

The shipped template for fb1 is recoverable from `root:/lib/modules/install_fpga.sh`:

```text
sizes         1920x1080, 1920x1080, 1280x360, 256x256, 32x32
buffer counts 3, 2, 2, 2, 2
separate      0, 2, 0, 0, 0
```

So fb1 is 1920x1080, two frames, 32bpp (BGRA storage on this platform).

## Kernel evidence: FBIOPUT_VSCREENINFO reaches SetPar

`ntkdriver_nondovi.ko` (MD5 `fded7005651e37b8a1b5e8e89f73f4ff`) has:

```text
gstOSDfbOps at 0x75494 (92 bytes)
  0x754a8  _ker_osd_fb_CheckVar   (0x22686c)
  0x754ac  _ker_osd_fb_SetPar     (0x227328)
  0x754b4  _ker_osd_fb_SetCmap
  0x754bc  _ker_osd_fb_PanDisplay
  0x754d4  _ker_osd_fb_Ioctl
  0x754dc  _ker_osd_fb_Mmap
```

The relocations are `R_ARM_ABS32` entries in `gstOSDfbOps`; this is the fb_ops
table used by the framebuffer core. Standard `FBIOPUT_VSCREENINFO` calls
`fb_check_var` and then `fb_set_par`, so this path invokes
`_ker_osd_fb_SetPar`. `OSD-KERNEL-INIT` recovered that SetPar enables alpha
blending (255), sets the colour mode/stride (32-byte aligned), fills default
source/destination rectangles and calls `pfSetDispWin`. It does not itself
prove that the plane is visible or that the panel scans it out.

## Implementation

`mod_display` gains opt-in `display.cold_init=1` (source default 0; both shipped
bundle configs set 1):

1. open `/dev/fb1` with the existing wait/mdev path;
2. `FBIOGET_VSCREENINFO` / `FBIOGET_FSCREENINFO`;
3. if `xres`, `yres`, `bits_per_pixel==32` and `line_length` are valid, do
   nothing (takeover/vendor-initialized state is unchanged);
4. otherwise build the recovered fb1 template (`display.cold_width=1920`,
   `display.cold_height=1080`, `display.cold_frames=2`, 32bpp, BGRA bitfields,
   `FB_ACTIVATE_NOW`) and issue `FBIOPUT_VSCREENINFO`;
5. re-read var/fix and reject a driver that did not accept the template;
6. continue with the existing region write and first `FBIOPAN_DISPLAY`.

No new private ioctl, DRM allocation or GEM path is introduced. `PanDisplay`
still provides the binding/commit through `_ker_osd_fb_PanDisplay`.

## Tests

`check-display` now starts the real module against a fake fb whose var is zero:

```text
ok cold first pan: region written from zero state before PanDisplay
ok cold repair: unconfigured fb programmed before map/region/pan
```

The new case asserts `FBIOPUT_VSCREENINFO` is issued exactly once, the var
becomes 8x2x32 with two frames (test overrides the template dimensions), the
region is written from zero before any pan, and the first `FBIOPAN_DISPLAY`
commits buffer 0. With the repair branch skipped, the test fails at
`put_calls == 1`. All existing display tests still pass, including the
vendor-initialized path where no PUT is issued.

## Limits

- This is not cold-boot or scanout acceptance. It proves the userspace mode
  request and the vendor fb_ops wiring offline, not that a cold panel lights.
- The 1920x1080/2-frame template comes from the shipped install script, not a
  live cold var read.
- `FBIOPUT_VSCREENINFO` is a mode-changing ioctl; the bundle enables it because
  it only fires when the var is invalid. A future hardware run must verify it
  does not disturb an already-valid vendor OSD.
- The real proof still needs an authorized takeover/cold boot with
  `takeover-full.sh` and visual/scanout observation.

Report: COLD-DISPLAY-REPAIR-2026-11-07. Related: OSD-KERNEL-INIT,
OSD-BUFFER-CONTRACT, REPLACEMENT-SYSTEM.
