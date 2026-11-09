# Artifact map and provenance

Local-only firmware research for this TV. Do not redistribute firmware, extracted
libraries, device databases or private keys. Documentation review on 2026-10-31
did not change the TV or any firmware/recovery/binary artifacts.

## Find the right source

The preserved November1 ABI3 production stage is openaurora/build-tv-v3:
core08f549d4026cfb626c1ffc93b2e02fc6,16 modules, broker/supervisor and stage.md5.
It is not the latest whole source build: current development uses separately
named ABI3 subdirectories; MEMORY owns their verified state. Matching stage preflight
and audio_mixer query/reload tests are recorded in MIXER-MODULE-2026-11-01.
The previous ABI3 stage is preserved in archive/build-tv-v3-before-mixer-20261101.tar
with its SHA256. Installed/recovery ABI1 and historical ABI2 were not changed.
analysis/cec-ownership-2026-11-01/mixer-module-tested-tv.tar preserves the exact
smaller runtime test set; mixer-matching-tested-tv.md5 records the full preflight.

| location | meaning / limitations |
|---|---|
| dumps/tv_nand.bin | this TV's 512 MiB NAND capture; the firmware source of truth |
| dumps/apdat_files | historical /conf extraction/snapshot; mutable settings and hooks are not current TV state |
| profiles | hardware/software/language profiles exported from this set |
| protocol | keymap XML/CSV, profile copy and historical screenshots/probe captures |
| privacy | dated settingsDB variants; filenames do not prove current live values |
| ui-assets | extracted theme PNGs and _layout manifests/indexes/string inventory |
| manuals | hardware/service references, not proof of runtime behavior |
| analysis/osd-2026-10-31 | raw decompile/instruction/relocation logs and a read-only MAU observation |
| analysis/runtime-check-2026-10-31 | loader failures/successes, qemu and real-TV preflight evidence |
| analysis/module-reload-2026-10-31 | host/sanitizer/ARM-qemu lifecycle, replacement-failure and safety/registry checks; newer build not TV-tested |
| analysis/module-phases-2026-10-31 | dependency-order reproduction/regressions, callback error propagation, host/sanitizer/ARM-qemu evidence; no TV run |
| analysis/control-replies-2026-10-31 | status heap-overflow reproduction, bounded reply/full-table Unix-socket tests, strict host/sanitizer/ARM-qemu evidence; no TV run |
| analysis/control-transport-2026-10-31 | nonblocking queues/backpressure, partial-write/disconnect/input/cleanup tests, strict host/sanitizer/ARM-qemu evidence; no TV run |
| analysis/power-2026-10-31 | standby/device/wake-network/monitor/UART and target ntkethmac PHY policy/PM contracts; static evidence only |
| analysis/sleep-cycle-2026-10-31 | internal host final-entry/transition-ledger/guard/cancellation regression tests; no hardware integration |
| analysis/power-service-2026-10-31 | coordinated service/CLI, pinned notifications, failure/cleanup tests with wrapped power I/O; no physical standby or TV call |
| analysis/ui-animation-2026-10-31 | animation controller/effects/theme-format RE, 39-descriptor TSV and matching host/sanitizer/ARM reader tests; no native UI effect runtime |
| analysis/ui-resource-bindings-2026-10-31 | 702 CRC-verified resource names/IDs, named/annotated animations and widget/category state bindings; ad styles remain excluded |
| analysis/ui-clock-2026-10-31 | FROST clock instructions and seven host/sanitizer/ARM-qemu DSO groups; reloadable ui_clock implemented, animation controller pending |
| analysis/cec-audio-2026-10-31 | current audio ABI/CEC transport instructions, wire/fake-HAL regression tests and durable-path verification |
| analysis/cec-transport-2026-10-31 | private V4L2/kernel transport, packet marshalling tests, driver filter and close/reopen evidence; no TV run |
| analysis/cec-ownership-2026-11-01 | PreInit registration/emulation, raw-backend injected-I/O checks, audio init/getter evidence; later shared-owner Steam Deck queries, cleanup and spec regressions, interpreted in CEC-LIVE |
| workbench/ghidra_preinit | new PreInit / PreInit.ko analysed project, source MD5 1e50bee8b7888315c7a070f8e69947f6; separate project manifest in analysis/cec-ownership, created after migration snapshot |
| workbench/ghidra_hdmiswitch | new HdmiSwitch / ntkndphdmiswitch.ko analysed project, source MD5 d4ff737dcbd9be2b628f2dd4eccf5d7a; separate project manifest in analysis/cec-ownership |
| workbench/ghidra_alsa | new Alsa / ntkndpalsa.ko project, MD5 8758a13d12799217ce702849df7d54da matching live; separate manifest and AUDIO-GAIN-TRANSPORT evidence in analysis/cec-ownership |
| analysis/cec-ownership-2026-11-01/live-audio-routing.bin | 612-byte read-only AOUT cache snapshot from verified live Aurora; decoded with openaurora/tools/audio/audio-state.c; AUDIO-ROUTING records provenance/limits and AD82587_D selection |
| analysis/cec-ownership-2026-11-01/live-volume-table.bin | 202-byte read-only runtime volume table; all101 entries match the new AQ decoder, also tested on TV; AUDIO-VOLUME records source hashes and limits |
| workbench | durable firmware extracts, analysed Ghidra projects, toolchains, Python tools, specifications and historical research moved from /tmp/opencode; README maps scope; SHA256SUMS records migration |
| reports/ghidra | early raw analysis; inferred signatures are not verified declarations |
| reports | dated interpretations; use [the index](reports/README.md) before following advice |
| payload | loose root hook, keeper, hosts/keys/tools and several image generations |
| recovery-stability | tested current /conf-only USB image/script pair; RECOVERY.txt has physical steps |
| scripts | extraction, image construction and destructive restore tools; see scripts/README.md |
| archive | pre-cleanup documentation only; not active instructions or a task list |

