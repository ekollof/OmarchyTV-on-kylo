# Immutable graphics IPC and owner lifetime — 2026-11-01

Status: lower protocol/transport/server implemented and tested with injected
rendering callbacks on host, ASan/UBSan, ARM/qemu and the actual TV. This is a
step toward the asynchronous GPU display provider. It does not run EGL, draw,
publish a module service or establish display/animation acceptance. The existing
native GPU diagnostic remains the separate EGL-WINDOW milestone.

## Contract

`include/openaurora/scene_broker.h` and `common/scene_broker*.c` define private
OGS1 local AF_UNIX/SOCK_SEQPACKET records, exactly64 bytes little-endian. These
are our IPC, not a recovered vendor ABI. OPEN validates the currently supported
1920x1080 profile. FRAME carries a nonzero image revision, type0..8, finite
progress0..1 and an upload flag. CLOSE drains ownership. All reserved bytes must
be zero; tickets strictly increase. Invalid records never reach the renderer.

Uploads use one SCM_RIGHTS descriptor for an8294400-byte top-down RGBA image.
`oa_scene_image_create` copies into a CLOEXEC anonymous memfd and applies WRITE,
GROW, SHRINK and SEAL seals. No unsealed fallback exists. The receiver requires
all seals and exact size, maps read-only, and closes the received descriptor.
The parent can close its copy immediately after a successful send. Cached frames
carry no descriptor and must name the current uploaded revision; subsequent
uploads must increase the revision. No pointers cross the socket.

The blocking child calls injected open/render/finish/close callbacks. A FRAME
reply follows successful render and finish; enqueueing is not a committed frame.
Even this completion does not certify physical scanout. Failed render/finish
retains the mapped image until owner cleanup succeeds. Partial open and failed
close preserve the owner. Explicit CLOSE can be retried; disconnect/protocol
failure drains off-loop, retrying close every100ms without a forced ownership
drop. A permanent close failure therefore intentionally prevents return. Backend
code and callback table remain borrowed until serve returns. No hardware call
is replayed after EINTR. The receiver may retry an interrupted socket receive.

Packet send/receive offer MSG_DONTWAIT for the future parent. Rejected records
close received descriptors, including extra descriptors; successful received
descriptors are CLOEXEC. The ancillary buffer accommodates Linux's253-descriptor
maximum. The production parent still needs its bounded outstanding slot,
coalescing, completion watches, WNOHANG reap and ABI3 nonblocking quiescence.

## Actual target-kernel correction

The first TV run passed sealed-file/descriptor cleanup tests, then its server
returned EPERM before the injected renderer ran. The initial mapping was
PROT_READ/MAP_SHARED. The test's wait for renderer entry timed out. A diagnostic
retry confirmed server errno1. Neither run accessed graphics hardware; stock
operation remained intact. Test assertions now exit normally, and a timeout
handler uses _exit124, avoiding the vendor core handler on expectation failures.

Offline target `ker0_sym.elf` instructions resolve this:

- `sys_memfd_create`80158fbc and `shmem_add_seals`80154790 are present.
- `mmap_region`801743a4 tests VM_SHARED(bit8), entering8017464c.
- 80174654..80174694 checks the mapping's writable count. A negative count
  produces `mvn r7,#0` (EPERM), even for this read-only shared mapping.
- `shmem_mmap`801545ac itself returns0 after installing its vm_ops.

The receiver now uses PROT_READ/MAP_PRIVATE. The sealed file still supplies
immutable backing pages; private mapping does not eagerly copy the frame.
The unchanged WRITE/resize/seal requirements remain enforced. All native session
and lifetime tests then passed. Do not weaken seals or infer that memfd is
unsupported from the earlier EPERM. Instruction evidence is
`analysis/scene-broker-2026-11-01/kernel-mmap-region.txt`.

## Emulator observation

With the initial ancillary buffer for8 descriptors, ARM/qemu rejected a packet
containing16 descriptors and exposed8 for our code to close. The fd count still
increased by8. The qemu syscall trace records those8 closes and the increased
getdents result. The same case passed its no-leak assertion on the TV and on the
native host. This is an observed emulator-specific truncation discrepancy; its
upstream implementation was not audited. Accommodating the complete legal
descriptor record avoids that path and lets the complete ARM/qemu suite pass.
Do not label this a target-kernel descriptor leak. The initial trace is retained.

## Evidence and limits

`make check-scene-broker` covers four groups:

1. Endian/float protocol, bounds and atomic malformed-record rejection.
2. Seals, write/truncate rejection, CLOEXEC, nonblocking receive, oversized/short
   packets and extra-descriptor cleanup with fd counts.
3. Upload/cache, caller descriptor release, changed caller memory after upload,
   and no completion while the injected finish callback is blocked.
4. Partial open, interrupted render without replay, failed finish, retained image
   through two failed closes, explicit CLOSE retries, EOF drain, stale tickets,
   absent cache, nonregular/unsealed image rejection and wrong socket type.

Strict C11/-Wall/-Wextra/-Werror/-pedantic, ASan/UBSan and ARM/qemu pass.
The complete host `make check` passes207 checks (previous203 plus these4 groups).
Actual-TV binary: **87760bytes**, MD5**ba89e0afce422f03e8254ae1b0c307cc**,
maxGLIBC2.4, preserved in `openaurora/build-tv-v3/scene-broker/tests/scene_broker`.
Upload size/hash verified in a separate connection before execution; native
exit0. All raw logs are in `artifacts/analysis/scene-broker-2026-11-01/`.

Staging `/tmp/oa-scene-ipc-20261101` was removed; no test process remains. Same
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live Aurora1785 and zombie1967.
Full16588800-byte framebuffer hash41735daa69dc137743b59efe12db4a19 and pan0,0
unchanged. Hook/hosts hashes, SSH22/telnet2323, Samba REJECT and safety markers
remain. No persistent deployment or graphics/audio/CEC hardware operation.
The matching16-module production stage was not rebuilt.

## Next integration requirement

There is no graphics executable or module service yet. A discarded draft linked
Mali and closed all fd>=3 in main. That ordering is unsafe: library constructors
run first and may own descriptors. Do not copy the audio executable's fd cleanup
blindly into a directly linked graphics executable. Establish a bootstrap/load
boundary (or a complete verified inherited-fd policy) before loading Mali; retain
and drain the library/renderer together until cleanup succeeds. Redirect logging
before exec. The future GL finish implementation also needs real GPU validation.

Then wire a separate owning helper and asynchronous display service, preserving
source/provider holds, final frames under backpressure, first-frame readiness,
failed cleanup and reload/suspend/resume lifetime. Exercise the integrated path
on the actual display with complete framebuffer restoration. The unchanged CPU
display remains the production implementation; no display.renderer option exists.
