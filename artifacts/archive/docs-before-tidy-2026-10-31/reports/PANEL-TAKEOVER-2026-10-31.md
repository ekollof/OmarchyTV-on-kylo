# Taking the panel: watchdog, layers, planes (2026-10-31)

**Later continuation and audit:** recovery supervision is now implemented and deployed
with native boot disabled; see `openaurora/docs/RECOVERY.md`. The old init/scanout
claims below are superseded by `DRM-PLANE-CONTRACT-2026-10-31.md`: `_kms_init` does
discovery, while explicit OSD commits are in `plat_Gfx_DRM_osd_drv::Update_Enable`.
`TODO-AUDIT-2026-10-31.md` records further incomplete module and lifecycle claims.

What this round established, in order of usefulness. Everything here is read out of the firmware
(the vendor binaries, the driver, the kernel with recovered symbols) or measured on the set — not
inferred from a screenshot.

---

# Pick up here — state at the end of 2026-10-31

**Set**: reachable over ssh/telnet, vendor UI running, payload hook (226 lines, md5 `2b1d4b7d…`),
telemetry blackholed (3 `vstlsrv` entries in `/etc/hosts`), `dialserver` up, `/conf/coredumpnoreset`
set (a crashed process no longer resets the TV). Deployed under `/buffer/openaurora/`: binary
`3fa345d7…`, 11 modules, 15 UI blobs, plus `wdtctl`, `fbregion`, `drmprobe`. No `oa_*` markers except
`coredumpnoreset`.

**Works, verified on the set**

