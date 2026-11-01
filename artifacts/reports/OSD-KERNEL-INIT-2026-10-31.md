# Kernel OSD init and physical binding — offline follow-up

> **Status: Current registration/callback/order evidence. Later MAU/pool work is in OSD-MAU-SELECTION and OSD-BUFFER-CONTRACT.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Source: `vendor2:/modules/ntkdriver_nondovi.ko`, MD5
`fded7005651e37b8a1b5e8e89f73f4ff`. Decompilation in the existing `nondovi`
Ghidra project, with read-only/noanalysis mode. Cross-checked allocation and
registration calls against ARM ELF relocation records and the physical-binding
function against instructions. No TV access or hardware experiments.

## Framebuffer registration is a kernel initialization step

`KER_OSD_DrvInit` is ELF **`.init.text+0x1904`**, Ghidra `0x687b74`. This is
not an ordinary `.text` address; do not apply the `.text` address offset to it.
The function processes sizes/counts/order parameters, obtains the driver callback
table, allocates five `fb_info` objects with 0x3c8 bytes of private state, supplies
them to `pfSetFbInfoPointer`, and runs `pfPreInitSetting` for each plane.

It then obtains contiguous memory (or uses explicit per-plane buffers), installs
the fb operations and screen-info templates, and supplies physical base/size to
each `fb_info`. After CheckVar, it calls SetPar for non-fast-logo planes when
CheckVar succeeds, then calls `register_framebuffer` for all five planes.

Disassembly anchors in `.init.text`:

| offset | call relocation |
|---:|---|
| `0x1cbc` | `framebuffer_alloc`, r0=0x3c8 |
| `0x24a8` | `_ker_osd_fb_SetPar` (conditional) |
| `0x24c4` | `register_framebuffer` |

Thus the previous blanket statement that fb devices only exist because Aurora
creates them is incorrect. Registration is implemented in driver init. This
does not establish exactly when that function completes in a particular boot,
when `/dev` nodes are populated, or that an enabled visible OSD follows from
registration. Keep those questions separate.

`root:/lib/modules/install_fpga.sh` supplies sizes 1920x1080, 1920x1080,
1280x360, 256x256, 32x32; buffer counts 3,2,2,2,2; separate-buffer values
0,2,0,0,0; order 4,0; fast-logo plane 0. The meaning of separate-buffer value
2 has not been fully traced here.

## SetPar configures more than memory

`_ker_osd_fb_SetPar`: ELF `.text+0x227328`, Ghidra `0x237550`.
After checking init state and taking the per-plane semaphore, it invokes:

1. `pfAlphaBlendEnable`, with enable=1;
2. `pfSetAlpha`, with alpha=255;
3. pixel-format conversion and `pfSetColorMode`;
4. palette-cache allocation and `pfSetStride`, using a 32-byte-aligned stride;
5. default source/destination rectangles when their sizes are absent, followed
   by `pfSetDispWin`.

It does not by itself prove presentation. The previous report traces the pan
handler's physical-buffer bind and update callback. A later userspace mode change
can reach SetPar too; merely mapping the framebuffer does not reproduce these
configuration steps. No new FBIOPUT_VSCREENINFO or other ioctl has been added.

## Physical binding includes frame order and source origin

`_ker_osd_Set_BufPhyAddrSize`: ELF `.text+0x23bda4`, Ghidra `0x24bfcc`.
It validates plane/region, obtains stride and colour-byte width from cached
state, and reads the source origin. It derives the selected frame's address:
cached buffer-order values 3..5 add one `yres * stride`; values 6..8 add two.
It then adds the source-origin offset (with a different Y calculation for the
flip case), stores the effective address, and writes the OSD address register.
Normal planes encode the address in 32-byte units while retaining the upper
address/control bits. Cursor planes have a separate upper-bit path.

