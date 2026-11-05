# Review of subsequent agent work — 2026-11-05

Scope:8ebacd2..40f2f66, all production changes, new harnesses and13 added TODO
checks, plus the revised JPEG/complete-decoder entries. Firmware and hardware
evidence remain distinct from host fixtures. No broad acceptance gate closed.

## Fixed gaps

1. `core/control.c`'s new image-decode command hashed returned dimensions without
   enforcing the service's8192px axes/64MiB output limits. A replacement provider
   could overflow the ARM32 length or cause an excessive/out-of-bounds hash walk.
   It now returns EOVERFLOW before hashing oversized results. control_reply
   injects8193x1,1x8193,8192x2049,INT_MAX squared, zero-width and valid1x1 replies;
   every result releases its buffer while the service remains held, then drops
   the hold. The axis fixture has enough backing for the previous hash loop,
   so its regression is a return-value failure, not an intentional invalid read.
   This does not make arbitrary dishonest providers memory-safe.
2. The vendor decoder harness compared shipped classes with Pillow/host
   TurboJPEG. It never executed our DSO on generated variants/progressive/CMYK
   or foxxum files. `tests/image_decode --decode` now loads the actual module,
   returns dimensions/status/raw RGBA and performs release/stop/unload. The
   maintained differential runs this ARM binary under qemu against every same
   vendor input. All26 inputs pass:20 PNG successes,5 JPEG successes and one
   CMYK rejection. External oracles remain additional checks. Nonzero rejection
   codes need not equal vendor enum values; empty output invariants are checked.
3. `--extra-jpeg` silently skipped missing explicit inputs. It now fails. The
   two available files were only in /tmp/opencode; copied unchanged into the
   private ignored workbench/decoder-corpus and included by default when present.
   corpus-sha256.txt records their identities. The other two remain unavailable.
4. Fresh RectHandler/HorizontalMaskScaleTransform/SVGGrid decompilation and ARM
   inspection confirm transform-before-storage and the zero-mask self-transform
   result. The prior evidence's right-edge subtraction was reversed; corrected
   at its source. The algebraic test now includes the inner surface shrink by
   side-mask widths. It is explicitly a model, not an executed native transform
   differential or proof of visible grid parity. A separate fresh getButtonPosition
   trace at10ca8ac found a pre-existing implementation mismatch in the reviewed
   helper: the right edge adds
   the left inset twice overall. oa_vgrid_button and its two nonzero-inset
   expectations now match that instruction sequence. Zero-inset portal geometry
   is unchanged. This comparison is static ARM evidence plus C regression,
   not execution of the native getter. Conditional compiled nav0x80 senders do not prove a working
   stock remote-key entry on this model; documentation now says that explicitly.

## TODO dispositions

| checked milestone | result / acceptance boundary |
|---|---|
| image.decode socket lifecycle | retained; actual host/socket/stdin and PNG/JPEG/error/reload checks pass; synchronous decoding remains explicit |
| ui_vendor recorder lifecycle | retained; actual modules/control plane, recorder only |
| non-HAL UI stack lifecycle | retained; copied DB and real service chain, no cold hardware claim |
| fake-fbdev lifecycle | retained; real display_fb with userspace fake and restored pixels/region/pan/MAU; not native driver acceptance |
| apps-grid compiled-path audit | retained with narrowed wording; static paths and transform evidence, stock key path/capture still open |
| corrected-capture comparator | retained; metrics/BGR/regions and historical sample regression pass; no new stock capture |
| plat-consumer lifecycle | retained; fake plat with disabled hardware paths, not HAL bring-up |
| graphics/scene_display lifecycle | retained; actual broker/child/service chain with fixture renderer, not Mali |
| fixture-backed modules lifecycle | retained; actual providers over IPC with synthetic/private inputs, not hardware ownership |
| hardware-free module matrix | retained; actual reload ordering/nav/list/power/CEC idle paths, not native CEC traffic |
| skin socket lifecycle | retained; configured fallback/select/error paths, recorder only |
| vendor decoder differential | retained after direct production comparison added; prior external-oracle gap fixed |
| stock-capture analysis | retained as dated analysis; old artwork/states are not a current apps-grid oracle |

JPEG entry's stale claims of no offline foxxum/no differential were corrected.
Complete decoder acceptance remains unchecked: missing files, YCCK/APNG policy,
worker scheduling/cancellation/frame lifetime and stock appearance remain open.
Earlier comprehensive182/183-item audits remain their historical scope; this
review does not claim a new source-by-source revalidation of every old check.

## Verification

Durable logs: artifacts/analysis/agent-review-2026-11-05.

* Fresh strict host `BUILD=build/agent-review-2026-11-05 make check` passes,
  including all new socket tests, comparator and grid checks. No count from an
  older build is substituted for this run. C11/Wall/Wextra/Werror/pedantic retained.
* Focused control_reply, image_decode and vendor_grid pass leak-enabled ASan/UBSan.
* Fresh ARM `build-tv-v3/agent-review-2026-11-05` control_reply passes qemu;
  vendor_grid also passes qemu; check-vendor-decoder passes direct production/
  vendor comparison on all26 files.
* Fresh grid-firmware.txt and grid-button-firmware.txt preserve decompilation/instructions from
  Aurora MD5 64c252340d3af6c5d66072611b7bad7a at ef6128/eecda8/10c9ed0/10ca8ac.
  Existing JPEG fast-DCT recovery is confirmed by direct byte equality rather
  than changing decoder policy again.

On TV, only our decoder test and DSO ran, with staged ordinary image files.
No vendor HAL/UI was initialized; no device, framebuffer, audio/CEC ownership,
key or setting operation was requested. Owner now authorizes fb1 too, but it
was not used. tvssh sent WoL after its initial reachability failure.

Upload and verification were separate successful connections. Verified manifest
SHA256 ce42813a464ade9b34229a6af08bb362ba191c515ad339fecff4430d4450f4d4,
15 payload files totaling659563 bytes; sizes and every payload hash matched.
TV library unit passes9 corpus PNGs,2 DMR JPEGs, truncation/missing/non-image,
reload and suspend/resume/reuse checks. Two further foxxum raw results exactly
match ARM/qemu, including result headers:

- 1685:a74cd871912032d59ffc5a7e2f9418df4b5c0b288180476a57be86ec14b2154a
- 3753:b62d700d366a0f8ed082c6cb72532c060fc68585ad29869bd973984dc3681877

Staging removed, no helper in /proc scan. Before/after boot ID
71f7eb63-727f-4b9a-a9c7-2b04dc3f9197 and vendor pair1967(zombie)/1785(live)
remain; /conf/coredumpnoreset retained. No persistent deployment change or
display-restoration claim is implied by a CPU-only run.

## Continuation

Use the maintained direct comparison instead of rerunning external-oracle-only
claims. Full decoder/UI acceptance and native amp/CEC authority remain open;
AMP-VIDEO-OWNER still owns the next libv4l2/board-lifetime question. Ordinary
display callbacks and decoder commands remain synchronous where documented;
these tests do not supply production worker scheduling or cold takeover.
