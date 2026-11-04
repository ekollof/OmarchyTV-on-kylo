# Framebuffer restoration review — 2026-11-04

Offline review of FB-SNAPSHOT-COLD-BUNDLE. No TV connection or framebuffer
access occurred; the owner's current test permission excludes active fb1.

## Reproduced defects and fixes

1. Restore accepted a changed `smem_start` if capacity and MAU matched. It also
   accepted larger allocations and did not compare stride, geometry or colour
   interpretation. Saved pixels could therefore be written to a different
   backing or interpreted under a different mode while reporting success.
   `common/fb_snapshot.c` now returns `-ESTALE` before mapping/writing on
   changed backing, exact capacity, driver identity/type/visual, stride, pixel
   layout or display timing/mode. Offsets may differ because pan restores them.
   The guard does not issue a mode setter or change the allocation/MAU.
2. A failed writable mmap skipped region/pan restoration, contrary to the
   original report's best-effort cleanup claim. Restore now retains that first
   error and attempts both understood region and pan operations. Existing
   sync/unmap/region/pan errors also retain the first error. The library never
   consumes the snapshot, so its caller can retry after failure.
3. `fb-restore save` ignored `fclose` failure and could announce a successful
   backup after buffered data failed to flush. A `/dev/full` regression
   reproduced this; failed close now returns failure and skips the success
   message. This does not add fsync/crash durability or atomic file replacement.
4. The tool test includes its source directly but lacked that source as a make
   prerequisite. The rule now rebuilds it after tool changes without compiling
   the included translation unit twice.

The before-fix assertions are retained in before.txt and before-flush.txt.
The source correction is in `fb_snapshot.c`, its header, `fb-restore.c`, the
Makefile and their focused tests. No new driver API was inferred or probed.

## Firmware basis and verification

OSD-KERNEL-INIT records the shipped nondovi kernel driver MD5
`fded7005651e37b8a1b5e8e89f73f4ff`: SetPar at237550 changes format/stride and
window state as well as memory interpretation. OSD-BUFFER-CONTRACT records
that FBIOPAN_DISPLAY commits the selected frame, not a complete mode rollback.
The guards follow from those existing instruction-level findings; rejection of
a changed mode is an explicit project safety policy, not a vendor parity claim.

`check-fb-snapshot check-display` passes strict host, leak-enabled ASan/UBSan
and fresh ARM/qemu builds in build-tv-v3/fb-restore-review. Tests execute the
library, the included tool, and both actual display module variants with all
device calls wrapped. Twelve changed-backing/layout cases verify zero new
maps/pixel/region/pan writes; mapping/sync failures verify remaining cleanup
and retry; the tool uses real temporary files and `/dev/full`. The display
tests continue to prove only their fake-driver model. Logs and ARM executable
hashes: `artifacts/analysis/fb-restore-review-2026-11-04/`.
Full strict host `make BUILD=build-fb-restore-review check`:372 ok, exit0
(full-host.txt). No new change to the native45-group UI/audio reference bodies
was made, so those instruction comparisons were not rerun for this policy fix.

## Corrected limits and continuation

"Full snapshot/restore" was too broad: var/fix are saved metadata, not modes
that the helper restores. The implemented operation restores backing bytes,
region and pan **only under unchanged backing/layout/mode/MAU**. It cannot
restore an allocation or change MAU. The checked TODO milestone is narrowed
to that offline behavior; native full ownership/restoration remains unchecked.

These GET checks cannot exclude another owner racing them or detect a recycled
allocation at the same physical address. The caller must hold exclusive device
authority through the whole transaction. No real-driver equality is proved.

At9dbb04f, `mod_display` logged restoration failure and freed its snapshot while
closing/suspending, so the library's retry capability did not provide a module
recovery protocol at that milestone. [DISPLAY-CLEANUP](DISPLAY-CLEANUP-2026-11-04.md)
subsequently adds retained failure state and an owning stop/unload worker with
nonblocking join; synchronous suspend now reports failure/retains its snapshot.
[DISPLAY-DSO](DISPLAY-DSO-2026-11-04.md) subsequently covers the controlled
actual-host/loader cases. Native restoration acceptance remains open. Do
not put blocking restore operations in an ABI3 quiescence hook. File-format
portability across host/ARM ABI and untrusted-file validation also remain open;
FBR2 currently stores native fb structs. No live restore/cold startup is tested
or authorized by the non-fb1 test window.
