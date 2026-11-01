# OSD allocation and presentation: firmware evidence, 2026-10-31

> **Status: Current scoped firmware evidence, updated through GEM backing/pool tracing. Cold scanout/resume remains unverified; final section owns remaining questions.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Offline decompilation and ARM disassembly of this TV's firmware. No TV access,
ioctls, drawing, deployments, keys or reboots were performed. This report
supersedes the palette/buffer-handshake and "DRM is not scanout" conclusions in
`OSD-INIT-RE-2026-10-30.md`. It extends `DRM-PLANE-CONTRACT-2026-10-31.md`.

## Inputs and reproduction

| firmware file | MD5 |
|---|---|
| `vendor:/com.nvt.idtv/aurora.elf` | `64c252340d3af6c5d66072611b7bad7a` |
| `vendor:/lib/libNDAL3.so` | `c842bad804302ec822076c3d9d3ce31d` |
| `root:/lib/modules/ntkndpdrm.ko` | `a70aca9b814e51cb9680476ce27051de` |
| `vendor2:/modules/ntkdriver_nondovi.ko` | `fded7005651e37b8a1b5e8e89f73f4ff` |
| `vendor:/run.sh` | `8451ebc21555e9cd5b175cd4cb1931c1` |

Full local decompilation logs are saved under `artifacts/analysis/osd-2026-10-31/`.
The projects are `/tmp/opencode/ghidra_proj Aurora` (aurora),
`ghidra_ndal3 ndal3`, `ghidra_tdga Tdga`, `ghidra_ntk ntk` (DRM module), and
the newly imported `ghidra_nondovi nondovi` (large driver). The latter emitted
warnings about unresolved external kernel relocations. Conclusions below use
local code, callback diagnostic names and objdump cross-checks, not those
unresolved external addresses. Module Ghidra addresses differ from ELF section
offsets by `0x10228`; aurora/NDAL addresses below are virtual addresses.

`openaurora/tools/re/DumpFirmware.java` now provides a reproducible project-local
script for exact-symbol/address decompilation and optional instruction dumps:

```sh
/opt/ghidra/support/analyzeHeadless /tmp/opencode/ghidra_proj Aurora \
  -process aurora.elf -readOnly -noanalysis \
  -scriptPath "$PWD/openaurora/tools/re" \
  -postScript DumpFirmware.java --asm @0093d48c \
  _Z37plat_Gfx_DRM_osd_drv_Calculate_Stride9_EN_COLORj
```

The script was run successfully on both queries. It prints the executable MD5
and labels signatures as analysis metadata: a Ghidra `void` prototype is not
proof that a function returns no value. Check register use and return paths.

## Ioctls 131/132 are palette operations

The actual implementation is `_ker_osd_fb_Ioctl_wrap` in
`ntkdriver_nondovi.ko`, ELF `.text+0x22868c` (Ghidra `0x2388b4`).

- Command 131 (`0x400cc883`) copies `{start, count, pointer}`, checks the range
  fits 256 entries, allocates `count * 4` bytes, copies palette entries and calls
  `gstDriverCallback->pfSetPaletteData` (callback slot `0x8d`).
- Command 132 (`0x800cc884`) uses the same structure, calls `pfGetPaletteData`
  (slot `0x8e`), then copies `count * 4` bytes to the supplied pointer.
