# Agent handoff — 2026-10-31

**Latest continuation:** pool construction/bitmap allocation and front/back
requests are now traced. Initial front requests S; back requests S or 2*S
(count3), or shares front (count1). Sync advertises 2*align4096(S), but the
OSD kernel commit binds address and size from its GEM pointer, not from those
advertised dimensions. Size helper reads GEM +0x70. _drm_gem_init passes aligned
size to generic drm_gem_object_init; its final +0x70 store remains to inspect.
Next confirm that generic initializer and complete the OSD format/stride/region/
alpha/enable/update contract before choosing a backend. Later resize/switching
and actual cold scanout remain unverified. Report, TODO and seven raw logs
updated; no TV access or code changes. This supersedes the next step below.

**Post-handoff update:** offline private GEM tracing resolved +0xa9 provenance:
main/sub create flags set the descriptor byte copied there to 1. Requested size
is page-aligned and reaches the OSD-plane pool AllocMem call. Full backing
extent/doubled-size relationship is still open. Next inspect
`_create_osd_memory_pool` and `ndp_drm_gem_mem_AllocMem`, then MCU allocation-table
size against Sync_FrontSurface2HW. New raw logs:
`oa-osd-private-gem-{create,allocation,plane}.txt`; buffer-contract report updated.
No TV access, new code changes, or new test runs in this continuation.

Read AGENTS.md, TODO.md and MEMORY.md before continuing. This file summarizes
the latest state; older chronological entries may describe superseded builds
or failed experiments. No TV access was performed while preparing this handoff.

## Objective and owner constraints

Build a bug-for-bug compatible replacement for vendor Aurora, modular and
reloadable through IPC/socket/CLI, eventually supporting a native OmarchyTV UI.
Parity includes the other TV features, not only a launcher. Telemetry,
advertising, ACR and tracking are explicitly excluded, including optional modules.
The objective remains incomplete.

- Firmware is the source of truth. Decompile/disassemble before implementing
  hardware APIs; on-set queries confirm understood behavior, not discover APIs.
- TV is in use. View-only interrogation is always authorized, as now recorded in
  AGENTS.md. No drawing, keys, hardware takeover, service/network changes,
  persistent deployment or reboot. Use tools/tvssh.sh, per owner preference.
- Temporary /tmp staging for read-only checks/queries was authorized; verify
  file size/hash in a separate connection before execution and remove afterward.
- Only target 192.168.178.87. Use apply_patch to edit; no Python editing.
- Keep -Werror -pedantic in CFLAGS. No subagents unless explicitly requested.

## TV state — last verified, not freshly queried

Recovered successfully using physical USB restore. Vendor UI running; SSH,
root telnet, network and privacy protections verified. Recovery details and
the previous upload failure are in MEMORY and STABILITY-2026-10-31.md.

- Boot ID ebb0aaaa-72a2-4410-9f8d-95b9249f8a90; vendor PIDs 1899 and 1737.
- /conf/oa_safe and /conf/coredumpnoreset present; /conf/oa_boot absent.
- Installed core remains ABI 1, MD5 a898384df076572cf377749f25427689.
- Recovery UBIFS MD5 994da7ff76a0b2fbafb8f6fa0c682627; raw mtd13 is older.
- Hook MD5 0b1cd32a9de2b0251f6252e6603db8b7; keeper
  4ebd0f9d7ee9f470afde8b0f2807522b. Keepalive repairs access/privacy/network.
- Never combine stdin-streamed upload and asynchronous work in an AND-list.
  A previous trailing & caused an empty hook and physical recovery. Verify
  installed files separately before any later action.
- No staged helpers or runtime test processes were left on the TV.

## Completed implementation and validation

All new ARM work is isolated in openaurora/build-tv-v2. Preserve build-tv,
installed ABI 1 and recovery artifacts. Rebuild core and modules together when
ABI 2 is eventually deployed; deployment is not currently authorized.

1. ABI 2 descriptor-size checks, module-owned thread posts, owner/token lifetime
   handling, queued callback cancellation, failed-init registration cleanup,
   held-provider stop protection. Host strict suite, ASan/UBSan safety tests and
   ARM/qemu checks passed. See MODULE-SAFETY-2026-10-31.md.
2. Runtime --check runs before host initialization, checks ELF/ABI note/entry and
   invokes PT_INTERP in loader trace mode. No module constructors/init/start,
   HAL calls or host socket. It catches unresolved symbols even when the loader
   exits zero. All eleven production modules emit OA_MODULE_EXPORT metadata.
   Host negative cases and ARM/qemu passed. Actual TV staged core + 11 modules
   passed all twelve checks, exit 0, then staging removed; boot/PIDs unchanged.
   Current tested core MD5 424ef6ea213cb706acb51a7adb39f778.
   See openaurora/docs/RUNTIME-CHECK.md and RUNTIME-CHECK-2026-10-31.md.
   Older installed audio/panel/source have unresolved v4l2 symbols; rebuilt
   closure is correct. Do not silently replace installed modules.
