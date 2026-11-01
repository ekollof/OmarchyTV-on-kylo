# How aurora initialises the display (OSD) — reversing the tdga/fbdev path (2026-10-30)

**Firmware correction, 2026-10-31:** see
[OSD-BUFFER-CONTRACT-2026-10-31.md](OSD-BUFFER-CONTRACT-2026-10-31.md).
The kernel proves 131/132 are palette operations, not frame-buffer acquisition.
CREATE_DUMB allocates a fixed 4 KiB; Aurora's OSD uses private GEM allocation
followed by DRM ADDFB2/SETPLANE. The historical conclusion below excluding DRM
is withdrawn. FBIOPAN_DISPLAY is the fbdev hardware commit even without rotation.

Why: a native splash drawn from the boot hook never appeared (see `BOOTSPLASH-2026-10-30.md`). The
suspicion was that the OSD plane is not presented until the display owner configures it. This is what
the display owner actually does.

## Where the code is

`aurora.elf` itself only calls into DirectFB. The hardware setup happens in the vendor modules:

| module | role |
|---|---|
| `/vendor/lib/dfb/directfb-1.7-1/systems/libdirectfb_fbdev.so` (8.4 KB) | `system=fbdev`: opens `/dev/fbX`, `FBIOGET_VSCREENINFO/FSCREENINFO`, `mmap` |
| `/vendor/lib/dfb/directfb-1.7-1/gfxdrivers/libdirectfb_tdga.so` (55 KB) | **`NVTGA_OSD_*` — the OSD plane engine**, uses the vendor ioctls |
| `/vendor/lib/dfb/libdirectfb-1.7.so.1` | DirectFB core (layers, surfaces, sawman) |

Everything below is from the tdga driver (symbols are present, so `objdump`/Ghidra read cleanly).
`NVTGA_OSD_AddRegion` and `NVTGA_OSD_RemoveRegion` are 4-byte stubs; the real work is in
`NVTGA_OSD_InitLayer`, `NVTGA_OSD_SetRegion`, `NVTGA_OSD_UpdateRegion`, `NVTGA_OSD_FlipRegion`,
`nvtga_osd_set_pixelformat`.

## The vendor ioctl set (`'f'`-style encoding, type 0xc8)

| ioctl | direction / size | where | guessed role |
|---|---|---|---|
| `0x4600` | `FBIOGET_VSCREENINFO` | everywhere | read the fb's current var |
| `0x4602` | `FBIOGET_FSCREENINFO` | `nvtga_fb_init` | smem_start / line_length |
| `0x4606` | `FBIOPAN_DISPLAY` | **UpdateRegion / FlipRegion** | **flip to buffer N** (`var.yoffset`) |
| `0x400cc883` | `_IOW(0xc8, 131, 12)` | InitLayer, SetRegion, Update, Flip, pixelformat | **set palette entries** `{start,count,32-bit pointer}` (kernel-confirmed) |
| `0x800cc884` | `_IOR(0xc8, 132, 12)` | **UpdateRegion / FlipRegion** | **get palette entries**, same layout (kernel-confirmed) |
| `0x4008c80a` | `_IOW(0xc8, 10, 8)` | InitLayer, SetRegion, pixelformat | write an 8-byte config pair |
| `0x4020c86b` | `_IOW(0xc8, 107, 32)` | SetRegion | 32-byte **region/rectangle** |
| `0x8020c86c` | `_IOR(0xc8, 108, 32)` | SetRegion | read back the region |
| `0x4004c865` | `_IOW(0xc8, 101, 4)` | pixelformat (ARGB path) | one-word setting |
| `0x4004c871` | `_IOW(0xc8, 113, 4)` | SetRegion, pixelformat | one-word setting |

(These are what the naive "grep the constant before `ioctl`" misses: the values are built with
`movw`+`movt`, so the low half alone looks like `0x4004`.)

## InitLayer — what a layer *is*