- `NVTGA_OSD_UpdateRegion` (`0x132f4` in the tdga library's Ghidra project) and
  `FlipRegion` only use these around palette handling. They compare palette
  entries and set them when different. They do not acquire frame buffers.

The earlier proposed "acquire with 132, draw, return with 131" experiment is
invalid and must not be performed. Frame-buffer ownership cannot be inferred
from either command. See `oa-osd-nondovi-fb.txt` and `oa-osd-tdga-recheck.txt`.

## FBIOPAN_DISPLAY is also the frame commit

Both tdga UpdateRegion and FlipRegion finish valid presentation with ioctl
`0x4606`, including when no palette needs changing. The kernel implementation
`_ker_osd_fb_PanDisplay` (ELF `.text+0x227b24`, Ghidra `0x237d4c`) checks OSD init
and suspend state, takes a semaphore, manages current-buffer order, obtains
physical base/size from `fb_info` (or the selected alternate MAU buffer), calls
`pfSetBufPhyAddrSize`, and calls `pfUpdateEnable`. It also has conditional plane
and region show logic. Thus panning is a hardware commit, not merely choosing
which pixels the next raw write touches. It does not need palette ioctls first.

Openaurora previously defaulted to `display.pan=0` with region setup enabled.
That skipped the entire ioctl; present returned success after msync. It also
skipped it with one frame, and swallowed pan failures. The offline source now
always commits through FBIOPAN_DISPLAY; `display.pan` controls buffer rotation
only. Failed sync/commit returns failure and retains the frame for retry. Resume
selects the current or next frame according to the same rotation policy.

`tests/display_present.c` runs the actual module with wrapped fbdev operations:
current-frame commits (including resume), one-frame commits, rotation, failure
propagation and retry. The three groups pass with strict CFLAGS on host, under
ASan/UBSan, and as ARM under qemu against the TV libc. Full host checks pass too.
This proves the userspace call contract, **not visible output or cold startup**.
No changed binary or configuration has been installed on the TV.

## Why the old dumb-buffer experiment cannot settle DRM viability

In `ntkndpdrm.ko`, `_ndp_drm_gem_cma_dumb_create_4k` (ELF `.text+0x12090`,
Ghidra `0x222b8`) computes pitch and a dimension-based reported size, but then:

```text
0x12108  mov r0, r5
0x1210c  mov r1, #4096
0x12110  bl drm_gem_cma_create       [R_ARM_CALL relocation]
```

It allocates **4096 bytes**, regardless of the requested dimensions. A returned
handle does not establish that the dimension-sized backing store exists. This
is consistent with the old large mmap failures; the exact live failure branch
was not traced and is not claimed proven. CRTC `mode_valid=0` and SET_MASTER
failure likewise do not erase the explicit vendor ADDFB2/SETPLANE path.

## Aurora's actual OSD allocation/binding path

`plat_Gfx_Center::CreateWindow` (`0x942690`) computes a surface stride as
`align32(width * ((colour & 0xff) >> 3))` via `Calculate_Stride` (`0x945264`).
Its dedicated-mode table at `0x13be6d8` is `{1,2,3,4,5}`. With the ordinary
non-UMP argument it calls `Alloc_DedicatePlane` (`0x93d6e8`) then
`_MCU_Alloc_Dedicate` (`0x93d48c`):

1. Private create command 0 through `Hal_Video_Libdrm_drmCommandWriteRead`, with
   16 bytes `{size_low, size_high=0, flags, handle=0}`. The NDAL wrapper at
   `0x4e7c8230` packs ioctl **`0xc0106440`**. The kernel handler returns a GEM
   handle and overwrites the flags word with the physical address. These two
   meanings must not be confused.
2. Map the returned handle using **`0xc01064b3`** (MAP_DUMB) and `mmap64` with
   the returned 64-bit offset (`drm_client_map`, `0x93c0f8`). This shares a mapping
   operation with dumb buffers, but allocation is through the private command.
3. Obtain a GEM flink name (`0xc008640a`, helper `0x93d3ec`) and record the
   handle/PID/name/VA/PA/size in the MCU allocation table. `Phy2Handle` (`0x93cde8`)
   resolves that table, using GEM_OPEN (`0xc010640b`) for another process when
   needed; it is not a physical-address-to-handle arithmetic conversion.
4. `Sync_FrontSurface2HW` sets colour, stride, physical address/size and display
   rectangles. `Set_BufPhyAddrSize` (`0x94457c`) resolves the GEM handle and calls
   ADDFB2; on success it removes the old fb ID and caches the new fb/base/size.
5. `Update_Enable` (`0x944860`) commits dirty geometry/buffer state through
   SETPLANE. Source coordinates are shifted left 16. Its unchanged-state branch
   uses the private on/off ioctl below.

| logical window | driver slot | `plane_type` property | dedicated create flags |
|---|---:|---:|---:|
| main | 2 | 0 | `0x40000` |
| sub | 1 | 1 | `0x80000` |
| txt | 0 | 2 | `0x100000` |
| cursor 1 | 3 | 3 | `0x200000` |
| cursor 2 | 4 | 4 | `0x200000` |

The slot permutation is from `ConnectWindowToOSD_HW` (`0x93f590`) followed by
OSD driver's `Init` (`0x943d8c`), not a guess from enumeration order. Kernel
allocation may fall back to a general pool; no live allocation was tested.

## Separate OSD state and enable payload

OSD driver state has a 12-byte header and five **68-byte** plane records
(`ShareMem_Create` at `0x9436bc`, SysV key `ftok("/dev/ntksys",100)`, size
`0x160`). It is distinct from the generic NDAL nine-record, 48-byte cache.

| record offset | proven use |
|---:|---|
| `0x00`, `0x04`, `0x08` | plane, CRTC and framebuffer IDs passed to SETPLANE |
| `0x0c..0x18` | source x/y/width/height, integer units before SETPLANE shift |
| `0x1c..0x28` | destination x/y/width/height |
| `0x2c`, `0x30` | stride and vendor colour enum |
| `0x38`, `0x39` | cached enable byte and dirty byte |
| `0x3c`, `0x40` | physical base and buffer size |

Unlisted bytes have not been assigned meanings here. `Plane_Enable`
(`0x9440a0`) sends **`0xc00c6455`** with `{plane_id, 0, enable}`. Kernel ioctl
table `.data+0x3e4` maps that command to `ndp_drm_crtc_set_onoff_ioctl` (ELF
`.text+0x50a8`), which resolves the plane ID and treats selector 0 as sync-on/off
plus order update. It is not a blanket "dirty flag" command. Aurora sets cached
enable only when the ioctl succeeds. `Show_Enable` is a stub in this firmware.

Colour mapping in ADDFB2 is `0x808 -> C8`, `0x10444410 -> AR12`, otherwise AR24.
`run.sh` sets plane sizes/counts to main 1920x1080x3, sub 1920x1080x2, txt
1280x360x2, cursors 256x256x2 and 32x32x2. These startup values are distinct
from the live virtual fb sizes observed later.

## Surface observations (follow-up resolutions below)

Further offline surface tracing: `plat_Gfx_Surface::SetResolution` at 0x9412c0
checks required stride*height against the existing surface byte capacity; it
updates dimensions/stride without allocating another buffer. `_Gfx_flush_2D`
at 0x939de4 passes separate back/front physical-address descriptors to GPU
Copy or VsyncFlush. `SetShowFbNumb` at 0x940598 marks the front surface dirty;
`Set_ForceBufOrder` at 0x944c48 caches the requested order at OSD record +0x34.
Later sections trace the doubled-size declaration and GEM binding separately.
Two surface objects do not imply one contiguous double-sized GEM object. Raw logs:
`oa-osd-surface-sizing.txt` and `oa-osd-surface-flush.txt` in the analysis directory.

The missing default fbdev commit is fixed offline, not yet validated on screen.
Kernel framebuffer registration is now traced in OSD-KERNEL-INIT; allocation
and binding are traced below. Complete cold enable/alpha/geometry state, resume
and ownership acceptance remain unverified. Follow the final section's remaining
questions rather than repeating already resolved callback/allocation searches.

## GPU flush and framebuffer creation follow-up

Offline decompilation and instructions confirm `VsyncFlush` (`0x93bc04`) calls
`Copy` (`0x93b720`); its wrapper adds no wait, page flip or allocation. Copy builds
an 84-byte GA request and submits `0xc0546480` (SDP_GA_EXEC), protected by a
semaphore with a ten-second realtime deadline. It does not submit SETPLANE.
This describes the wrapper, not unexamined GPU-driver completion semantics.

`GetFlushCopyMode` (`0x9408c4`) is not a no-op: the saved Ghidra void return type
is wrong. Instructions load a cached byte from window shared state at +0x38
into r0 (`0x9408e8`). Treat saved function signatures as metadata until checked.

In ntkndpdrm, `ndp_drm_fb_create` (ELF .text+0xd168) looks up GEM objects and
classifies them with `ndp_drm_gem_IsFrameBufferMode_device` (+0x1453c). All
framebuffer-mode objects go to `__ndp_drm_fb_create` (+0xccfc); all ordinary
objects use `_ndp_drm_fb_cma_create_4k` (+0xcefc); mixed kinds return EINVAL.
The predicate checks object magic at +0xb0 then returns byte +0xa9. The recovered
creation callbacks retain GEM references and fill framebuffer metadata. They
do not themselves establish that the advertised dimensions fit the backing
allocation; generic DRM core validation has not been fully examined.

The private flag and pool-allocation questions raised here are resolved in
the following sections. Cold setup remains unverified. Evidence:
`oa-osd-vsync-flush.txt`, `oa-osd-vsync-instructions.txt`,
`oa-osd-fb-gem-bounds.txt`, and `oa-osd-gem-classification.txt` in
`artifacts/analysis/osd-2026-10-31/`.

## Private GEM flag and plane-pool allocation

Follow-up recovered `ndp_drm_gem_create_ioctl` (ELF .text+0x18c24),
`_drm_gem_create` (+0x18948), `_drm_alloc_buf` (+0x14a88), and
`_OSDPlane_buffer_allocate.constprop.10` (+0x10808). Instructions confirm:

- Main/sub dedicated flags 0x40000/0x80000 select allocation kind 2 and their
  respective plane-selection byte. The create request's descriptor byte 13
  is set to 1, byte 14 to 0. `_drm_gem_create` copies descriptor bytes 12..15
  into object +0xa8..+0xab, so the previously observed framebuffer-mode +0xa9
  byte is 1 for these requests. This is not inferred from an object name.
- A zero requested size fails. The low size word is rounded by
  `(size + 4095) & ~4095` before `_drm_gem_init` and `_drm_init_buf`.
  This path is distinct from the fixed 4096-byte CREATE_DUMB allocator.
- `_drm_alloc_buf` dispatches kind 2 to the OSD-plane allocator. It creates
  OSD memory pools, chooses a pool using the descriptor's plane bytes, and
  passes buffer record +0x1c (the stored size) to `ndp_drm_gem_mem_AllocMem`.
  It stores the returned physical address at buffer +8 and fails with ENOMEM
  when no address is obtained. It also allocates a per-page bookkeeping array.

These findings establish the flag's provenance and size propagation into pool
allocation. The pool/MCU size trace and doubled metadata relationship are
covered below, including the confirmed generic GEM size-field initialization. Do not
substitute the current pool request for proof that every advertised framebuffer
row is backed. Evidence: `oa-osd-private-gem-{create,allocation,plane}.txt` in
the same analysis directory. No TV access or code changes were needed.

## Pool extent and front/back allocation versus advertised size

`_create_osd_memory_pool` (ELF .text+0xe048) obtains contiguous reserved memory
through nvt_mem APIs. It subdivides that region using `osd_tbl` child `region`
properties (`osd`, `g`, `sg`, `cursor1`, `cursor2`) when present, otherwise uses
compiled default offsets/sizes. Do not assume the fallback layout is the live
device-tree layout. It initializes page-granularity pools through
`ndp_drm_gem_mem_InitMem(...,4096)`; any remainder may form a further pool.

`ndp_drm_gem_mem_AllocMem` (+0x1b884) rounds requested bytes to pool units,
searches free contiguous bitmap runs, records the allocated address/unit count,
marks exactly that run occupied and subtracts its bytes from free capacity.
The address is `pool_base + first_unit * unit_size`; failure returns zero.
There is no automatic doubling of this request in the recovered allocator.
`_drm_init_buf` stores the aligned request at buffer record +0x1c, confirming
the value subsequently passed to the allocator.

Aurora `CreateWindow` (`0x942690`) clarifies the userspace relationship. Let
`S = stride * surface_height`:

| object / configuration | requested bytes |
|---|---:|
| initial front surface | S |
| back surface, configured count 1 | shares front physical address |
| back surface, configured count 2 | S, separate allocation |
| back surface, configured count 3 | 2*S, separate allocation |

Count is read from `Default_OSD_PLANE_Size` through `0x93f1fc`; it is not
deduced from virtual fb height. The front request is the multiply at `0x942886`
and call at `0x942896`; the count-3 back multiplier is at `0x942a80`.
`_MCU_Alloc_Dedicate` records its original requested bytes in the allocation
table (`local_40 = param_3`), while the kernel page-aligns the actual pool request.

Separately, `Sync_FrontSurface2HW` (`0x940388`) unconditionally declares
`2 * align4096(front_surface_bytes)` when the front is dirty: instructions at
`0x9403d6..0x9403e6` load, align and shift that field before Set_BufPhyAddrSize.
Thus initial front allocation and advertised buffer extent are demonstrably
different. A contiguous pool does not make adjacent allocations part of the
same GEM object. This is firmware behavior to account for, not evidence that
userspace may map/write the doubled extent. The examined code does not prove
that the hardware reads that full extent or that generic DRM accepts it in all
states. Keep visible source geometry, advertised framebuffer geometry, GEM
extent and mmap extent distinct when implementing parity.

The next section traces how OSD commit consumes backing size separately from
metadata. Later surface switching/resizing remains unverified.
Raw decompilation and instructions: `oa-osd-gem-pool-sizing.txt`,
`oa-osd-window-extent.txt`, and `oa-osd-sync-extent.txt` in the analysis directory.

## Which size reaches the hardware binding

The actual OSD commit callback is `osdp_plane_mode_set` (ELF .text+0x4534c),
which forwards the prepared plane state to `ndp_osd_plane_mode_set` (+0x47a40).
That routine obtains the fb's GEM backing through `ndp_osddp_fb_buffer(fb,0)`
(+0xd774) and `ndp_osddp_fb_buffer_size(fb,0)` (+0xd944). Both index the GEM
pointer array at framebuffer +0x90; the address helper calls
`ndp_drm_gem_phyical_addr_device`, and the size helper reads GEM object +0x70
(instruction at Ghidra 0x1dbe0). It does not compute width*height*pitch from the
advertised framebuffer dimensions.

`ndp_osd_plane_mode_set` places the returned address and size into the payload
for `_ker_osd_drm_fb_Ioctl(...,0x400cc873,...)`. It also configures format,
stride, region enable, source/destination region, and eventually update command
0x4008c80a. Source crop and advertised framebuffer extent therefore travel
separately from the GEM-derived binding size. `_drm_gem_init` (+0x14620) passes
the page-aligned request to `drm_gem_object_init` and records it at its vendor
extension +0x94. The generic initializer's +0x70 size store is now confirmed in
both kernel slots from this TV's dump, as detailed below.

This resolves the earlier assumption that doubling the advertised surface size
must imply doubling its backing allocation. The examined vendor path deliberately
keeps metadata and backing distinct. It does not authorize writes outside the
actual mapping, prove cold setup, or establish generic DRM bounds validation.
Remaining initialization work should follow the complete format/stride/region/
enable/alpha/update sequence and resume behavior before adding a backend.
Raw evidence: `oa-osd-plane-backing-{use,commit}.txt`,
`oa-osd-fb-backing-helpers.txt`, `oa-osd-gem-object-init.txt`.

## Generic GEM size: confirmed from both target kernel slots

Both reconstructed kernel ELF load segments were compared byte-for-byte with
fresh decompression of this TV's NAND uImages. This avoids relying on the old
Medion/sibling-kernel notes or the filename of an existing /tmp cache.

| slot | NAND gzip offset / compressed bytes | raw MD5 | reconstructed ELF MD5 |
|---|---|---|---|
| ker0 | 0xd40040 / 0x4c8a38 | 9b91aa0ad7339e7cdfdb3b67c5b50f81 | 2a01437fbb085b3e75929dd681d38430 |
| ker1 | 0x1440040 / 0x84bdf0 | 95615c3e9e80230e0be658d82d2a2b23 | f9fb74333ac730500cd33aefcd40e4b6 |

In ker0, drm_gem_object_init at 0x8040f5a0 calls
drm_gem_private_object_init at 0x8040f530; instruction 0x8040f56c stores the
size argument r2 to object +0x70. In ker1, the corresponding functions are
0xc03aa3ec / 0xc03aa37c, and the size store is at 0xc03aa3c0. Thus the helper
used by OSD really returns the GEM allocation size, not doubled framebuffer
metadata. This is static evidence in both slots, not a claim about which slot
is currently booted or about physical scanout.

Reproduction: read the compressed size from each 64-byte uImage header, use GNU
dd with iflag=skip_bytes,count_bytes at the offsets above, pipe to gzip -dc,
then compare to the reconstructed ELF LOAD segment (file offset0x124, lengths
0xa69ef8 for ker0 / 0xdb0698 for ker1). Disassemble the functions with the ARM
toolchain objdump. Raw instructions are saved as oa-osd-kernel-gem-size.txt and
oa-osd-kernel1-gem-size.txt. No new kernel reconstruction or TV read was needed.

## Commit control and region-enable distinction

The existing ioctl-wrapper trace and new callback instructions establish:

| command low byte | argument behavior | recovered purpose |
|---|---|---|
| 0x63 | argument's low byte is a value; callback receives its address | pfRegionShowEnable, not alpha blending |
| 0x71 | argument value aligned up to32 | stride; do not pass a uint pointer just because encoding says IOW |
| 0x73 | pointer to12 bytes | physical buffer descriptor; binding uses GEM-derived address/size |
| 0x75 | argument's low byte is a value | vertical flip enable |
| 0x98 | pointer to uint32 | current-buffer-order input, translated through ExchangeBufOrder |
| 0x0a | pointer to8 bytes | update-control payload forwarded to pfUpdateEnable |

`_ker_osd_Update_Enable` (ELF .text+0x241c20, Ghidra0x251e48) reads payload
byte4 to choose its commit-register sequence. It tests byte5==1 for VSyncWait,
with an additional cached-state gate at fb-private +0x29a. The recovered OSD
caller supplies word0=0x0e, byte4=1 and a debug-policy byte5 (zero for cursors).
Do not collapse the last four bytes into an arbitrary uint32 boolean or assume
every commit waits for vsync. The role of word0 outside this callback is not
generalized here.

The OSD commit path's region-show command is separate from plane show and
alpha setup. `_ker_osd_PlaneShow_Enable` also honors u8OSD_ForceDisable; a show
request does not override that state. Earlier SetPar evidence initializes
alpha/blending, but complete cold alpha ownership and resume must still be
traced before adding setup calls. Evidence: oa-osd-update-controls.txt plus
the existing oa-osd-nondovi-fb.txt and oa-osd-plane-backing-commit.txt. No new
ioctl, allocation, drawing or takeover was run.