* `mod_plat` runs the vendor's bring-up in `PLAT_DRV_INI_Init` order (audio prelude → `plat_common_init
  ("DVIF")` → GPIO/I2C → VIN defaults → `NDAL_VideoExt_OpenDevice(0,10)`), and `display`/`panel`/
  `audio`/`source`/`ir` refuse to start without it.
* `mod_display` opens `/dev/fb1`, commits the region, flips; `ui_fb` paints the parity portal
  (1.65/255 vs the stock reference) and writes `/conf/oa_ok`.
* Runtime swap: `wdtctl disable` + `kill -9` the vendor UI → **no reboot**, appmgr does not respawn,
  our binary runs and owns fb1/IR/panel.
* Crash safety: `/conf/coredumpnoreset` verified with a deliberate SIGSEGV (uptime advanced,
  `always_logging_reset_count` 2→3, set stayed up).

**Not solved — the single blocker**

Our OSD is not on the glass (HDMI1 is). Unproven hypotheses, in order: plane z-order; OSD plane never
enabled; plane scanning a buffer that is not the fbdev mapping; our own region write hiding the window.
Note the old §4 measurement: with no vendor UI at all, `/dev/fb1` never appeared for our process — that
has never been retested since.

**Next steps (from the adversarial review, §10)**

1. **Cold-boot trace of our own process** with the vendor UI never started (tracer loaded into
   openaurora, not into aurora): does anything create `/dev/fb1`, and does any `SETPLANE` reference the
   buffer we mmap?
2. **Supervisor in the hook** before any `/conf/oa_boot` test: a separate process waiting on a timestamp
   the *host loop* rewrites each second; on stall → `SIGKILL`, re-arm the MCU watchdog (`wdtctl set 10`),
   then `exec` the vendor UI. `oa_ok` written once must not be what keeps a UI alive.
3. Only then consider a boot-time takeover.
4. **Do not** drive `dp_order` again: the object id is `info[1]` from a 12-word (0x30-byte) plane record,
   `0xcccccccc` is `DRM_MODE_OBJECT_CRTC`, and a bad property write oopses the kernel —
   `coredumpnoreset` does not cover that.

**Traps worth remembering** (all measured today): `core_helper.sh` resets on any crash;
`/vendor/run.sh` arms a 10 s reset when `/tmp/trigger_reboot` appears; `SIGSTOP` keeps aurora's fds and
DRM master, `SIGKILL` releases them; `--allow-shlib-undefined` hides missing vendor symbols until
runtime (`v4l2_open`); `"key = value"` in a config file used to keep a leading space (fixed, tested).

## 1. The MCU watchdog can be disabled from userspace — verified

This was the gate on everything else: vendor aurora is what feeds the MCU watchdog, so stopping it
resets the SoC (reproduced three times earlier).

`PLAT_SYS_KickWatchDog()` decompiles (Ghidra, aurora) to a call to
`NDAL_VideoExt_STBC_FreshWDT()`. Reaching that through `libNDAL_EX` from outside aurora **resets the
set**: the STBC path needs the vendor's MCU state machine (`ndcommon_v4l2_config_ctrl` →
`_send_v4l2_command` → `ndcommon_v4l2_ioctl` on a registered fd), and a half-initialised call panics
the kernel.

The working route is `pcbconfig` (`/usr/sbin/pcbconfig`, 16 KB, links only libc, uses open+write):
it talks to the MCU over a 6-byte packet on `/proc/nvtstbcipc`.

```c
open("/proc/nvtstbcipc", O_RDWR); fcntl(F_SETLK, F_WRLCK);
packet = { 0, 0xae, arg0, arg1, arg2, 0xae }; write(fd, packet, 6);   /* read 6 back for an ack */
```

* `FIPC_set_watch_dog(sec)`  → cmd `0xae`, arg0 = sec>>8, arg1 = sec&0xff, arg2 = 1
* `FIPC_disable_watch_dog()` → cmd `0xae`, args 0,0,0

Implemented as `openaurora/tools/mcu/wdtctl.c` (`disable` / `set <sec>` / `get`). Note: this
toolchain emits `fcntl@GLIBC_2.28`, the set has 2.23, so the lock goes through `syscall(SYS_fcntl,…)`.

**Measured**: after `wdtctl disable`, vendor aurora can be `SIGSTOP`ped and the set stays up
(224 s and counting, versus a reset within ~40 s before). This is the single biggest unlock: it
makes "openaurora runs alone" testable.

## 2. The OSD region: what the driver reports, and what we write

`NVTGA_OSD_SetRegion` (libdirectfb_tdga, decompiled) does a read-modify-write on a 32-byte struct:

```c
ioctl(fd, 0x8020c86c, region);            /* read current */
region.w = want_w; region.h = want_h;     /* words 2 and 3 (local_b8 / local_b4) */
ioctl(fd, 0x4020c86b, region);            /* write */
ioctl(fd, 0x4004c871, strided_bytes);     /* stride */
ioctl(fd, 0x4008c80a, cfg8);              /* 8-byte layer config */
```

Reading it back from the **live, working** driver (`openaurora/tools/mcu/fbregion.c`, vendor aurora
running) gives the layout outright:

```
/dev/fb1 region: [0]=0 [1]=0 [2]=1920 [3]=1080 [4]=0 [5]=0 [6]=3840 [7]=2160
                 x    y    w      h                    ?      canvas_w canvas_h