The second word of the callback's input/output pair is overwritten with the
effective physical address on this path. Do not assume every similarly named
userspace/kernel structure has identical field meanings. The exact buffer-order
enum mapping and relation to double-sized userspace surface declarations remain
open; this function alone does not settle allocation size or ownership.

## Callback and order follow-up

`_ker_osd_Get_DriverCallback` (ELF `.text+0x24386c`, Ghidra `0x253a94`)
returns the static `gstDriverCallbackList`; it does not construct a userspace
dispatch table. The ELF `.data+0x76054` relocation table resolves these slots:

| byte offset | target |
|---:|---|
| `0x08` | `_ker_osd_Open` |
| `0x10` | `_ker_osd_Set_FbInfoPointer` |
| `0x14` | `_ker_osd_PreInitSetting` |
| `0x30` | `_ker_osd_Update_Enable` |
| `0x40` | `_ker_osd_PlaneShow_Enable` |

`_ker_osd_Set_FbInfoPointer` (ELF `.text+0x23f154`, Ghidra `0x24f37c`)
validates plane 0..4 and stores the supplied pointer in `gpstHwFbInfo[plane]`.
This ties the hardware callbacks to the `fb_info` allocated during kernel init;
it does not require Aurora to pass a userspace object into the kernel.

`_ker_osd_ExchangeBufOrder` (ELF `.text+0x2261d8`, Ghidra `0x236400`)
derives selected frame 0/1/2 from yoffset 0/yres/2*yres on the pan-handler path.
It updates the cached order according to its previous group. Static branch
inspection gives the following mapping for the normal 0..9 input domain:

| selected frame | previous 0..2 | previous 3..5 | previous 6..8 | previous 9 |
|---:|---:|---:|---:|---:|
| 0 | 2 | 0 | 1 | 1 |
| 1 | 3 | 5 | 4 | 3 |
| 2 | 6 | 7 | 8 | 7 |

These enum values are stateful; they are not just frame numbers. The physical
binding function consumes groups 0..2/3..5/6..8 as base/one-frame/two-frame
offsets, consistent with the recovered mapping. This strengthens the reason to
use the existing fbdev pan handler rather than duplicate its private order
logic in openaurora. The table has not been exercised on the TV.

The driver-init separate-buffer branch treats a nonzero per-plane parameter as
enabling an alternate MAU entry in `gast_osd_dmabuf`, with a second base and
per-plane size. It emits an OOM diagnostic when the alternate region is smaller
than the required plane buffer. This establishes an alternate memory path, not
the complete semantics of parameter value 2. Do not equate that value with a
simple frame count or with a second userspace GEM allocation.

Additional raw logs: `oa-osd-callback-order.txt`, `oa-osd-callback-pointer.txt`,
`oa-osd-data-relocations.txt` in the same analysis directory.

## Next evidence gate

The callback-table/pointer question is resolved above. Later MAU selection is
traced in OSD-MAU-SELECTION; private GEM pool/backing relationships are traced
in OSD-BUFFER-CONTRACT. Alpha/PM follow-up is below. Full zero-buffer behavior,
resize and cold lifecycle acceptance remain open. Independent scanout acceptance remains
open, and the TV remains in its existing working state.

Raw logs: `artifacts/analysis/osd-2026-10-31/oa-osd-kernel-bind.txt`,
`oa-osd-kernel-bind-instructions.txt`, and `oa-osd-driver-init.txt`.

## Alpha callbacks are not interchangeable

New decompilation and ARM instructions distinguish the normal OSD path from
cursor controls. `_ker_osd_AlphaBlend_Enable` (ELF .text+0x2363d4, Ghidra
0x2465fc) validates plane0..4. For normal planes0..2, only region0 is accepted
and the valid path returns without a register write (instructions
0x24663c..0x246664). Its blend-bit writes apply to cursor planes3/4, regions0/1.
Calling that function on fb1 is not a missing normal-plane alpha initialization
operation. SetPar's call is real, but its hardware effect differs by plane.

