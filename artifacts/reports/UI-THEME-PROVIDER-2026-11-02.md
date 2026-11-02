# Reloadable immutable BinSVG provider — 2026-11-02

common/svg_pack.c loads an owned, validated index/binary snapshot. mod_ui_theme
publishes it as ui.theme v1 through the normal module lifecycle. Consumers can
now resolve actual resources by name or index without keeping file handles or
coordinates copied from TSVs. This is data plumbing, not complete theme/style
selection or a rendering/UI-parity acceptance.

## Firmware and ownership

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. Fresh ARM/decompile for
DynamicUILoader::loadSVG ef4d48 and BinSVGContainer::setSVG ef9268 is preserved
in artifacts/analysis/ui-theme-provider-2026-11-02/firmware.txt. loadSVG reads
the count and hash/length index entries, then consecutive records in that order;
it checks the accumulated bytes against file size. DynamicSVGSupplier's name
hash and first-match scan were already recovered at ef57bc/ef58e4 in
analysis/ui-resource-bindings-2026-10-31/oa-svg-hash-and-widget.txt and
oa-svg-name-binding.txt. CRC32 includes case and suffix, excludes terminating
NUL. IDs are index positions; no runtime ELF or generated vendor name table is
needed for named lookup. Duplicate hashes preserve the firmware's first match.

The new loader reads regular files through its own nonblocking/CLOEXEC fds,
rejecting FIFOs/directories before reads. It checks numeric conversion, exact
total size, missing/extra entries, trailing junk/embedded NUL and every record's
bounded BinSVG structure before publishing anything. Output is unchanged on
failure. Bounds of65536 entries/4MiB index/64MiB binary are explicit project
resource limits, not claimed firmware limits. Index tokens allow whitespace
separation. The shipped702-entry/289764-byte pack is fully supported.

Snapshot bytes are copied, not mmap-backed; truncating/replacing/deleting the
input files cannot mutate an already published snapshot or cause a mapped-file
SIGBUS. No file I/O occurs in service calls. Startup/reload performs bounded
regular-file reads; this does not claim a hard storage-latency deadline or
transactional consistency across concurrent edits of the two input files.

## Service and reload contract

ui.theme v1 has record(id,out), named(name,out) and count(). The first two
return errno values and borrowed immutable oa_binsvg views; count returns0
when unavailable. Hold the service through lookup/release for the entire use of
all returned views AND strings. Drop those before releasing/stopping; copied
integer geometry may survive reload, borrowed metadata strings may not.
These operations are loop-thread only and do not touch devices or draw.

Configured paths are explicit ui_theme.index and ui_theme.binary. Missing or
bad input refuses start without publication. Publication collisions free the
candidate snapshot while preserving the peer; the same module can retry.
Ordinary standby retains the published immutable snapshot. A suspended
replacement defers first loading/publication until resume, with retry on failed
activation. Normal registry consumer holds prevent stop/unload/reload while
views are in use. No new force-unload bypass or lifetime guarantee is invented.

Reload rereads configured files; it does not reread host configuration. The
host still lacks old-binary rollback: after an unheld provider is removed, a
failed replacement leaves no provider. The new tests prove no stale service
escapes and explicit loading recovers. UI consumers still need to implement
their fallback/rebind behavior when they adopt this service.

examples/ui-theme.conf is a data-only local example, with TV path instructions.
Source Makefile now builds21 modules. The preserved16-module TV stage and
installed ABI1/recovery artifacts were not changed.

## Evidence and acceptance limits

tests/ui_theme.c uses real DSOs and host command dispatch. It covers snapshot
immutability through input truncation/replacement/deletion, CRC lookup/case,
duplicate-hash first match, atomic failed-load outputs, consumer holds,
publication collision/retry, stop/unload/reload, standby and deferred activation,
invalid paths/FIFOs/numbers/indexes/records and failure after old-provider unload.
Malformed later records reject the whole pack even if the first record is valid.

Optional corpus arguments start the actual provider on the full shipped pack,
iterate702 resources/11888 elements, and verify name SettingListBoxUFA.svg maps
to index477 and rowPos=(244,178,319,248). This is real provider/data acceptance,
not visible widget/rendering acceptance. No telemetry or ad functionality is
implemented by a generic resource parser/provider.

Strict full host suite, focused ASan/UBSan, ARM/qemu and native TV lifecycle plus
corpus tests all pass. Native artifacts in build-tv-v3/theme:

| artifact | bytes | MD5 | max GLIBC |
|---|---:|---|---|
| tests/ui_theme |508228|9975f90fcce4483dd9b2187d034b549e|2.17|
| modules/mod_ui_theme.so |47116|33609e021a52fab6c3e9b61b0459d6d4|2.4|
| modules/mod_ticker.so (test consumer) |21644|5b503cdece5cefd16f84942cbd8d38b1|2.4|

All three sizes/hashes were verified in a separate successful TV connection
before execution. Native test reads live theme files and writes only its own
temporary synthetic fixtures; there are no device/display/settings operations.
/tmp/oa-theme-20261102 and synthetic fixture directories were removed;
absence/processes independently checked. Stock boot71f7eb63-727f-4b9a-a9c7-
2b04dc3f9197/live1785/zombie1967, hook/hosts, SSH22/telnet2323, Samba REJECT
and safety markers remain unchanged. No persistent deployment or pending job.

Next wire real UI consumers: own/release the service correctly, resolve the
state/group resources, compose parent/window/slice/textbox geometry and font
origins, and verify FA/UFA labels/graphics against vendor behavior. Current
captured panels and fitted text origins are unchanged. Fonts, images, manifest
selection, localization, shaping and full theme replacement remain open.
