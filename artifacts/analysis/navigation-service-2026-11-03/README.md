# Settings-navigation provider evidence

Interpretation: ../../reports/NAVIGATION-SERVICE-2026-11-03.md. No TV access.
default-state.txt: fresh sequential read-only/noanalysis DumpFirmware.java
@ce0f40 against Aurora MD5 64c252340d3af6c5d66072611b7bad7a. ARM cache excerpts
confirm zero registers/stores atce1358/ce1360 and1259 store atce175e.
Field setters and operation/plan corpus are in ../launcher-routing-2026-11-03/.
message-state.txt: DumpFirmware.java --asm @d0e9b8 @d18868 checks the
navigation message payload and later TabDialog construction using DemoApp.

```
make -C openaurora check-ui-navigation
make -C openaurora check-ui-navigation-ipc
```

host-test/sanitize/arm-qemu are real-DSO service lifecycle groups; arm-build
uses the durable glibc toolchain/sysroot. unix-ipc.txt runs the actual local
Unix control path, without a UI/display/HAL module. The process/socket/config
are removed by the test. host-loader-check is a host-only metadata/relocation
preflight, not a TV loader/device check. host-full preserves the full strict
suite. All deployed/recovery/historical ABI stages remain unchanged.