```
memset(var, 0, 0xa0); ioctl(fd, 0x4600, &var)        // FBIOGET_VSCREENINFO
layer->type   = 0x5f                                 // 95, the vendor layer id
layer->xres   = var.xres;  layer->yres = var.yres
layer->format = nvtga_osd_convert_var_to_pixelformat(&var)
if (format == 0x4011040b) {                          // DSPF_ARGB
    pal = calloc(var.yres?, 4)
    ioctl(fd, 0x400cc883, {start, count, pal})       // palette, not a frame descriptor
    ioctl(fd, 0x4008c80a, {u32, u32})                // 8-byte config
}
layer->buffers = 1 | 2 | 8 depending on line_length vs yres  (single/double/triple)
layer->caps    = 0x21; layer->… = 5
```

So the "multi-frame" behaviour we observed on `/dev/fb1` (1920×2160 = 2 frames) is exactly this
`buffers` value; the driver gets told how many buffers the mapping has.

## SetRegion — geometry

```
ioctl(fd, 0x4600, &var)                              // current var
if (driver_format != layer_format)
    nvtga_osd_set_pixelformat(fd, layer_format, pitch)   // -> 0x4004c865 / 0x4004c871 / 0x4008c80a / 0x400cc883
ioctl(fd, 0x8020c86c, region32)                      // read region
region32 = {x, y, w, h, …} for the new surface
ioctl(fd, 0x4020c86b, region32)                      // write region
```

## UpdateRegion / FlipRegion — how a frame reaches the screen

Corrected after kernel decompilation on 2026-10-31:

```
ioctl(fd, 0x4600, &var)              // current var
// when palette handling applies, read palette entries with 132 and set changed entries with 131
ioctl(fd, 0x4606, &var)              // FBIOPAN_DISPLAY: commit, including without rotation
```

`FlipRegion` also commits this way. The kernel pan handler binds the physical
buffer and calls its update callback. The old assertion that 132/131 form a
mandatory frame-buffer handshake was incorrect. It does not explain invisible
raw writes; raw writes to an already enabled vendor mapping were later visible.

## What is still unknown

* the full **8-byte update-control** layout (ioctl 10);
* what `0x5f` (layer type) and `caps = 0x21` mean (DFB enum values vs vendor ids);
* complete cold OSD init, geometry/enable state and resume restoration.

## Next step (offline)

The former proposed 132/131 buffer experiment is withdrawn: those calls access
palette memory, and the layout is already known from the kernel. Continue from
the remaining evidence gate in `OSD-BUFFER-CONTRACT-2026-10-31.md`. The TV is in
use; do not deploy or probe it.

---

# Addendum (same day): the OSD driver is a normal DRM driver

## Where the fb device comes from

`/proc/mtd`-level work showed the fb nodes; the *driver* is the module **`ntkndpdrm.ko`**
(`/lib/modules/`, 927 KB, symbols intact):

```
ndp_drm_driver{0,1}      ndp_drm_plane_create     osdp_plane_ops / osdp_sys_ops
ndp_drm_crtc_*           ndp_osd_plane_page_flip  ndp_osd_plane_mode_set
_ndp_drm_gem_cma_dumb_create_4k   ndp_drm_gem_dumb_create / dumb_map_offset
```

So: a **standard DRM/KMS driver** (`drm_gem_cma` dumb buffers, CRTCs, planes) that also exposes the
fbdev compatibility nodes DirectFB uses (`fb0..4`, i.e. OSD_NORMAL1/2/3 + cursors). The vendor ioctls we
recovered from `libdirectfb_tdga.so` (`0xc8` group) are its **fbdev-emulation path**.

## The one symbol we could not place

`ntkndpdrm.ko` (and `ntkndp_pq.ko`) reference **`_ker_osd_drm_fb_Ioctl`** as an *undefined* symbol — the
actual fb ioctl handler. It is not in either kernel slot we have:

* `ker0` (2021 build) and `ker1` (2019 build) were symbol-reconstructed with `vmlinux-to-elf`
  (kallsyms are compressed, so `strings` never shows them) — 64 k / 66 k symbols, both contain
  `do_fb_ioctl` and `fb_ioctl`, **neither contains `_ker_osd_drm_fb_Ioctl`**;
* no other module in the dump defines it.