```

So words 2/3 are the window size, 6/7 the 3840x2160 canvas. An earlier note in this file claimed the
size lives in words 4/5 — that was wrong, and writing it there is what put our layer in the
bottom-right quadrant on the panel. `mod_display` now writes `[2]=w [3]=h [4]=0 [5]=0` and keeps the
rest of the read-back.

Also from the same function: the OSD layer is **region based** — `NVTGA_OSD_AddRegion`,
`RemoveRegion`, `TestRegion`, `UpdateRegion`, `FlipRegion` — and there is no alpha/opacity control
anywhere in the OSD driver.

Everything else about the region turned out to be a red herring: the reason our `open()` failed was the
config parser keeping a leading space (`display.fb = /dev/fb1` resolved to `" /dev/fb1"`), fixed in
`core/host.c` and now covered by `tests/run.sh`.

## 3. The OSD is not visible — hypotheses, not yet a conclusion

**Retraction (after adversarial review, 2026-10-31):** this section used to state that "the video plane
sits above the OSD". That is *not* established. What the frozen-aurora run actually showed is that
`/dev/fb1` held our pixels while the glass showed HDMI1 — two different objects. Aurora was only
`SIGSTOP`ped, so it still held DRM master (the trace has `SET_MASTER` failing with `rc=-1` on the second
DRM fd), which means our `Hal_Video_DrmInit(1)` in that process could not have restacked anything, and
its return value was never checked. Equally consistent with the glass: the OSD plane is not enabled, the
plane scans a GEM buffer that is not the fbdev mapping, or our own region write hid the window.

The candidate list below is therefore a hypothesis set, not a finding:

Boot trace of aurora's display calls (`artifacts/protocol/oatrace-boot-2026-10-31.txt`, captured with
`openaurora/tools/hold/oa_trace.so` as an `LD_PRELOAD` in the vendor process):

| ioctl | meaning | payload seen |
|---|---|---|
| `0xc03064b7` | `DRM_IOCTL_MODE_SETPLANE` | plane 0x3c, crtc 0x1a, fb 0x42, crtc 3840×2160, src 1920×1080 (16.16) |
| `0xc03064b7` | second plane | plane 0x3b, same crtc, src 1920×1080 |
| `0xc03064b7` | third plane | plane 0x38, src 0x0168×5 — small overlay |
| `0xc1a46451` | **Nuvoton vendor DRM ioctl**, 420-byte payload | plane table (see below) |
| `0xc01864ba` | `DRM_IOCTL_MODE_ADDFB2` | framebuffer add |
| `0x4606` / `0x4600` / `0x4602` | fbdev pan / get var / get fix | the OSD layer |

`Hal_Video_Set_DrmPlaneInfo(int plane, u32 info[12])` (libNDAL3, decompiled) copies 12 words into a
per-plane record at `base + plane*0x30 + 8` — i.e. a **12-word plane descriptor in a 0x30-stride
table**, which is what `0xc1a46451` hands to the kernel.

So the z-order/visibility question is answered structurally: the video path owns the plane stack
through DRM, and the fbdev OSD layer is composited according to the region it has committed. When
the vendor process stops, nobody re-commits the region or touches the planes, so the panel keeps
showing whatever the video plane had — in our tests, HDMI1.

**Consequence for the roadmap**: "own the screen like aurora does" means implementing this plane
setup, not just the fbdev layer. The remaining unknown is the field layout of the 12-word plane
descriptor (`0xc1a46451`); it is defined in the kernel display driver, and the trace payload is
mostly pointers, so it must be read from the driver rather than pattern-matched from a capture.

## 4. The takeover (replacing aurora as the idtv app) — built, and where it stops

Implemented and safe, in the boot hook (`/conf/remotelf/aurora.elf`, block starts at "openaurora
takeover"):

* `/conf/oa_boot` present → the hook runs openaurora **instead of** `exec /vendor/com.nvt.idtv/aurora.elf`;
* openaurora writes `/conf/oa_ok` after its first successful paint, and only then is the takeover kept;
* otherwise the hook kills it, increments `/conf/oa_fail`, and `exec`s the vendor UI **in the same boot**
  (3 failures delete `/conf/oa_boot`); `/conf/oa_safe` forces the vendor UI outright.

That guard worked every time we tried a broken build — the set always came back with the vendor UI.

What blocks a *successful* takeover is display bring-up, not the hook:

| fact | evidence |
|---|---|
| the fbdev nodes are not simply "there at boot" | hook's `[ -c /dev/fb1 ]` true at 1.3 s, our process then sees it missing for 40 s, and it is present again once vendor aurora starts |
| fb registration belongs to the OSD/DRM bring-up | `kosd` (in `ntkdriver`) + `ntkndpdrm` (standard DRM/KMS, fbdev emulation); `dmesg`: `kosd: nvt_device registered`, `[drm] Initialized ndp` |
| aurora's own order for it | `Hal_Video_DrmInit(1)`: `_hal_video_drm_open` (retries 5 s) → `_hal_video_drm_info_init` → `_hal_video_drm_create_fb` → `_hal_video_drm_kms_init`; boot trace then shows `ADDFB2` + `SETPLANE` |
| calling that alone is not enough | `drm_init: Hal_Video_DrmInit(1) done` in the log, and `/dev/fb1` still missing |
| the vendor module parameters matter | `/lib/modules/install_fpga.sh`: `insmod /vendor2/modules/ntkdriver_nondovi.ko gau32PlaneWinSize=1920,1080,1920,1080,1280,360,256,256,32,32 gau32PlaneOrder=4,0 gau32NumOfBuffer=3,2,2,2,2 gau32SeperateBuffer=0,2,0,0,0 gu32FastLogoPlaneID=0`, then `echo 1 > /tmp/ntkdriver_completed` (the flag aurora's `ICE_HardwareInit` waits on) |

Reusable finding from the same work: **`libNDAL3.so` cannot be `dlopen`ed on its own** — it references
globals that live in `libNDAL_EX.so` (`gs32Ndp_PQ_fd` was the first error), so `libNDAL_EX` has to be
in the global scope first (`dlopen(..., RTLD_GLOBAL)`) or, better, link both.

### Next step (firmware-specified, not guessed)

Reproduce the vendor's platform init in our own process before touching fb. Our own audio work already
recovered that order from `PLAT_DRV_INI_Init`:

```
Protect → Mixer → Proxy → plat_common_init("DVIF") → Hal_Gpio_Init → Hal_Iic_Init
        → plat_vin_slot_init → NDAL_VideoExt_OpenDevice(0, 10)
