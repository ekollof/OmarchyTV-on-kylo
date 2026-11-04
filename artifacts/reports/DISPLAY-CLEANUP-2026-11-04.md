# Display cleanup retention and firmware comparison — 2026-11-04

Both `display` and `display_fb` now retain failed snapshot restoration instead
of reporting successful suspend or freeing the only backup. Stop/unload uses
an owning cleanup thread and a nonblocking ABI3 quiescence gate. No TV access
or framebuffer device was used in this work; all device calls in tests are
wrapped. Current owner permission excludes active fb1.

## Firmware comparison and its limits

The initial draft used the existing OSD-KERNEL-INIT/OSD-BUFFER-CONTRACT evidence
for region, mapping and pan. After the owner's reminder, direct read-only
decompilation and ARM inspection of the teardown/suspend functions was added
before finalization. This comparison does **not** establish vendor lifecycle
parity: the snapshot/retry layer supports openaurora's temporary binding and
module reload, and is not a recovered vendor snapshot algorithm.

Fresh evidence is under `artifacts/analysis/display-cleanup-2026-11-04/firmware/`.
All existing Ghidra projects were opened read-only/noanalysis, separately.

| Shipped function | Instructions / observed behavior | Consequence |
|---|---|---|
| TDGA `driver_close_device` at Ghidra12350 | `bx lr` | No device restoration in this function. |
| TDGA `NVTGA_OSD_RemoveRegion` at12344 | `movs r0,0; bx lr` | Region removal itself does not restore the previous image/region. |
| TDGA `NVTGA_DestroyPool` at12830 | Three iterations, two munmap calls per iteration; results ignored | Pool teardown releases mappings; success does not prove previous pixels were restored. |
| TDGA `driver_close_driver` at147a0 | Calls driver+0c, leaves/destroys pool, closes three fds, conditionally frees two TDGA buffers; close results ignored | The body supplies no framebuffer snapshot/retry protocol; do not reproduce it as an exact rollback guarantee. |
| `NDALex_GPU_Close` at4db5b2a4 in libNDAL_EX | Mutex, refcount decrement; at last user, closes sequence/queue/semaphore, unmaps GPU state, closes Linux fd, clears context | This is shared GPU resource teardown, not a generic layer-state rollback. Lower GPU command effects were not independently executed here. |
| nondovi `_ker_osd_fb_Mmap` at236d34 | Uses fb physical base plus page offset, bounds mapping by backing capacity, calls remap_pfn_range | Userspace mapping aliases the layer backing; unmapping alone is not a copy/rollback of its contents. |
| nondovi `_ker_osd_fb_PanDisplay` at237d4c | Checks private+3c4 before acquiring its semaphore; nonzero returns fffffff2 (-EFAULT) | Restore must retain failure if kernel PM has already suspended OSD. New test uses that exact error. |
| nondovi `_ker_osd_Suspend`/`_ker_osd_Resume` at2400c8/24006c | Invoke callback0/+4 if present; wrapper returns0 irrespective of callback result | This wrapper is not an acceptance check. Kernel register restoration remains the separate PMI path described in OSD-KERNEL-INIT. |

TDGA Ghidra addresses include its10000 image-base adjustment; raw ELF symbols
are2350/2344/2830/47a0. Sources: TDGA MD5
`a770a7e5e7d33c2857e773a91c6c2654`, nondovi
`fded7005651e37b8a1b5e8e89f73f4ff`, libNDAL_EX
`e6a465861d1869b8b138b3f299c89f87`.

The indirect driver+0c call is resolved from driver_init_driver's stores at
15a14/15a1a: raw PC5a02+literal14dd6 gives GOT1a7d8; plus18c gives1a964,
whose R_ARM_GLOB_DAT names NDALex_GPU_Close. This export belongs to libNDAL_EX,
not libNDAL3. The latter project's lack of that function is not API absence.
The callback/relocation evidence and libNDAL_EX objdump are preserved. No
unresolved external relocation was treated as a decoded call target.

## Implemented module policy

- On failed synchronous suspend, return the restore error and retain snapshot,
  mapping and fd. The host has no asynchronous suspend transaction protocol;
  the existing suspend path remains synchronous. Retry can restore the same
  original snapshot. This is not kernel PMI suspend/resume or complete power
  parity.
- Stop/unload quiesce marks the display closing and starts a cleanup thread.
  The worker restores pixels/region/pan, frees the snapshot only after success,
  then unmaps and closes. It never invokes the host API. While it owns these
  fields, all display drawing/present/info calls reject and start/resume/suspend
  refuse to race it.
- The gate only creates a worker or tries `pthread_tryjoin_np`; it performs no
  ioctl/msync/munmap/close. Actual join, including TLS termination, is required
  before reading the worker result or allowing stop/destroy. Creation/join
  failures retain ownership; a blocked driver cannot make the gate wait.
- Restore/unmap failure retains unfinished resources for retry. A250ms
  monotonic retry interval is explicit project policy, not a vendor constant.
  Permanent failure keeps unload/shutdown pending rather than declaring safe
  release. Linux final-close errors consume the descriptor; never replay it.
- Start-publication failure retains resources for the host's existing failed
  start gate. The direct caller scene-probe now honors quiesce before stop/
  destroy. All affected link rules use-pthread; no module ABI change.

## Verification and remaining work

Focused strict host, leak-enabled ASan/UBSan and fresh ARM/qemu tests pass for
both actual display variants. They exercise failed sync suspend and retry/
resume, exact firmware suspended-pan EFAULT, a deliberately blocked worker
ioctl with continued gate calls, creation/join failure retention, drawing and
restart rejection, failed worker restoration, failed unmap retry and consumed
close errors. Wrapped calls prohibit real devices; driver return policies are
controlled fixtures, not independent hardware acceptance. ARM module builds
and version requirements are recorded; executable/module SHA256s are in the
analysis directory. Existing target tryjoin availability/execution evidence is
in AMP-OWNER, not a new live display test here.
Full strict host check on the final source:382 ok, exit0; scene-probe also
builds successfully. Full log is full-host.txt in that evidence directory.

Next: actual host/DSO reload/failure integration for this display gate,
exclusive layer authority, real-driver state equality, complete native
power/standby/resume and fallback. Ordinary start, drawing/present and suspend
still issue synchronous operations; this worker only moves stop/unload cleanup
off the loop. No cold bundle was deployed or executed. The full Aurora goal
and native ownership/restoration gates remain open.
