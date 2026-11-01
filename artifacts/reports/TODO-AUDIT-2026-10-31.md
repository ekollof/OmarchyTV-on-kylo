# TODO accuracy audit — 2026-10-31

> **Status: Historical checklist audit; later MODULE-SAFETY, PWM, OSD and RUNTIME-CHECK close some findings. Current pending work is in TODO.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

**Subsequent offline follow-up:** thread-post ownership and descriptor-size
validation have since been implemented in ABI 2 and tested on host, with
ASan/UBSan and ARM/qemu; see [MODULE-SAFETY-2026-10-31.md](MODULE-SAFETY-2026-10-31.md).
The inventory below records findings at audit time. TODO now tracks the updated
state; no ABI 2 build has been deployed to the TV.

Further firmware-first work corrected the old palette/buffer-handshake and DRM
exclusion claims and fixed a skipped fbdev frame commit; see
[OSD-BUFFER-CONTRACT-2026-10-31.md](OSD-BUFFER-CONTRACT-2026-10-31.md).
The stale PWM header/return claims are also corrected, with instruction evidence:
[PWM-CONTRACT-RECHECK-2026-10-31.md](PWM-CONTRACT-RECHECK-2026-10-31.md).

Audited the 72 checked entries present when the owner requested the audit, against
the current source, build/test results, saved firmware analyses and read-only
observations of the target TV. Fourteen were reopened as incomplete. The separate
plane-setup checkmark had already been reopened earlier in this continuation.
Other entries were narrowed where they described a historical experiment or an
artifact rather than a completed replacement feature.

This is not a replay of every hardware experiment. In the inventory below,
**code** means the implementation was inspected; **live** means checked again on
the TV; **historical** means supported by the saved report/artifact but not
independently repeated. A retained research checkmark does not certify a working
native implementation. No TV reboot, source change, real suspend, UART capture,
watchdog write or display ioctl was performed for this audit.

## Bugs reproduced and fixed

1. `core/host.c`: when all 128 hold slots were occupied, lookup logged an error
   but returned the service pointer anyway. It now returns NULL unless an
   existing hold can be reused.
2. `core/module.c`: direct unload of a held provider was refused, but `stop`
   removed its service/holds and allowed subsequent unload while the consumer
   retained a pointer. Stop now refuses a held provider; control reports failure.
   Explicit force-unload retains its documented unsafe override.
3. Failed `init()` called destroy/dlclose without sweeping the module's
   registrations. A fixture that published a service and timer before returning
   -1 left both registered. It now destroys with the correct owner, sweeps
   registrations and then closes the library. Failed-start stop callbacks also
   run under their module owner.

`tests/registry.c` and `tests/failed_init.c` cover these cases. `make check` passes
the existing lifecycle smoke suite, registry regressions and 11 recovery cases.
The registry executable also passes as ARM code under qemu with the TV's rootfs.
The recovery work also fixed ignored SIGTERM/SIGINT and added independently
supervised host-loop liveness; see `openaurora/docs/RECOVERY.md`.

## Remaining implementation gaps

| area | actual gap |
|---|---|
| thread posts / reload | queued posts store callback and userdata, without module ownership; unload cannot cancel/drain them safely. A callback may outlive its library. |
| descriptor ABI | `oa_module` has magic/version but no size field. `oa_host_api` has size; earlier descriptions confused the two. |
| IR | f[3] is treated heuristically as an RC code, otherwise NEC cmd is emitted. No confirmed canonical map; repeat is never emitted; 0x5e release is assumed. |
| display | rendering to vendor-initialised fbdev is recorded; independent enable, buffer binding, cold scanout and resume restoration are not established. |
| power | service standby emits events and writes sysfs; no other module subscribes to those power events. Host suspend and resume are separate commands, not an automatic complete cycle. |
| audio | `plat` and `audio` repeat the audio prelude; Protect_Init prototypes disagree. No audible routing/output verification. |
| source | only VIN select/start is implemented. Companion VPLT command, V4l2Info binding, audio routing and source list are missing. Opening the VIN node does not prove switching. |
| platform | most calls return void. Private copied VIN arrays are unused and cannot initialise aurora's internal routing state. |
| settings | generic integer service exists; typed access, string/BLOB handling and NVRAM do not. A numeric identifier can be parsed as an SQL expression; this is not a schema-checked typed accessor. |
| generated HAL header | `vendor/lib/vendor-hal.h` and its generator still describe obsolete PWM layouts/return values; current panel code uses separate corrected declarations. Do not reuse that header as an authoritative ABI. |

## Checked-entry inventory

IDs refer to the ordering of the 72 checkmarks at audit start, not current line
numbers. **Reopened** means the checkbox is now empty. **Narrowed** retains a
completed subtask while removing an unsupported broader claim.