```

`plat_*` and `PLAT_DRV_INI_Init` are *inside* aurora (not in `libNDAL3`/`libNDAL_EX`), so this is the
point where a `plat` module either wraps the DRM path from `libNDAL3` (`Hal_Video_DrmInit` +
`Hal_Video_Set_DrmPlaneInfo` + `Hal_Video_FlushPlaneInfo`) or, more faithfully, links **DirectFB**
(`/vendor/lib/dfb/libdirectfb-1.7.so.1` + `libdirectfb_tdga.so`) exactly as aurora does and lets it own
the layer. The second option is what `OSD-INIT-RE-2026-10-30.md` already recommended as preferred.

Until that exists, the honest verification path stays the framebuffer compare (`UI-PARITY-2026-10-31.md`).

### Final measurements (this session, per-boot log)

Reading `/conf/tvhack.log` from its last `== tvhack boot` header (the file accumulates across boots —
an earlier reading of mine mixed boots and produced a wrong timeline):

```
openaurora: ntkdriver_completed=yes after 1800 ms     <- install_fpga.sh finished
openaurora: /dev/fb1 present after 000 ms             <- [ -c /dev/fb1 ] true
[  5932] WARN display  /dev/fb1: open No such file or directory, stat missing (waited 0 ms)
[  6932] WARN display  ... missing (waited 1000 ms)
   … missing for the whole 60 s window …
