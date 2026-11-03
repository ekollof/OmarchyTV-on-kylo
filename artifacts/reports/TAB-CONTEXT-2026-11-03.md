# Profile context and reloadable ui.profile — 2026-11-03

`ui_profile` now publishes copied immutable profile facts through `ui.profile`
v1. It supplies the connectivity and hotel-support inputs needed by the pending
TabDialog consumer, without inferring them from screenshots or settings SQL.
This does not supply mutable hotel activation, media/app state or permissions.

## Firmware source and correction

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Sequential read-only/noanalysis
Ghidra/ARM extracts are retained under `../analysis/tab-context-2026-11-03/`.
provider-binding.txt preserves the cached ARM source excerpt: DemoApp cd601e
calls NovoSystemInitialiser::applicationCapabilityProvider f21e64 and stores
the result at+314 (cd6022). provider-vtable.txt resolves the concrete
NovoApplicationCapabilityProvider vtable153481c, with actual vptr+8.

Its slot+3c is **getMenuCapability1056b38**, not a Wi-Fi or connectivity-capability
getter. menu-capability.txt shows its output: byte0 digital IB, word4 feature
OSD, byte8 CAM message, byte9 connectivity, byte10 AI menu. TabDialog ctor
d6f47a..d6f4a0 calls this slot with a stack buffer and copies buffer+9 into
byte37a0. The returned record is at least11 bytes. The network tab tests the
copied byte.

connectivity.txt traces MenuCapability::setConnectivityAvailability10752dc to
ProfileConfiguration::isConnectivityEnabled f5fe10. profile-access.txt proves
that wrapper enables only integer **equal to1**. Profile_GetConnectivityEnabled
c69aac reads profile `swprofile`, section `Config/Connectivity`, key
`Connectivity`, default0. Profile_IsHotelModeEnabled c62894 (preceding evidence)
reads `swprofile`, `Config/HotelTv`, `HotelMode`, default1; its wrapper also
tests equal to1. These settings control support, not the active hotel restriction.

The menu-capability provider has a separate null-MenuCapability fallback:
connectivity byte9 is1. The profile decoder/service represent the normal
profile-backed path, not that absent-object fallback. Wi-Fi availability remains
a separate showSubMenu content-creation gate. No vendor AI, tracking or ad
behavior is imported by decoding these two ordinary TV feature flags.

The saved swprofile is60075 bytes, MD5 `a1d9e5e80a926a6e0abd812c98852894`,
matching artifacts/profiles and dumps/apdat_files copies. It contains type5
inline values **Connectivity1/HotelMode1**. These are saved-source findings,
not a fresh TV-state poll. Header+8 reads91078 (getDataSize returns that word),
which exceeds this saved file's length. The numeric reader only needs bounded
record data; initialize/inline lookup do not require copying that advertised
size. Do not “repair” the file or copy beyond the actual buffer to match it.
The discrepancy's origin is not established here.

## Decoder and service contract

common/profile.c implements the inspected ROMProfile hash, section traversal
and inline-u32 lookup. getUniqueID c5e100 uses **signed bytes**, signed division
remainders and ARM register shifts; negative shift remainders produce zero,
not an undefined C shift. Hashes are case-sensitive, NUL excluded. Sections
skip their descendant record counts; empty slash path segments are ignored.
The first matching non-section hash wins, even when its type is not numeric.
Only type5 supplies the inline word at+c. A missing section/value or wrong
type supplies the caller's default and present0, matching the native result.

Traversal bounds-checks the root record area and each traversed subsection span.
Malformed spans/magic return EBADMSG, bad arguments EINVAL, without modifying
outputs. This is a scalar reader, not validation of unrelated name tables,
binary payloads or every branch of the profile framework. The tab decoder
atomically returns raw values, exact-one enabled flags and presence flags.

ui_profile requires explicit `ui_profile.swprofile`; example configuration is
openaurora/examples/ui-profile.conf. It opens a regular file read-only/nonblocking,
rejects more than16MiB (a project allocation bound), snapshots its actual bytes
with short/growth checks, closes the descriptor and decodes before publication.
`copy()` has no I/O or borrowed pointers. Ordinary holds protect normal stop/
unload/reload. The published flag clears before removal callbacks, so cached
calls during removal fail ENODEV without touching output. Failed publication
preserves an existing peer service and can retry. No workers/timers are needed.

CLI/IPC `reload ui_profile` uses the existing generic module lifecycle. Reload
reads the configured file again; it does not reread the host config. Ordinary
paired standby retains immutable facts. Suspended reload defers file read and
publication until activation, including failure/retry. No native struct is
serialized and no vendor process/global profile cache is used.

## Verification and remaining work

The maintained optional ARM/qemu reference harness now executes actual shipped
getUniqueID/findSubSection/findValue/readValue instructions, plus the shipped
signed-division helper. Only guard pointer literals are redirected; strlen is
a local stub. No vendor init, TV/device access or vendor bytes embedded in source.
Its driver checks both ELF and profile MD5s before executing private mappings.
Fragment fingerprints and source boundaries are in the harness.

- **65,281** empty/single/two-byte key hashes match, including every nonzero
  high byte and signed-remainder case.
- **27** actual saved-profile lookups/default combinations match native values
  and present results; both recovered feature settings are1.
- Earlier **884,736** availability and **9,216** group-selection cases still pass.
- Host tests cover nested/duplicate/wrong-type/default cases, all fixture
  truncations, escaping/overflowing descendants, exact-one flags and atomic errors.
- Real DSO tests cover snapshot copying/reload, holds/removal/collision/retry,
  standby/deferred activation, invalid/special/oversized/malformed files and
  successful reads after the source file is removed. Saved-profile corpus passes.
- Full strict host build/suite and focused ASan/UBSan/ARM-qemu pass. Host loader
  preflight passes all23 source modules. No target loader or hardware acceptance.

Isolated build-tv-v3/tab-policy final ARM module36832 bytes/MD5
67e4491c6909c2a251d38d62131e9d54/maxGLIBC2.4; lifecycle test483612/
8c15a5d5bc6acb1671b2ada0b7e3356b; scalar test46560/
c67165aa8ba5db6ec42492c52589b243; optional reference71796/
32b4a745fbc2fa32993c8c85672d9686. No installed ABI/recovery artifacts changed.

No TV connection, wake, keys, drawing, settings/network mutation or deployment.
All checks completed; no helper/build/Ghidra job pending. The pending consumer
must hold/release ui.profile, combine these facts with actual mutable hotel/
media/app/entry state, descriptions and content permission/writeback contracts.
It still must replace the captured screen shortcut and pass real visible parity.
Complete CEC/audio/startup and the rest of TODO remain in scope.
