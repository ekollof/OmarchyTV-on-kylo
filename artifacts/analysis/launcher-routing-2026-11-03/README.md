# Launcher result route evidence

Interpretation: ../../reports/LAUNCHER-ROUTING-2026-11-03.md. No TV access.
Source ELF MD5 64c252340d3af6c5d66072611b7bad7a.

Sequential read-only/noanalysis DumpFirmware.java --asm queries:

- selection-operation.txt: @edde60 @d3dc54
- id-conversion.txt: @e4ae0c
- menu-caller.txt: @cf74a0 (containing cf7120); @cc78a0 resolved to unrelated
  cc77fc parental-control getter, not the intended wrapper; do not use it as
  routing evidence. Exact FindCallers call site is queried separately below.
- operation-wrapper.txt: @cc78d0 (actual handleOperation caller)
- launcher-bridge.txt: @d3d7dc @e48404 @e4a02c
- focus-modal.txt: @e4831c
- main-dispatch.txt: @e4a3f4
- retained-setters.txt: @ccca0c @ccca44 @cccaac
- callers.txt: FindCallers.java @d3dc54 @e4ae0c

The focus-modal filename is historical naming of that query: the focus function
does not itself call doModal. Main dispatcher and DemoApp caller establish the
conditional main modal-result path. Do not infer every Select takes that path.

Independent corpus check:

```
make -C openaurora check-launcher-route
openaurora/build/tests/launcher_route --dump > /tmp/oa-launcher-route.txt
node artifacts/analysis/launcher-routing-2026-11-03/verify-route.cjs /tmp/oa-launcher-route.txt
```

host/arm-corpus.txt are maintained C outputs; verify-route reads actual signed
ELF entries and derives ordered commands from fresh case text, not copied test
fixtures. host/arm-verify record all62 operation and28 action-plan comparisons.
Full host, focused sanitizer, ARM build/qemu logs preserved. Model rejects4b
portal prework and a6 runtime guards. No logging/telemetry behavior is ported.
No service/skin integration or actual TV/key acceptance claimed.