ok= (never written)   fail=1                          <- hook fell back to the vendor UI
```

Also checked: `/proc/self/ns/mnt` is the same for my shell and the app (`mnt:[4026531840]`), `/dev` is
devtmpfs, `/tmp` is tmpfs (so the completion flag is genuine, not stale). So the hook's own child sees
`/dev/fb1` disappear ~100 ms after `install_fpga.sh` finished, and it does not come back until the
vendor UI runs.

That is the concrete shape of the remaining problem: **the OSD framebuffers are registered by the
vendor OSD/DRM bring-up and are not a stable, always-present interface at app start.** Raw fbdev from
the hook is therefore not a reliable foundation — which is what `OSD-INIT-RE-2026-10-30.md` and
`MEMORY.md` (line 708) both concluded: use DirectFB, as aurora does.

## 5. Initialisation-path comparison: vendor vs openaurora (2026-10-31)

Done by tracing the vendor binary from `main()` (decompilations in `/tmp/opencode/ghidra_*`) and
comparing it with what our process actually runs.

```
aurora main() @006b7320
 └ FUN_007431d8: ICE_InitialiseSystemMemory → ICE_DebugInit → profileManagerStart
                 → FROST_DebugStartConsole → ICE_HardwareInit → ICE_InitRTC → eclipseStart
      ICE_HardwareInit @007551a0 (waits /tmp/ntkdriver_completed)
       └ PLAT_DRV_INI_Init @0089b6e4
          1 Hal_Audio_Protect_Init(0) / Mixer_Ctrl_Init / Proxy_Init
          2 plat_common_init("DVIF") @009247f8
              Vpltio_Init → V4l2InfoInit → DrmShareMemCreate → DrmInit(1) → Vpltio_SendCmd(0x302,src=1)
          3 Hal_Gpio_Init
          4 Hal_Iic_Init
          5 plat_vin_slot_init        (pure data: 7 slot descriptors + 0x150 bytes of state)
          6 NDAL_VideoExt_OpenDevice(0,10)
      then fbdev writes + FBIOPAN_DISPLAY; the layer is made visible by the DRM plane setup
           (ADDFB2 / SETPLANE), not by an InitLayer ioctl - see below
