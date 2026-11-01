# Kernel OSD init and physical binding — offline follow-up

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

The callback-table/pointer question is now resolved above. Continue tracing the
separate-buffer selection path, including the zero-base or zero-size cases. Reconcile those
with the userspace GEM allocator and the frame-order exchange logic. Only then
choose any additional cold-start setup. Independent scanout acceptance remains
open, and the TV remains in its existing working state.

Raw logs: `artifacts/analysis/osd-2026-10-31/oa-osd-kernel-bind.txt`,
`oa-osd-kernel-bind-instructions.txt`, and `oa-osd-driver-init.txt`.