Not needed any more, but if we ever want it: `grep _ker_osd_drm /proc/kallsyms` and
`grep -l _ker_osd_drm_fb_Ioctl /lib/modules/*.ko` on the TV answers it in one command (needs the set awake).

## Better route: use DRM, not the vendor ioctls

Because the driver is DRM, the **documented KMS API** replaces the reverse-engineered structs:

```
DRM_IOCTL_MODE_GETRESOURCES / GETPLANERESOURCES / GETPLANE   -> find the OSD plane
DRM_IOCTL_MODE_CREATE_DUMB + MAP_DUMB                        -> a buffer we can write
DRM_IOCTL_MODE_ADDFB                                         -> wrap it
DRM_IOCTL_MODE_SETPLANE (or atomic)                          -> put it on screen
```

No vendor structs, no guessing — and it also explains the failed boot splash: presenting requires
KMS ownership, not just a write into the fb mapping.

`launcher/osdprobe/drmprobe.c` (30 KB static ARM, no libdrm needed — only `<drm/drm.h>`):

```sh
drmprobe                 # read-only: driver name, CRTCs, planes, formats
drmprobe --paint [N]     # create a dumb buffer, paint a pattern, SETPLANE on plane N
```

Validated on a normal DRM host (it reports `amdgpu`, 3 planes, formats `XR24 AR24 …`), so the ioctl usage
is sound; what remains is the TV-specific result (: which driver name it reports, how many planes, and
whether a plane is free while `aurora` holds the KMS master).

## Next

1. with the TV awake: `drmprobe` (read-only) — driver identity + plane list;
2. `drmprobe --paint <spare plane>` — does the set display a dumb buffer?
3. if yes: a DRM boot splash becomes possible (our hook would take KMS, paint, exit, then `exec aurora`),
   and so does a native overlay.

---

# Addendum: on-TV test results (2026-10-30, set awake, input = Steam Deck)

## Historical DRM measurements (the scanout conclusion is withdrawn)

```
driver: ndp | Novatek DRM Driver
crtc 0: id=26 fb=0 0x0 mode_valid=0        <-- no mode, no fb
planes: 8
  plane 0 (id 40), plane 1 (id 54): crtc=0 fb=0, NV21 NV61 NV42 YU24 XR24   (video, unused)
  plane 2 (id 55): crtc=26 fb=63,   NV21 …                                  (video, live)
  plane 3..7 (id 56/59/60/61/62): crtc=26 fb=111/108/112/72/74, XR24 AR24 XR12 AR12  (OSD)
```

A CRTC with `mode_valid=0` and `fb=0` was observed. This does not rule out the
vendor's explicit DRM OSD commit path, recovered later from Aurora.

Also, dumb buffers are unusable from userspace on this driver:

* `MODE_CREATE_DUMB` works (returns handle/pitch) but leaves `size=0` — the size field is not filled;
* `MODE_MAP_DUMB` returns an offset, but `mmap()` on it fails `EINVAL`;
* the alternative `PRIME_HANDLE_TO_FD` path succeeds and its dmabuf also fails `mmap()` with `EINVAL`;
* `DRM_IOCTL_SET_MASTER` returns `EINVAL` (not implemented / not the ownership model here).

Later correction: CREATE_DUMB allocates only 4 KiB despite the dimension-based
reported size. Its large mapping failures do not test the private GEM allocation
path Aurora actually uses. Backend selection requires the recovered vendor
contract; the measurements above do not justify excluding DRM.

## Framebuffers

```
fb0 OSD_NORMAL1  1920x3240 (3 frames) 32bpp
fb1 OSD_NORMAL2  1920x2160 (2 frames) 32bpp
fb2 OSD_NORMAL3  1280x1440 (2 frames) 16bpp
fb3 OSD_CURSOR1  256x512             32bpp
fb4 OSD_CURSOR2  32x64               32bpp
```

Painting `fb2` directly (band, then full frame, both virtual frames) writes the expected pixels
(`1f f8` = magenta RGB565) but **nothing appears on the panel**, with the layer already live. That
confirms the earlier boot-splash finding: an fb write alone is not enough, the frame must go through
the driver's buffer handshake.