```

| stage | vendor | openaurora before | now |
|---|---|---|---|
| audio prelude | first call of all | `mod_audio` did it, loaded *after* `display` | **`mod_plat`, first** |
| common init | `plat_common_init("DVIF")` | inside `mod_display`, prelude possibly not run | **`mod_plat`**, exact order + `SendCmd(0x302,src=1)` |
| GPIO / I2C | `Hal_Gpio_Init`, `Hal_Iic_Init` | never called | **`mod_plat`** |
| VIN slots | `plat_vin_slot_init` | never | **`mod_plat`**: same 7-slot defaults (`{0,7,7,1,1,0,4,0,0}` + `{7,0,25}`, 0x150 bytes of state) — it is pure data, no HAL calls |
| video device | `NDAL_VideoExt_OpenDevice(0,10)` | never | **`mod_plat`** |
| UI layer | fbdev writes + `FBIOPAN_DISPLAY`; layer made visible by the DRM plane setup | raw fbdev writes, planes never configured | **same path, now with the planes configured** (`Hal_Video_DrmInit(1)` → `_hal_video_drm_kms_init` → `ADDFB2`/`SETPLANE`) |

### The DirectFB question: what the capture can and cannot say

The earlier hypothesis was that the vendor UI owns the layer through DirectFB's `NVTGA_OSD_InitLayer`
(12-byte descriptor `0x400cc883` + 8-byte config `0x4008c80a`). The boot trace says otherwise: across
the whole vendor startup there is **not one** occurrence of `0x400cc883`, `0x800cc884` or `0x4008c80a`.
That is weaker evidence than it looks: the tracer hooks libc `ioctl` only, only for fds whose path
contains `/fb` or `/dri`, only the first three of each request, and the same capture contains *no*
region ioctls either (`0x4020c86b`/`0x8020c86c`), which the vendor driver certainly issues. So the
correct statement is "not observed through this tracer", not "the vendor does not do it".
What is there is fbdev `0x4600`/`0x4602`/`0x4606` (get var / get fix / **pan**) plus the DRM plane
ioctls (`ADDFB2 0xc01864ba`, `SETPLANE 0xc03064b7`, and the Nuvoton table `0xc1a46451`).

So the vendor draws into fbdev and flips, exactly like `mod_display`, and its layer becomes visible
because the **DRM planes are configured** at startup — which `Hal_Video_DrmInit(1)` does. Adopting
DirectFB is therefore not required for layer ownership; the requirement is the plane setup, and that is
now part of `mod_plat`'s common-init stage.

Implemented: `openaurora/modules/plat/` publishes the `plat` service, and `display`, `panel`, `audio`,
`source` and `ir` declare `requires = { OA_SVC_PLAT }`, so the host refuses to start any HAL consumer
before the platform bring-up has run — the ordering is now enforced by the module system rather than by
the order of a config line. `mod_display`'s own bring-up is off by default (`display.drm_init=0`),
because the vendor does `plat_common_init` exactly once.

No structural divergence remains in the initialisation path: every stage of `PLAT_DRV_INI_Init` is now
either called in the vendor's order or, for `plat_vin_slot_init`, reproduced as the same initial data.
What is left is verification on the set (a dry run, not a boot change).

## 6. The two watchdogs (decompiled)

The in-app watchdog is not a separate supervisor — appmgr has no watchdog and no respawn (it only
has `timeout`/`exit_timeout` and `waitpid`). What exists:

| piece | behaviour (decompiled) |
|---|---|
| `ICE_HardwareWatchdog(1)` @0074c434 | only sets *enabled* and a **10 s** default timeout |
| `ICE_HardwareSetWatchdogTimeout(sec)` @0074c4f0 | stores `sec * 1000` ms in a global (0 when negative) |
| `PLAT_SYS_KickWatchDog()` @008c4fac | the kick: `NDAL_VideoExt_STBC_FreshWDT()`, with a timing guard and a "Monitor MCPU fail" escape |
| `ICE_HardwareInit` | `if (ICE_HardwareWatchdog(1) == 1) FROST_ThreadCreate(...)` then `PLAT_SYS_MCPU_MonitorTask_Start()` |
| SoC hardware WDT | not driven by us; resets the box on a kernel crash/hang (this is what fired when the `dp_order` call crashed the kernel) |

So the MCU watchdog is fed by a thread **inside aurora**. Can we feed it instead, so a takeover keeps a
live safety net? Measured (dry run, vendor aurora frozen, watchdog disabled, `plat` fully initialised
including `NDAL_VideoExt_OpenDevice(0,10)`):

```
plat: NDAL_VideoExt_OpenDevice(0,10)
plat: FreshWDT probe -> 1 (0x1)          <- aurora treats non-zero as "Call PLAT_SYS_KickWatchDog() fail"
plat: bring-up complete (errors 0)
… set stayed up (uptime 218 s)
```

`FreshWDT` returned 1 in that one probe. **That does not prove it cannot be fed** (review caught the
overreach): the probe ran with the watchdog already disabled and never called `SetWDT` /
`SetMcuWatchDogTimeout` first, which is what `ICE_HardwareWatchdog(1)` does before aurora's kick thread
exists. `PLAT_SYS_KickWatchDog` also bails early on its own timing globals and on the "Monitor MCPU
fail" flag without calling `FreshWDT` at all. The honest status is: one call, in the wrong state,
failed — the STBC init still has to be read before any conclusion about feeding it. A takeover therefore must keep it disabled (`wdtctl disable`) and treat the heartbeat +
same-boot fallback as the only recovery mechanism, because there is no watchdog left to reset a hung UI.

## 7. The boot chain, and what actually resets the set (2026-10-31)

Traced because "something monitors aurora and reboots when it changes" was suspected. The boot chain:

```
/etc/inittab            sysinit → /etc/rc.local
rc.local                show_bootlogo &  active_usb &  mount_ubifs (→ insert_idtv_ko &, mount_apdat &)
                        setup_network &  enable_swap &  → /usr/script/startup.sh → /vendor/run.sh
run.sh                  sets up dirs/env, then appmgr (or run_aurora.sh), then:
                        trigger_reboot &        (waits for /tmp/trigger_reboot)
