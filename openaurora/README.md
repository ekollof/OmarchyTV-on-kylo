# openaurora

A modular C replacement for vendor aurora.elf on Telefunken D65U546A / Vestel
MB230. Target: bug-for-bug compatible behavior, reloadable modules through
Unix socket/CLI, then a native OmarchyTV front end. Telemetry and advertising
are excluded, including optional tracking/ACR/ad implementations.

```sh
make BUILD=build-strict
make BUILD=build-strict check
./build-strict/openaurora --check --modules build-strict/modules
./build-strict/openaurora --modules build-strict/modules --sock /tmp/oa.sock \
  --load ticker --load ui_console
# in another shell:
python3 tests/client.py /tmp/oa.sock "reload ui_console"
```

Default CFLAGS retain C11, -Wall -Wextra -Werror -pedantic.
Use [ARCHITECTURE](docs/ARCHITECTURE.md) for ABI/build/lifecycle details,
[RUNTIME-CHECK](docs/RUNTIME-CHECK.md) for preflight and
[RECOVERY](docs/RECOVERY.md) for the independent supervisor.

TV test-window authorization and the latest verified state are in ../AGENTS.md
and ../MEMORY.md. Installed/recovery artifacts remain ABI 1. New development is ABI 3 in
build-tv-v3: rebuild core/modules together and preserve build-tv/build-tv-v2. See the explicit
build command in ARCHITECTURE; make tv currently hardcodes the baseline directory.

| component | actual evidence and remaining limit |
|---|---|
| core/registry/worker posts/ABI checks | host/sanitizer/ARM regressions; state-aware reload and dependency-ordered lifecycle; system-sleep/rollback limits remain |
| Unix control transport | bounded nonblocking queues, partial writes/backpressure, disconnect/input/cleanup tests pass host/sanitizer/ARM; stdout console remains synchronous |
| sleep cycle / power service+CLI | nine engine and seven wrapped-power regression groups pass host/sanitizer/ARM; native device/wake/hardware acceptance remains open |
| --check | preserved matching core +16 modules passed on TV; source now18, graphics/scene_display have separate native evidence; loader preflight is not hardware health |
| display/UI | CPU path plus GPU/coalescer/ui_fb completion-driven readiness verified on real fb1, including UI reload/module cycle and full restoration; full vendor visuals/actions/input/TV standby/cold ownership remain open |
| IR | reader/profile map exist; actual code-space, repeat/release and navigation unverified |
| power | final RAM-entry callback via host.sleep; manual hooks never sleep; full device preparation/wake ownership unverified |
| panel | verified PWM request/return contract; independent brightness/power behavior incomplete |
| audio/source/plat | partial glue; init ownership, routing and hardware effects incomplete |
| CEC/audio follow-up | spec-based wire/status/ARC/identity validators and corrected audio ABI/prelude/cache/handoff; host/sanitizer/ARM-qemu coverage; CEC transport and audible acceptance remain open |
| audio_curve | reloadable immutable AQ curve provider, explicit path/profile; six real-DSO lifecycle groups pass including on TV; no hardware gain application |
| CEC transport contracts | raw/VPL packet layouts and bounded marshalling tested in four host/sanitizer/ARM-qemu groups; driver status/filter/reopen defects recovered; authority/worker/module integration still open |
| raw CEC backend | explicit blocking cdev lifecycle/send/RX with failed-close ownership retention; three injected-I/O host/sanitizer/ARM-qemu groups; authority/workers/module/hardware acceptance still open |
| settings | generic integer service, read-only real-TV milestone; typed/profile/NVRAM behavior incomplete |
| ticker/ui_console | lifecycle/state smoke fixtures working |
| ui_clock | separate firmware-derived corrected UI time; seven host/sanitizer/ARM-qemu DSO groups; native animation controller still pending |
| animation RE/tools | 39 named descriptors and 702 CRC-verified resource names; controller/easing/scrolling/widget states recovered; native effects/remaining callers still open |
| ui_fb | assets and one-frame pixel comparison; complete UI/native ownership not established |

All non-telemetry/ad vendor features remain in scope. Remaining work is in
[TODO](../TODO.md); last verified state in [MEMORY](../MEMORY.md).
Use [FINDINGS](../FINDINGS.md) and the [report index](../artifacts/reports/README.md)
before following an old experiment plan.