| ID | item | verdict and evidence |
|---:|---|---|
| 1 | network/privacy restored | live: internet=1; ads/personalised/marketing/third-party/ACR=0; logging=0, DNT=1. Corrected stale logging=1 text. |
| 2 | hosts block | live hosts entries present; 90-second absence of traffic remains historical TELEMETRY report evidence. |
| 3 | WoL works | code in wol.py; historical end-to-end wake report, not repeated. |
| 4 | wake setting / PHY | live setting=1; PHY retention evidence historical. |
| 5 | static sqlite | live executable, 1,349,016 bytes; executed read-only queries successfully. Free-space description is approximate. |
| 6 | network self-heal | code: pre-UI UPDATE and 15-second eth0 watchdog; live internet=1. Deliberate disabled-network/reboot test historical. |
| 7 | privacy persisted | live flags match; persistence test historical. |
| 8 | Samba IP / hosts block | live OUTPUT REJECT for 216.183.117.0/24 and hosts entries. Egress capture historical. |
| 9 | awake/SSH helpers | code: ensure_awake.py, wol.py, tvssh.sh use the target and unwrapped SSH. |
| 10 | no-signal sleep cause | historical diagnosis in MEMORY and standby reports; not retested. |
| 11 | DHCP retries | code: -t 5 -T 3 -A 3 in both hook paths. |
| 12 | hook fail-safe | narrowed: normal paths reach exec; native recovery supervised. Setup hangs or supervisor-loader failure are not universally covered. |
| 13 | dropbear respawn | code: PID-file check every 30 seconds; earlier kill/respawn test historical. |
| 14 | guarded hosts bind | code: file existence guard in hook. |
| 15 | bounded log | narrowed to actual code: rotate at boot above 200 KB, not a runtime size bound. |
| 16 | aurora influence analysis | historical research: AURORA-INFLUENCE report/decompilations; not a new exhaustive security review. |
| 17 | DIAL restored | live ports 56789/56790 and dd.xml 200 with correct Application-URL; key/browser/state execution remains historical. |
| 18 | screenshot tool | code: SSH framebuffer read and BGRA decode; saved captures present. Framebuffer pixels alone do not prove panel scanout. |
| 19 | virtual keymap extracted | XML artifacts and IR/DIAL reports present; extraction task complete. |
| 20 | no cached DCF | historical negative search, not a permanent guarantee of runtime cache state. |
| 21 | key slots dumped | historical OTA report/fingerprints and local RE artifact; no vendor signing-key recovery claim. |
| 22 | browser capability probe | historical P0 report and captured results; does not establish reliability under all workloads. |
| 23 | mock launcher | code and P1 report; corrected BGRA claim for fb1. |
| 24 | source switching via keys | historical DIAL experiment; does not validate native mod_source. |
| 25 | web boot integration | code present and P2 historical failure test; currently opt-in flag absent. |
| 26 | portal re-entry tile | portal_tile.sh and historical DB/UI experiment; not re-exercised on screen. |
| 27 | OmarchyTV web port | app/platform files and P3 report; changed current-tense boot claim to historical opt-in result. |
| 28 | offline simulator | tools/tvsim.py and tvsimctl.sh implement stated control/browser shim; prior screenshots/state tests historical. |
| 29 | splash | narrowed: artifact and post-init render exist; negative cold result was superseded by quarter-screen boot paint. Full cold splash remains open. |
| 30 | OSD init RE | narrowed to recovered ioctl vocabulary. Earlier claim that acquire/commit is always required is unsupported by later visible raw writes. |
| 31 | DRM driver identified | narrowed: identification retained; standard KMS replacing vendor OSD is not proven. |
| 32 | remote solved on paper | reopened: protocol skeleton exists; byte mapping, repeat/release and native navigation remain unconfirmed. |
| 33 | CEC spec research | historical CEC-SPEC report/local normative reference. Research complete does not mean mod_cec exists. |
| 34 | CEC comparison | historical CEC-SPEC-VS-AURORA analysis; no independent comprehensive conformance test. |
| 35 | vendor init reversed | historical AURORA-INIT call census/report; not full replacement implementation. |
| 36 | offline extractor | code inspected, cached extracted files present; historical byte-identity verification. |
| 37 | source/CEC architecture mapped | historical SOURCES-CEC analysis; native routing/policy implementation still partial/absent. |
| 38 | CEC input renaming | historical CEC-INPUT-RENAME decompilation; no new device interaction. |
| 39 | UBIFS snapshot | actual MD5 matches 76b1464a4b70566b014a58ac39ef795b; round-trip/ownership test historical. Snapshot is older than live hook. |
| 40 | second image rebuild | same verified image MD5; historical hook hash corrected in TODO. New supervisor is not in these images. |
| 41 | core complete | reopened: ownerless thread posts invalidate the general safe-reload claim. Three registry bugs fixed in this audit. |
| 42 | module ABI size validation | reopened: descriptor size field does not exist. |
| 43 | safe loader complete | reopened: fixed three reproduced bugs, but posts/descriptor compatibility remain. |
| 44 | control plane | code and passing lifecycle tests; stop now reports held-provider refusal. |
| 45 | lifecycle tests | passing again; corrected stale count/scope. Tests do not prove native hardware parity or all state contents. |
| 46 | full IR module | reopened: reader exists, full input semantics not confirmed or implemented. |
| 47 | ticker/console state | code: save/restore and event subscriptions exist. Smoke test covers restore invocation; non-default selection preservation is not exercised. |
| 48 | vendor reuse strategy | retained, corrected distinct dynamic function counts to Hal=321/NDAL=80 and actual linked-library strategy. |
| 49 | panel HAL wrapper | code implements corrected six-/four-word PWM descriptors; host/qemu reachability evidence. T1 panel still correctly unchecked. |
| 50 | display complete | reopened: runtime paint is not independent display bring-up or resume parity. |
| 51 | IR keymap complete | reopened: CSV recovery done, canonical live UART translation not done. |
| 52 | power complete | reopened: sysfs/WoL test is lower-level than native lifecycle integration. |
| 53 | audio complete | reopened: ABI/init ownership and audible routing/output unresolved. |
| 54 | source complete | reopened: open observed, switch not validated and route setup incomplete. |
| 55 | settings complete | reopened: integer subset exists; typed/blob/NVRAM scope unfinished. |
| 56 | TV toolchain | rebuilt host/ARM; symbols remain <=GLIBC_2.17; qemu and ARM-on-TV fake-display recovery ran. |
| 57 | first native render | historical visually confirmed milestone; code has since evolved to portal parity. Does not certify cold scanout. |
| 58 | colour correction | code BGRA decoder/renderer and corrected image artifact agree; historical colour-bar confirmation. |
| 59 | content pipeline | mk-ui.py and icons-to-raw.py implement OAI1 conversion. Inputs include external local extracted assets; not a self-contained asset bundle. |
| 60 | watchdog control | wdtctl implements recorded packet; historical real disable experiment. Audit uses stub tools only. |
| 61 | crash reset path | live core_pattern and coredumpnoreset flag; reset helper analysis and crash-off test historical. |
| 62 | watchdog research | narrowed: rejected conclusion that one failed disabled-state kick proves feeding impossible. |
| 63 | region resolved | reopened: read/modify/write implemented, full field semantics and cold visibility not resolved. |
| 64 | takeover harness | narrowed to tested software supervision; old one-shot guard text removed. Real takeover/fallback acceptance still untested. |
| 65 | platform bring-up complete | reopened: calls sequenced, but void results and unused VIN state do not establish parity. |
| 66 | DirectFB unnecessary | reopened: filtered trace cannot establish this; explicit OSD commit found separately. |
| 67 | whitespace bug fixed | code trims leading value space; spaced settings.db in passing tests exercises the parser. |
| 68 | dry run | narrowed to historical calls, framebuffer bytes and link-gap discovery, not verified scanout or successful hardware bring-up. |
| 69 | prior review fixes | review occurred; most fixes present, hold-table claim was false until this audit's fix. Remaining gaps recorded explicitly. |
| 70 | NDAL library dependency | Makefile links NDAL3/EX and closure; prior loader failure explained by required data symbols. |
| 71 | module parameters | historical install_fpga.sh extraction/report; not a newly executed driver load. |
| 72 | translated strings extracted | narrowed: reference file actually has 56,715 sorted text lines. Raw field counts and alleged Dutch slot do not establish preserved language/ID alignment. |

Stale unchecked tasks were also closed narrowly: initial OAI1 conversion and
supervised test-mode implementation, plus already-recorded quick-standby, network
permission, stock re-entry, DRM probe and current-state documentation work. None closes
native portal interaction or real cold-takeover acceptance.

## Current target and artifacts

Read-only checks: SSH/telnet/DIAL ports open; DIAL description 200; network/privacy
flags match the intended values; Samba reject and hosts block present;
`coredumpnoreset` present. Vendor UI remains active. Native and web boot flags
are absent. No signed volume was changed.

Recounted with `readelf --dyn-syms --wide`: 321 distinct defined Hal_* functions
in NDAL3, 80 NDAL_* in EX. Counts from combined symbol tables can double-count
symbols; old 643/160 totals are not unique exported entry-point counts.

`apdat0_full.ubifs`, `apdat0_orig.ubifs` and raw mtd13 image hashes match the
existing payload README. The loose/live hook is newer. Rebuild images separately;
do not infer image freshness from the presence of an updated loose hook.
