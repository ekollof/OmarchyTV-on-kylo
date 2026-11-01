# CEC ownership/backend evidence

MIXER-MODULE-2026-11-01 interprets mixer-module-*, live-mixer-module-*,
live-mixer-matching-* and spawn-*: real module/service/control-socket lifecycle,
native query/reload/reaping, target libc spawn semantics and complete16-module
ABI3 TV loader preflight. Runtime tar/manifest preserved; both stages removed
and stock baseline unchanged. spawn return0/child127 is firmware behavior,
not a qemu defect. Earlier failed fixtures are retained as diagnosis history.

AUDIO-BROKER-2026-11-01 interprets audio-broker-* / live-audio-broker-*:
fresh read-only prelude/PID-binding trace, separate native query executable,
strict local IPC, host/sanitizer/ARM-qemu checks and real spawned TV query/reap.
Exact tested ELFs preserved; staging removed, stock baseline retained. Initial
ARM fixture fcntl2.28 loader failure is preserved separately and corrected.
The helper is a blocking child; a nonblocking parent module is still needed.

CEC-ASYNC-2026-11-01 interprets cec-async-* / live-cec-async-*:
copied ticketed requests/results, retained failed-close backend, full host and
focused sanitizer/ARM-qemu tests. Four injected-I/O groups pass on the TV;
exact ELF preserved, staging removed, stock baseline intact. No live CEC I/O.

AUDIO-ASYNC-2026-11-01 interprets audio-async-* / live-audio-async-*:
bounded submissions/value results, full host/sanitizer/ARM-qemu checks and
three injected-I/O groups on TV. Exact tested ELF preserved locally; staging
removed and stock baseline retained. No live socket/device access. Module
service integration and CEC async requests remain pending.

WORK-REVIEW-2026-11-01 interprets review-* logs: baseline/fixed full host suite,
focused sanitizer checks, explicit ARM/qemu tests and one view-only TV status
read. No review binary was uploaded. Current stock boot/PID supersede earlier
samples; see MEMORY. Owner quiesce is now nonblocking and loop-thread audio
binding is disabled until async completion exists.

Interpretation: ../../reports/CEC-OWNERSHIP-2026-11-01.md. Work crossed the local
date boundary from October 31 to November 1; early command paths in logs retain
their original date. The initial ownership/backend stage was offline. Subsequent
authorized live logs and spec regressions are interpreted in
../../reports/CEC-LIVE-2026-11-01.md.

- preinit-vu / preinit-registration: target PreInit device emulation, actual
  Linux registration and ioctl/open/release dispatch; new analysed project saved
  durably under workbench/ghidra_preinit. preinit-project-SHA256SUMS records it.
- cec-fileops-relocations: exact .rel.rodata entries; other relocation sections
  reuse offsets and must not be conflated with the operations table.
- init-authority: CEC driver registration, hardware initialization and STBC
  authority/register helpers, with ARM instructions. Different helper operations
  are not interchangeable.
- audio-output-owner: amplifier task/init and output/speaker routing setup.
- pcb-function-inventory / pcb-audio-query / pcb-audio-side-effects: getter
  discovery and utility/tag-file behavior, all offline. The DumpFirmware.java
  find:<regex> option was compiled/exercised against this existing read-only project.
- host-device / sanitize-device / arm-device-build / arm-device: three injected
  raw-backend I/O groups. host-full: full strict host suite including the new target.

preinit-directory-failure preserves the initial failed import before the new
project directory existed. That handle ended with exit 1; the directory was then
created and the successful import/analysis performed. Later PreInit work used
-readOnly -noanalysis. Missing-directory failure does not imply corrupt firmware.

Follow-up evidence:
- live-* logs: source hashes, stable boot/UI/access/protections, temporary node
  ownership refusal, shared controls, vendor cache and three standard LA8 queries.
- shared-control-device / hdmiswitch-*: NDAL fd path and target HDMI switch
  open/release/control dispatch. New project has hdmiswitch-project-SHA256SUMS.
- controller-layout / controller-string-layout / controller-log-counter: exact
  constructor/cache offsets and UString representation; counter is not RX count.
- controller-wrong-address is a preserved mistaken address resolving a DVR
  function, not CEC evidence. live-process-memory initially selected zombie1899;
  live-cec-flags uses actual UI1737. Neither failure diagnoses a blocked API.