## Verified identities

These MD5 values identify existing historical artifacts, not a security signature.
Recovery/payload hashes below were re-read locally during documentation cleanup.
The NAND identity is from its stored manifest and prior evidence; it was not
re-dumped from the TV. The originals remain untouched.

| file (relative to artifacts/) | role | MD5 |
|---|---|---|
| dumps/tv_nand.bin | pristine source capture, 536870912 bytes | 36de1801478ce38c8042845374add41d |
| recovery-stability/apdat0_full.ubifs | tested current recovery, 7872512 bytes | 994da7ff76a0b2fbafb8f6fa0c682627 |
| recovery-stability/kylo_usb_update.scr | tested exact script, 584 bytes | de1591298d633cf6422a1ade4e8835a9 |
| payload/apdat0_full.ubifs | byte-identical current image copy | 994da7ff76a0b2fbafb8f6fa0c682627 |
| payload/apdat0_orig.ubifs | pristine /conf revert; removes rooting | fde74f196b11740de528735e49bcc976 |
| payload/apdat0_mtd13.img | older raw 10 MiB partition, predates maintenance | 875804842f4bb6fd71dd2c6097080edd |
| payload/aurora.elf | shell boot hook, not vendor ARM UI | 0b1cd32a9de2b0251f6252e6603db8b7 |
| payload/keepalive.sh | access/privacy/network keeper | 4ebd0f9d7ee9f470afde8b0f2807522b |
| payload/authorized_keys | accepted public keys | 0d11cf71a7e3054cb1359d62f9033cee |

The pre-stability UBIFS, compressed raw image and full-NAND chunks are not
substitutes for the tested pair. make_stick.sh selects scripts/apply_apdat0_hook.scr,
not the exact recovery-stability script. Do not silently regenerate a working
stick with a different active script. The failed 549-byte script's cause of
failure is unestablished; use the known working identity without inventing a
parser explanation. Reconfirm the actual USB device/mount before any future copy.

## Firmware paths and extraction

From the project root:

```sh
artifacts/scripts/extract_from_dump.sh vendor:/com.nvt.idtv/aurora.elf -o /tmp/tvh-firmware
artifacts/scripts/extract_from_dump.sh vendor:/lib/libNDAL3.so -o /tmp/tvh-firmware
artifacts/scripts/extract_from_dump.sh vendor2:/dynamic_ui -o /tmp/tvh-firmware
```

The helper uses artifacts/workbench/rootfs/{root,vendor,vendor2} as caches. It handles
root(fs0), vendor(ap0), vendor2(ap1), not /conf(apdat0). Profiles already exist
in profiles and protocol. If another original /conf file is needed, carve the
0xA00000-byte partition at NAND offset 0xE340000 using dd and extract with
`sudo .venv/bin/ubireader_extract_files -k` (see
[docs/reproduce/root-payload.md](../docs/reproduce/root-payload.md)); `-k`
preserves the 101:101 idtv ownership. That is local extraction, not a
reason to wake the TV. Preserve analysed Ghidra projects: regenerating analysis
is avoidable work even though their source firmware remains in the NAND dump.

| extracted source | recorded MD5 |
|---|---|
| vendor:/com.nvt.idtv/aurora.elf | 64c252340d3af6c5d66072611b7bad7a |
| vendor:/lib/libNDAL3.so | c842bad804302ec822076c3d9d3ce31d |
| root:/lib/modules/ntkndpdrm.ko | a70aca9b814e51cb9680476ce27051de |
| vendor2:/modules/ntkdriver_nondovi.ko | fded7005651e37b8a1b5e8e89f73f4ff |
| protocol/mb230_hwprofile.bin | 1b063b2cc7faa8b92c0f843921e561c7 |

The old Medion firmware and extracts were deleted; references in the archive
are sibling research, not missing dependencies to download again.

Both kernel slots were subsequently re-extracted from this NAND and compared
to reconstructed ELF LOAD segments before reading generic GEM code. Raw ker0
MD5 9b91aa0ad7339e7cdfdb3b67c5b50f81; ker1
95615c3e9e80230e0be658d82d2a2b23. Offsets, ELF hashes, comparison lengths and
size-store instructions are in OSD-BUFFER-CONTRACT. Existing /tmp kernel names
alone must not establish target provenance.

## Images, keymaps and strings

protocol/shot-launcher.png is the corrected BGRA-decoded reference.
shot-launcher.as-captured.png is the pre-correction image; do not sample its
red/blue palette. Other dated captures are historical until their decoder
provenance is checked. A framebuffer screenshot is not physical scanout proof.

protocol/ir-keymap-2026-10-30.csv maps ICE IDs/KeyString to profile RC codes;
it does not settle the active UART encoding. ICE, DIAL, NEC and DFB codes differ.
ui-assets/_layout/lang-strings.txt is a flattened 56715-line inventory, not
language-indexed records. Keep the original profile for ID/grouping recovery.

For raw analysis, read the corresponding current report first. Ghidra warnings,
saved void signatures and external relocation placeholders must not be treated
as proven machine contracts. No raw output was rewritten by the cleanup.