3. display.present now always submits FBIOPAN_DISPLAY, even display.pan=0 or
   one frame; errors propagate and preserve retry frame. Prior default omitted
   the commit. Fake-device host/ASan/ARM tests passed. Not tested drawing on TV;
   independent cold scanout remains unproven.
4. PWM header corrected from firmware: Open consumes six words, Set four;
   Init/Open/Set/Terminate return scalars despite saved void metadata. Close
   takes no channel. Maintained vendor_pwm.h replaces stale generated version.
   See PWM-CONTRACT-RECHECK-2026-10-31.md.
5. fbregion opens O_RDONLY and uses only four GETs (var/fix/MAU/region).
   Host/ARM tests assert this. Real-TV query returned primary MAU=0, fb1
   1920x1080 virtual1920x2160, offset0,1080, PA35bbb000, length16588800,
   stride7680, region {0,0,1920,1080,0,0,3840,2160}. Helper removed; boot/PIDs
   unchanged. Does not explain earlier invisible-output attempts.

## Current research and exact next step

Latest focus: vendor OSD allocation, GPU flush and scanout ownership. Read
OSD-BUFFER-CONTRACT-2026-10-31.md, OSD-KERNEL-INIT-2026-10-31.md,
OSD-MAU-SELECTION-2026-10-31.md and DRM-PLANE-CONTRACT-2026-10-31.md.
Raw evidence is in artifacts/analysis/osd-2026-10-31/.

Critical corrections to old notes:

- fbdev 131/132 are palette set/get, not acquire/release.
- Kernel KER_OSD_DrvInit registers five framebuffers. Registration, /dev node
  publication and visible presentation are different questions.
- CREATE_DUMB allocates only 4096 bytes regardless requested dimensions.
  Old large mmap failures do not rule out the actual vendor DRM path.
- Actual Aurora uses private GEM create 0xc0106440, MAP_DUMB+mmap64, GEM flink
  table, ADDFB2 and SETPLANE. Generic NDAL init only discovers IDs.
- Separate OSD cache records are 68 bytes, not generic NDAL's 48 bytes.
- Pan binds physical buffers and calls pfUpdateEnable; kernel already handles
  stateful buffer order and MAU selection. Do not guess private enums.

Just finished offline, now saved in the buffer-contract report and four raw logs:

- VsyncFlush 0x93bc04 calls Copy 0x93b720; wrapper adds no wait/flip/allocation.
  Copy builds 84-byte GA request, semaphore deadline10s, ioctl0xc0546480.
  Driver completion semantics remain unexamined.
- GetFlushCopyMode 0x9408c4 actually returns cached byte +0x38; saved Ghidra
  void prototype is wrong (ldrb r0 at0x9408e8).
- ntkndpdrm ndp_drm_fb_create classifies GEM objects with
  ndp_drm_gem_IsFrameBufferMode_device: magic at +0xb0, flag byte +0xa9.
  All special objects use __ndp_drm_fb_create, ordinary use cma_create_4k,
  mixed kinds fail EINVAL. Recovered callbacks fill framebuffer metadata and
  retain refs; no complete backing-size guarantee established from them.

NEXT: trace private GEM creation's initialization of +0xa9 and actual allocation
extent, then reconcile Sync_FrontSurface2HW's twice-page-aligned surface size
with the allocation-table entry. Two surface objects do not prove a contiguous
double-sized GEM object. Also inspect generic DRM validation as needed. Do this
offline; no allocation/drawing probe is justified yet.

Remaining broader work: cold display ownership/enable/alpha/resume parity,
IR framing/release behavior, complete native UI/hardware parity. Reload still
ignores restore failure, starts formerly stopped modules, and has undefined
suspended-state semantics; source-first OSD work took priority. TODO checkmarks
were audited (72 originally, 14 reopened); don't call the replacement complete.

## Tools and resumption pointers

- Ghidra /opt/ghidra/support/analyzeHeadless; use -readOnly -noanalysis.
  Durable script openaurora/tools/re/DumpFirmware.java accepts exact symbols or
  @address and --asm. Do not run simultaneous calls on the same project.
- Projects under /tmp/opencode: ghidra_proj/Aurora (aurora.elf),
  ghidra_ndal3/ndal3 (libNDAL3.so), ghidra_ntk/ntk (ntkndpdrm.ko),
  ghidra_nondovi/nondovi (ntkdriver_nondovi.ko), ghidra_tdga/Tdga.
- Firmware extracts /tmp/opencode/rootfs/{root,vendor,vendor2}; extract_from_dump.sh
  avoids waking the set. Aurora symbols need nm -D. Kernel .text Ghidra address
  is ELF+0x10228; .init.text has a different base. Check relocation evidence.
- ARM compiler /tmp/opencode/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc.
  qemu-arm-static available. TV libc2.23; new artifacts max GLIBC2.17.
- ARM --check child loader exec needs a read-only bwrap namespace with TV /lib,
  /etc/ld.so.cache and /vendor/lib; qemu -L alone doesn't remap child execve.
- Current directories are not Git repositories; do not assume git diff/status.
- No tool sessions are left running. Handoff changes only docs and saved RE
  output; no new code change or test run was needed for handoff preparation.