- spec-*: updated identity/spec regressions and strict/sanitizer/ARM checks.
  Live query acceptance is not proof of fresh response payloads or raw backend
  ownership. The final cleanup check confirms directory absent and no helper.

Audio follow-up: ../../reports/AUDIO-ROUTING-2026-11-01.md interprets amp-*,
ad82587-contract, audio-easyset-abi, live-audio-*, live-amp-* and audio-state-*.
Live descriptor/object prove AD82587_D selection; earlier AD82586C was an
unsupported assumption. amp-factory's last query (unrecognized task function)
failed after the useful results; objdump amp-task-instructions resolves the code.
amp-volume-callers has no direct references, not proof that amp volume is unused.
amp-dispatch's dmp_get_audio_type is unrelated DLNA content metadata.

AUDIO-BOARD-2026-11-01 interprets the subsequent amp-board/amp-iic/amp-stbc,
iic-* and live-iic/live-amp-* logs. PreInit adapter explains vendor two-message
read semantics; DDCCI and PreInit's separately named IIC_Ioctl are different paths.
Real amp-probe returned03..05=1b/05/05, two complete messages; cleanup verified.
live-iic-fds also records termination of the accidental vendor-stdin cat (9821).
Never read fd targets merely to inspect their names; use readlink.

AUDIO-VOLUME-2026-11-01 interprets volume-*, aq-file-layout, live-volume-*,
live-aq-pointer-volume and live-curve-*. All101 decoded normal AQ entries match
live-volume-table.bin; same curve test also passed on actual TV and was removed.
SetMainVolume is a range-check-only wrapper, not an audio setter. Samba calls in
ICE_AudioVolume are evidence only and excluded from implementation.

AUDIO-CURVE-MODULE-2026-11-01 interprets curve-module-* / live-curve-module-*:
real DSO lifecycle/consumer holds/failure/suspended-reload tests, including TV
execution, verified staged identities and complete cleanup. Full host suite passes.
curve-module-sanitize is the initial executable-only -no-pie link mistake for a
DSO; sanitize-dso-build and sanitize-pass record the corrected build/test.

AUDIO-GAIN-TRANSPORT-2026-11-01 interprets audio-gain-* and live-audio-mixer/
live-audio-kitems/live-mixer logs. New ghidra_alsa has its own project manifest.
Message-backed HAL0 loses send failure; ALSA get is a request cache. The first
fresh-process mixer query lacked the stock ALSA_CONFIG_PATH and failed; the same
verified binary with that variable succeeded. This was not a gain-write test.

AUDIO-NETLINK-2026-11-01 interprets audio-netlink-* / live-audio-netlink-*:
synchronous callback/table lifetime, discarded result, tested codec and native
read-only GetOPUGain exchange. Kernel-unicast is target NAND disassembly. GermBeacon
and capability functions were inspected to avoid misusing loader/setter APIs;
neither was executed. Probe staging was removed; no gain/table write occurred.

The session follow-up in AUDIO-NETLINK interprets audio-client-* and
live-audio-client-*: five offline I/O/fork groups, full host suite and refactored
native query pass. Current session-probe identity is44556 bytes /
bfef5a6123cbece7d17dfee92440f5e3; the initial private-socket probe is historical.
Staging removed. Table/selection methods have injected-I/O coverage only.

QUIESCENCE-2026-11-01 interprets quiesce-* / live-quiesce-*:
ABI 3 nonblocking cleanup veto, six real-DSO groups on host/sanitizers/ARM-qemu/TV,
full host suite and all 13 production modules' on-TV --check. Staging removed;
stock boot/UI/access/protection hashes unchanged. No hardware module started.
Shutdown completion/retry remains open before worker integration.

Subsequent implementation reports SHUTDOWN, OWNER-WORKER, WORK-REVIEW,
AUDIO-ASYNC, CEC-ASYNC, AUDIO-BROKER and MIXER-MODULE supersede that earlier
shutdown/integration queue; use the report index and canonical TODO for gates.

SPEAKER-GAIN-2026-11-01 interprets speaker-setter-* and live-speaker-wire-*:
registered direct setter, raw operand bounds, driver caches and distinct UI
AQ/balance/ratio path. The earlier audio-netlink-query-contract.txt supplies
the getter's100-to0x8000 normalization; it prevents exact raw-gain rollback.
Three codec/six client groups pass host/sanitizers/ARM-qemu/TV using injected
I/O only. Exact native units are in speaker-wire-tested-tv.tar. No real gain
write, new broker opcode or persistent deployment; staging was removed.