## The descriptor ioctl needs something we have not identified

```
ioctl(fd, 0x8020c86c /*region read*/, buf32)  -> ok   {0,0,W,H} and {0,0,3840,2160}
ioctl(fd, 0x800cc884 /*acquire*/, desc12)     -> EFAULT for every layout tried
ioctl(fd, 0x400cc883 /*commit*/,  desc12)     -> EFAULT
```

Tried `{0,0,0}`, `{ptr,bytes,0}`, `{0,ptr,bytes}`, `{bytes,ptr,0}`, `{ptr,0,bytes}`, `{ptr,bytes,1}`
with a 4 KiB-aligned buffer — all `EFAULT`. So the 12-byte descriptor is not a user buffer pointer in
any of those positions; it is likely a physical/handle value, or the ioctl wants a larger struct than
the encoded 12 bytes.

## Tooling notes

* `drmprobe` needed two fixes found here: the ndp driver returns `size=0` from `CREATE_DUMB` (derive it
  from `pitch*height`), and `cd.size` is `__u64` while it was printed with `%u` — on ARM that shifts
  every later vararg and produced a SIGSEGV (the vendor coredump handler reports it as "Killed",
  dmesg shows `[COREDUMP] SIGNR:11`).
* `fbpaint` / `tdgaprobe` / `tdgaexp` are disposable probes kept in /buffer on the set.

## Updated plan for `mod_display`

1. Preferred: link `/vendor/lib/dfb/libdirectfb-1.7.so.1` + `libdirectfb_tdga.so` and create the
   surface through DirectFB, exactly as aurora does. Removes all ioctl guessing; costs a DFB dependency.
2. Fallback: keep reversing the tdga descriptor ioctls until acquire/commit work, then drive the
   layer ourselves (smaller dependency, more work).
3. Superseded: these measurements did not establish that DRM is unusable for scanout.

## Follow-up measurement: fb1 raw writes ARE visible (important correction)

With the launcher closed (its page gone, `fb1` uniformly blank), a plain `mmap` + write to
`/dev/fb1` (OSD_NORMAL2, 1920x1080, 2 buffers, RGBA) **did appear on the panel** — a magenta band
across the top was confirmed visually. Readback showed the expected pixels (`ff 00 ff ff`).

So the earlier "a raw fb write stays invisible" conclusion was **too broad**. What it actually meant:

| case | result |
|---|---|
| `fb2` (OSD_NORMAL3, 1280x720 16bpp), any time | **invisible** — that plane is not enabled on this model |
| `fb1` before aurora/DFB initialised the layer | invisible (the original boot-splash result) |
| `fb1` after the layer is initialised | **visible** — plain writes work |

`FBIOPAN_DISPLAY` also works on fb1 (`0x4606` returns 0, `yoffset` readback `1080`), so double
buffering and flips are available without the tdga acquire/commit handshake.

One anomaly: a pan **back** to frame 0 was accepted (`0`, no error) but `yoffset` read back as `1080`,
i.e. it did not take effect. Either the driver only flips to a buffer it handed out, or a pan needs
the descriptor handshake after all. Untested: whether the flip is tear-free.

### What this means for `mod_display`

Direct `/dev/fb1` mmap is the cheapest viable path, no DRM and no DirectFB:

1. `open("/dev/fb1")`, `FBIOGET_FSCREENINFO/VSCREENINFO`, `mmap` the whole `yres_virtual`.
2. render into the non-displayed frame, `FBIOPAN_DISPLAY` to present.
3. still open: whether we can bring `fb1` up ourselves before aurora has ever run (the boot-splash
   case) — that is now the *only* remaining display question, and it decides whether the native shell
   can paint before aurora starts or must do a one-time layer init first.

## Boot-time paint: it DOES work (this supersedes the old "boot splash is impossible" note)

A one-shot test was added to the hook (run before `exec aurora`, guarded by `/conf/bootpaint_once`)
which mmaps `/dev/fb1` and fills the screen in three colours. Result, confirmed visually:

* **the colours appeared at boot, before aurora started** — so the OSD plane *is* scannable from the
  hook, contrary to the 2026-10-30 boot-splash conclusion;
* but they covered only the **top-left quarter** of the screen.

That quarter is explained by the region readout taken while aurora is running:

```
region(108) -> {0, 0, 1920, 1080} | {0, 0, 3840, 2160}
                ^ current x,y,w,h    ^ max / canvas
```

The OSD canvas is **3840x2160** (4K panel) while `/dev/fb1` is 1920x1080, so a full-frame write covers
a quarter until the region is set. aurora sets it; before aurora runs it is still the default.

To fill the screen from the hook we must either write the region (`0x4020c86b`, 32-byte struct
`{x,y,w,h,?,?,maxw,maxh}`) to `{0,0,1920,1080}`, or render at 3840x2160.

Status: the region-write experiment was prepared (`bootpaint` v3 logs to `/conf/bootpaint.log`) but the
reboot trigger did not fire on the attempt, so it is **unconfirmed**. Everything was then reverted: the
hook is back to its shipped version (md5 `2b1d4b7dc7acad8ca0e9b5ccaff03da7`, matching
`artifacts/payload/aurora.elf`) and the test binaries were removed.

Net: `mod_display` has a viable no-DFB path at boot — set the region, then mmap + paint + pan.

## `mod_display` implemented and verified (same day)

`openaurora/modules/display/` does exactly that and was confirmed on the set:

```
INFO display  /dev/fb1: 1920x1080 32bpp, 2 frame(s), back=1
INFO display  self-test: magenta band painted     <- visible on the panel
```

* opens `display.fb` (default `/dev/fb1`), reads var/fix, computes the frame count from
  `yres_virtual/yres` and starts on the frame that is *not* displayed;
* renders with `clear()` / `fill(x,y,w,h,r,g,b)` into the back frame, `msync()`, then
  `FBIOPAN_DISPLAY` to present and swaps frames;
* publishes a `display` service (`clear`, `fill`, `present`, `info`) so a UI module never touches fbdev;
* `display.set_region=1` writes the OSD region back to the framebuffer size (the 4K→1080p fix for the
  boot case, still unverified on a cold boot);
* `display.test=1` paints a band for a one-command visibility check;
* suspend/resume close and reopen the layer.

Still open: the region write on a *cold* boot (before aurora), which is the same session as the
native-shell ownership test.

## `mod_audio` — implemented, and the init order is mandatory

`void Hal_Audio_Ctrl(int opcode, uint *params)` with a 56-byte parameter block (decompiled):

| opcode | use | block |
|---|---|---|
| `0x33` | main volume | `params[0] = volume` (aurora: `PLAT_DEC_SetAudioMainVolume`) |
| `0x35` | speaker mute | `params[0] = speaker dev id`, `params[1] = 0/1` (`PLAT_AOUT_SetSpeakerHWMute`) |
| `0x3a` | I2S MCLK | `params[0] = 0x100` (`DEV_AMP_Init`) |

**The opcodes cannot be called cold.** Doing so killed the process (vendor coredump handler reported it
as "Killed"; dmesg `[COREDUMP] SIGNR:11`). The mandatory prelude is aurora's driver init:

```c
Hal_Audio_Protect_Init();     // void(void)
Hal_Audio_Mixer_Ctrl_Init();  // void(void)
Hal_Audio_Proxy_Init();       // void(void)
```

(`PLAT_DRV_INI_Init` order: Protect → Mixer → Proxy → `plat_common_init("DVIF")` → `Hal_Gpio_Init` →
`Hal_Iic_Init` → `plat_vin_slot_init` → `NDAL_VideoExt_OpenDevice(0,10)`.)

With the prelude, `mod_audio` applies volume 15/20/100 on the set with no crash. It could not be
verified *audibly* because this household routes volume to an external soundbar and has the TV's own
audio levels disabled — the TV amp is not the volume authority. Re-test with TV speakers selected.

Note for safety: this is the first module that can kill the process from a HAL call, which is the
concrete argument for eventually isolating the riskiest HAL users in a helper process.
