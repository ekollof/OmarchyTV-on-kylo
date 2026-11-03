# LauncherHelper::getImageId — real 62-way mapping (2026-11-03)

`common/launcher_image_id.c` now implements the shipped
`LauncherHelper::getImageId(NovoLauncherOperationId op, WIDGET_STATE state)`
exactly, replacing the synthetic `(op<<8)|state` probe that had stood in for
it. **469** native/C comparisons pass: the shipped 792-byte body at
`0xe4aaf4` (CRC `0xa4caa578`) is copied verbatim into a fixed mapping and
called for operations 0..63, 70, 100 and `UINT32_MAX` across states
0,1,2,3,4,100 and `UINT32_MAX`. Host units, focused ASan/UBSan, the full
ARM/qemu group (886060 render comparisons unchanged) and a focused real-TV
CPU run all pass.

## Recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF.

The function is a `tbh` switch on `op` with 62 entries; `op > 61` returns
`-1`. Every in-range block compares `state` with 1:

- `state == 1` selects the alternate id,
- **every other state value** (0, 2, 3, 4, 100, `UINT32_MAX`) selects the
  default id.

Operations 30, 31, 36 and 37 share the `-1` default block. Operations 42
(167), 56 (847) and 57 (848) ignore the state entirely. Operations
20–23 and 43–44 share the pair 781/782; 32 and 50 share 799/800; 46 and 47
share 763/764. Operation 52 is asymmetric: state 1 gives 825 but every other
state gives 790 (it reuses op 28's default 790). Operations 38, 39 and 61
invert the usual direction (state 1 takes the lower id).

The exact 62 pairs are transcribed as `{state==1, state!=1}` in
`launcher_image_id.c` and were generated directly from the table targets, not
guessed.

## Verification method

`tests/launcher_image_id_native.h` maps the shipped body at `0xe4aaf4`
(page `0xe4a000`) and the stack-guard slot at `0x1791d68` (page `0x1791000`,
canary written before the page is made executable). It calls the function
through a Thumb function pointer (`0xe4aaf5`) and compares each result with
`oa_launcher_get_image_id`. A table transcription error, a wrong jump-table
target, a wrong state comparison or a wrong default would all show up as a
mismatch with the op/state printed.

`tests/launcher_image_id.c` adds structural host checks (shared blocks,
`-1` defaults, state-independent entries and the "only state 1 differs"
invariant). `tests/settings_row_render.c` then overrides the render ops
`glyph_id` hook with the real function and asserts op 2 produces
733/734/734/734 for states 1/0/2/3, so the production hook is proven to
accept the real mapping.

One harness bug was found and fixed while building this: the guard literal is
`0x01791d68` (a 7-digit address), so the backing page is `0x1791000`, and the
canary must be written before `mprotect` makes the page read-only. An earlier
attempt used `0x1791d000`/offset `0x1d68`, which faulted inside the body.

## Verification and limits

| executive | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 224760 | c389c67af6b879bcf931ef9269951408 |
| tests/tab_policy_firmware | 1170852 | 380939250f9bac3df2084c267908ab44 |

Host `check-launcher-image-id` and `check-settings-row-render`, focused
ASan/UBSan, ARM/qemu units, the full `verify.cjs` (now 34 ok lines) and full
`make check` pass. The focused real-TV CPU test ran the self-contained
`settings_row_render` binary: exit 0, staging hash-verified and removed, no
helper left, no-reset flag retained. No device/screen/input/settings/network/
service/boot change.

`getImageId` is a pure function, so this closes it completely. The adjacent
`LauncherHelper::convertOperationIdToIDM` (0xe4ae0c) is a separate switch and
remains open. Still open: `setSelectionBoxImageId`/`SVGImage` internals (the
next slice), row construction, child visibility internals, live resources,
scrollbar internals, skin/module/display/scanout/startup and independent
visible parity.