`_ker_osd_Set_Alpha` (ELF .text+0x23530c, Ghidra0x245534) is separate. For
normal planes/region0 it masks the supplied alpha to8 bits and writes bits16..23
of the plane's control register, retaining other bits. SetPar supplies255 and
records that value in fb-private region state. Cursor alpha has its own layout.
This is global alpha; it does not replace the pixel-format/per-pixel alpha path.

The DRM property route also differs: Aurora Window::SetAlpha (0x940514) calls
the OSD driver's Set_Alpha (0x943f58), which requests property `gfxp_alpha`.
ntkndpdrm's osdp_sys_set_alpha forwards to ndp_osd_plane_set_alpha
(ELF .text+0x48c4c); that recovered function caches values at debug-state
+0x48/+0x4c and obtains fbinfo, but contains no fb ioctl/register write of its
own. Do not assume a property setter name proves immediate hardware effect.
Its cached values also affect a conditional GPU-filter branch in mode_set;
that is not generalized into complete opacity semantics here.

## Kernel power-management registration and restore

Driver init calls `_ker_osd_PMInit`. It resides in ELF **.text.unlikely+0x3a4**
(Ghidra0x68a42c), not .text or .init.text. Its call to
`KER_PMI_Install(0x30,0xe,...,10,0,&gstOsdDev)` supplies `_ker_osd_Suspend`
and `_ker_osd_Resume`. Relocations independently identify those function
pointers. The static gstDriverCallbackList entries .data+0x76054/+0x76058
resolve to `_ker_osd_Set_Suspend` / `_ker_osd_Set_Resume`.

The suspend callback (ELF .text+0x2416e8) marks all five fb-private states
suspended (+0x3c4), takes per-plane semaphores with bounded interrupt retries,
waits for pending activity, freezes AXI, hides display groups and uploads state.
It conditionally gates clocks when KER_PMI_Mode()==4. PanDisplay checks that
suspended flag and refuses work while it is set.

The resume callback (ELF .text+0x243080) restores clocks for that mode, replays
cached register-table values while handling upload/palette registers separately,
restores palette data for C8 planes, restores AXI-freeze state, shows groups,
triggers uploads and waits for completion. Finally it releases the semaphores
and clears the suspended flags. This is a kernel restoration path; opening a
new userspace fd is not what performs it. The wrapper's zero return also does
not prove that every restoration effect succeeded.

ntkndpdrm's `ndp_osd_restore_plane_mode_set` (ELF .text+0x484e4) is another,
narrower helper: it checks cached fb/rectangle state for **logical plane0 only**
and replays that plane's mode_set. Do not call it as an all-plane resume API.

## Consequence for the existing framebuffer backend

There is now a coherent firmware path to validate before inventing a second
backend: kernel registration + conditional SetPar for non-fast-logo planes,
existing framebuffer mapping/region state, and PanDisplay for buffer binding,
region show and commit. PreInitSetting initializes its first-pan recovery flag
at fb-private+0x298 to1; PanDisplay can show a hidden plane on that branch,
subject to force-disable state. It also enables a hidden region. These are
already kernel operations reached by the corrected present().

Keep fbdev as the initial candidate for independent fb1 validation. Do not add
generic alpha-enable, direct MMIO, PM replay or private-GEM calls merely to
imitate function names. mod_display currently reopens/remaps on resume, while
kernel PM restores device state; integration must still verify ordering,
preserved geometry/backing and the first post-wake frame. Full native power
orchestration, cold scanout, zero-buffer/MAU edge cases and later resize are not
closed by these static findings. The TV remains in use; no new device calls
or takeover tests were run.

Raw evidence in artifacts/analysis/osd-2026-10-31:
oa-osd-alpha-resume.txt, oa-osd-drm-alpha-restore.txt,
oa-osd-pm-registration.txt, oa-osd-pm-relocations.txt, oa-osd-window-alpha.txt.
Earlier SetPar/PreInit/PanDisplay logs supply the connecting calls and flags.