core_pattern            | /usr/sbin/core_helper.sh gzip %p %e %t
```

Two reset paths, both explicit — and neither is a supervisor for aurora:

| trigger | what happens |
|---|---|
| `/tmp/trigger_reboot` appears | `run.sh`'s `trigger_reboot`: `pkill -f aurora.elf`, `pcbconfig -i -w 10`, detach `/mtd_rwarea` `/conf` `/buffer`, run the file's command |
| **any process dumps core** | `core_helper.sh`: if `/conf/coredumpnoreset` is absent → `before_system_reset` (detach the same three) + `pcbconfig -i -w 1` → SoC resets ~1 s later |

The second one is what "bricked" the set every time a probe crashed: `always_logging_reset_count` had
reached **2**, matching the two crashes. `/conf/coredumpnoreset` is the documented switch that makes the
handler call `pcbconfig -i -w 0` instead, and it is now set on the set; `/conf/enable_coredump` (absent
here) only controls whether cores are written to USB.

Verified on the set: with `/conf/coredumpnoreset` present, a deliberate `SIGSEGV` left the box running
(uptime 387 → 395 s, reset counter 2 → 3, `dmesg` `[COREDUMP] SIGNR:11`, aurora and dialserver still
up) — the handler took the `stop_watchdog` path and exited. Without it, that same crash would have
detached `/conf` and `/buffer` and armed a 1 s reset.

Consequence for the takeover design: the set does **not** reboot because aurora is missing or changed —
it reboots because something crashed. That makes a development takeover far safer than the earlier
"MENU disappears → reboot" reading suggested, provided crashes are not treated as fatal
(`coredumpnoreset`) and the hook keeps its own heartbeat/fallback.

## 8. Where that leaves the work

### Dry run result (2026-10-31, vendor aurora frozen, no boot change)

The init path is correct and verified end to end:

```
plat: audio prelude (Protect/Mixer/Proxy)
plat: common init (vpltio/v4l2/sharemem/drm, DVIF)
plat: VIN slot defaults (7 slots)
plat: NDAL_VideoExt_OpenDevice(0,10)
plat: bring-up complete (errors 0)
display: /dev/fb1: 1920x1080 32bpp, 2 frame(s)     region -> 1920x1080 (ok)
ui_fb: painted portal: 5 rail items, 9 cards, focus 2     /conf/oa_ok = "5824 1920x1080"
fb1: 98.6 % non-black, mean |delta| vs the stock reference 1.65/255
```

The dry run also found one real link gap that no amount of reading would have produced: `plat` died with
`libNDAL_EX.so: undefined symbol: v4l2_open` because we linked NDAL3/NDAL_EX/asound but not
`libv4l2.so.0` (a NEEDED entry of aurora). Fixed: the vendor link set is now
`NDAL3 + NDAL_EX + asound + v4l2 + v4lconvert + nteec + mhash` with `-Wl,--no-as-needed` (without it the
linker drops the libs we do not call directly), and `fetch-vendor.sh` knows about the two new ones.

### The one thing still in the way: the video plane is above the OSD

With the vendor process frozen, our fb1 content is correct but **not on the glass** — HDMI1 is. When
aurora's own portal is up, our fb1 writes *are* visible, so the vendor explicitly sets the stacking, and
we have found that call:

```
plat_drm_set_windoworder @0092d090           eWindowOrder 0..5 -> dp_order 0,2,1,3,4,5
  dev = Hal_Video_Get_DrmDeviceId(0)
  Hal_Video_Get_DrmPlaneInfo(0, &info)
  Hal_Video_Drm_Set_Property(dev, 0xcccccccc, info[1], "dp_order", value, 0)
```

`mod_plat` has the call behind `plat.window_order` (off by default). **Calling it with order 2 from our
process reset the set** (kernel crash, SoC reboot) — so it is *not* a knob to try blindly: the plane id
`0xcccccccc` and the `info[1]` word have to be derived from the vendor's plane table first (which plane
the UI uses, and what that second word is), and it must not be driven at boot before that is known.

Safe next step: read `Hal_Video_Get_DrmPlaneInfo` / `Hal_Video_Set_DrmPlaneInfo`'s 12-word descriptor
from the firmware to learn what `info[1]` means for the OSD plane, then drive `dp_order` with the same
plane the vendor uses — verified by a dry run, never by a boot change first.

Done and verified:

* parity render (`ui_fb` vs the stock portal: **1.62–1.69 / 255 mean error**, `UI-PARITY-2026-10-31.md`);
* watchdog control (`wdtctl`, verified on the set);
* region struct fix in `mod_display`;
* an ioctl tracer that records the vendor's display bring-up on any boot
  (`touch /conf/oa_trace_enable`, then read `/conf/oa_trace.log`).

Next, in order:

1. read the 12-word plane descriptor out of the kernel display driver
   (`ker*_sym.elf`, handler for `0xc1a46451`) and document the fields;
2. implement `display.planes` in `mod_display` (or a new `vpp` module): planes + region commit, so
   the OSD is visible over video without the vendor process;
3. only then add the boot-hook test mode (`/conf/ui_test`) that starts openaurora instead of aurora,
   with a fail counter so a bad boot heals itself;
4. keep vendor aurora out of the loop for functional parity work — with `wdtctl disable` this is now
   a two-command sequence, not a brick risk.

## 9. Adversarial review (Grok 4.7, 2026-10-31) — what it caught

A second model was given the code and the report and asked to attack the reasoning. The findings that
held up, and what was done about each:

| finding | action |
|---|---|
| `0xcccccccc` is `DRM_MODE_OBJECT_CRTC`, not a "wildcard plane"; the object id is `info[1]`, and `Hal_Video_Get_DrmPlaneInfo` writes **0x30 bytes** into what `mod_plat` declared as `uint32_t info[4]` — a 32-byte stack overwrite before the property call | `plat.window_order` deleted entirely; do not re-add without a 12-word buffer and a checked return |
| "video plane sits above the OSD" was asserted from a run where aurora still held DRM master | retracted in §3, replaced by the hypothesis list |
| "DrmInit(1) configures the OSD planes" overclaims a call graph (void return, never checked; `create_fb` is a DRM fb, not `/dev/fbN`) | plat now logs `sequence done (… unverifiable: void returns)` instead of "bring-up complete" |
| "DirectFB is unnecessary" rested on absence in a filtered trace that also lacks the region ioctls | softened in §5 to what the tracer can actually observe |
| "the MCU watchdog cannot be fed" came from one `FreshWDT` call with the watchdog already disabled | retracted in §6 |
| `oa_ok` is a one-shot receipt, not a heartbeat; the UI has no input handling and ignores SIGTERM | open — this is the next design step (§10) |
| `ui_fb` dropped its display pointer on suspend without releasing the hold | fixed: `m_suspend` releases |
| `api_lookup` added a hold per call and returned the iface even when the hold table was full | fixed: one hold per (holder, service), error logged when full |
| `oa_module_stop` left timers/watches/services registered; failed `start` leaked module resources | fixed: both paths call `stop()` + `oa_host_cleanup_owned()` |
| test 11 claimed to test `requires` but asserted nothing | fixed: it now starts `display` without `plat` and expects the refusal |

Two of its claims were wrong and were checked rather than applied: the `SETPLANE` source rectangle is
`src_h=0x438`/`src_w=0x780` (1080/1920 in 16.16, matching `drm_mode_set_plane`'s field order), and
`--allow-shlib-undefined` is still needed for the vendor libs' own unresolved references.

## 10. What the review says to do next (accepted)

1. **Do not** retry `dp_order` with a "valid plane id" — it is the wrong argument (the object id is
   `info[1]`), and a guessed property write can oops the kernel, which `coredumpnoreset` does not cover.
2. **Cold-boot trace of our own process** with aurora never started (tracer on openaurora, not on
   aurora): does `DrmInit(1)` create `/dev/fb1` and a scanned OSD plane? The old §4 measurement says
   no, and the dry run never retested it because it opened a node aurora had already created.
3. **A supervisor in the hook**, before any `/conf/oa_boot` test: a process that is not the UI, waiting
   on a timestamp the *host loop* rewrites every second; on stall → `SIGKILL`, re-arm the MCU watchdog
   (`wdtctl set 10`), then `exec` the vendor UI. `oa_ok` alone must not decide to keep a process.
4. Only then, with the glass confirmed by a readback *and* a human, consider a boot-time takeover.
