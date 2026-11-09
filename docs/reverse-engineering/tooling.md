# Tooling and search recipes

The goal is to answer “where is this contract?” from the firmware, not from a
decompiler guess. Every query below is read-only and local.

**Local-only paths.** `artifacts/workbench/` (Ghidra projects, extracted rootfs,
objdump cache, ARM toolchain) and `artifacts/dumps/` are not in git. The commands
below use the original host's paths; in a fresh clone, create your own project
and extracted tree from your own dump ([../reproduce/README.md](../reproduce/README.md))
and substitute those paths. Nothing here requires the original set's files.

## Ghidra headless

Canonical launcher:

```sh
GH=/opt/ghidra/support/analyzeHeadless
RE=openaurora/tools/re
```

The project/program map is in [README.md](README.md) and
[binaries.md](binaries.md). Always use `-readOnly -noanalysis` on an existing
project, and never open the same project concurrently.

```sh
# Exact symbol decompile (+ instructions):
$GH artifacts/workbench/ghidra_proj Aurora -process aurora.elf \
  -readOnly -noanalysis -scriptPath "$PWD/$RE" \
  -postScript DumpFirmware.java PLAT_VAUD_SwitchHP

# Exact address, with disassembly:
$GH artifacts/workbench/ghidra_proj Aurora -process aurora.elf \
  -readOnly -noanalysis -scriptPath "$PWD/$RE" \
  -postScript DumpFirmware.java --asm @0x8d3fd8

# Data dump:
$GH artifacts/workbench/ghidra_proj Aurora -process aurora.elf \
  -readOnly -noanalysis -scriptPath "$PWD/$RE" \
  -postScript DumpFirmware.java data:0x136aa18:8

# Callers/data references:
$GH artifacts/workbench/ghidra_proj Aurora -process aurora.elf \
  -readOnly -noanalysis -scriptPath "$PWD/$RE" \
  -postScript FindCallers.java --calls PLAT_VAUD_SetSoftMute
```

- `DumpFirmware.java` accepts an exact symbol, `@0xADDR`, `--asm`, and
  `data:ADDR:LEN`. It also searches function names.
- `FindCallers.java` reports call and data references; use it instead of a
  whole-binary grep when asking “who calls X”.
- `DecompileRaw.java` is for an exact Thumb entry Ghidra did not define. Run
  it only on a **disposable copy** of the project; it creates the function
  there. Never modify the canonical project.
- `-process` names vary by project: `aurora.elf`, `libNDAL3.so`,
  `libNDAL_EX.so`, `ntkdriver_nondovi.ko`, `ntkndpdrm.ko`, `PreInit.ko`,
  `libdirectfb_tdga.so`, `ntkethmac.ko`, `ntkndpalsa.ko`,
  `ntkndphdmiswitch.ko`, `ntkndpextin.ko`, `libmali.so`.

## Extraction from the NAND dump

```sh
artifacts/scripts/extract_from_dump.sh vendor:/com.nvt.idtv/aurora.elf -o /tmp/out
artifacts/scripts/extract_from_dump.sh vendor2:/dynamic_ui -o /tmp/out
artifacts/scripts/extract_from_dump.sh root:/lib/modules/ntkndpdrm.ko -o /tmp/out
```

- Sources: `root` = fs0, `vendor` = ap0, `vendor2` = ap1.
- `/conf` (apdat0) is not supported by the helper. Carve 0xA00000 bytes at
  NAND offset 0xE340000 with `dd`, then extract it as root with ownership
  preserved: `sudo .venv/bin/ubireader_extract_files -k -o conf-ubi apdat0.img`
  (see [../reproduce/root-payload.md](../reproduce/root-payload.md)).
- The extractor uses `artifacts/workbench/rootfs/*` as caches. If a hash does
  not match the report, re-extract before analysing.

## Fast instruction and symbol search

`artifacts/workbench/aurora.dis` is the objdump cache for `aurora.elf`
(~189 MB, `objdump -d --no-show-raw-insn`). Use it for instruction-level greps
without re-disassembling:

```sh
grep -n "bl .*<PLAT_AUDIO" artifacts/workbench/aurora.dis
grep -n -A20 -B5 "^ 8d3fd8:" artifacts/workbench/aurora.dis
```

For a kernel module (ET_REL; symbols are section-relative):

```sh
ARM=artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf
M=artifacts/workbench/rootfs/vendor2/modules/ntkdriver_nondovi.ko

$ARM-nm -n "$M" | grep -iE "Mpfw|PQ|OSD|TCON"     # symbol offsets
$ARM-objdump -dr --no-show-raw-insn --start-address=0x253d84 \
  --stop-address=0x254080 "$M"                     # function + relocations
$ARM-readelf -S "$M" | grep -E "text|rodata"       # section layout
strings -t x -a "$M" | grep -i "PQ_MPFW"           # file offset of a string
```

`objdump -dr` prints relocation names (`.LC*`, `.LANCHOR*`, `R_ARM_CALL`),
which is how a string/data reference is tied back to a function. For an
ET_REL module, do not apply `.text` offsets to `.init.text` or `.rodata`.

For `aurora.elf`, which is stripped in the ordinary sense but keeps dynamic
names:

```sh
$ARM-nm -D --defined-only artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf | grep -i audio
$ARM-readelf -d artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf
```

## ARM execution and tests

```sh
# Host build/tests (no vendor HAL):
cd openaurora && make check

# ARM + qemu with the extracted rootfs:
qemu-arm-static -L ../artifacts/workbench/rootfs/root build-tv-v3/tests/<binary>

# TV/ARM build (glibc 2.23; do not use musl for vendor libs):
make TARGET=tv BUILD=build-tv-v3 \
  CC=../artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc
```

Keep focused sanitizer runs and the full suite distinct. A qemu run proves ARM
execution against a guest rootfs, not device effects. A CPU-only on-TV run is
not display/HAL acceptance.

## ABI and address traps

- **Instructions win.** Saved Ghidra signatures are metadata; verify argument
  registers, return conventions, hidden struct-return pointers, signedness and
  Thumb/ARM state at the call site.
- `BLX` to an even PLT slot enters ARM state; a Thumb trampoline there is
  wrong. Check `CPSR`/instruction encoding.
- External relocation placeholders are not resolved call targets.
- `.init.text` offsets differ from `.text`; never reuse a `.text` address.
- Vendor functions frequently return `void`; a zero/success return does not
  prove the hardware effect.
- The kernel's jiffies/HZ and the load average are not CPU percentages; see
  [PQ-MPFW-TASK](../../artifacts/reports/PQ-MPFW-TASK-2026-11-09.md).

## Recording evidence

A report should state: exact source path and MD5, exact function/address,
whether the evidence is decompile/instruction/on-TV/host/qemu, the failure
branches and ignored returns, and what remains unproven. Raw decompiles live
under `artifacts/analysis/<topic>/`; dated interpretations live under
`artifacts/reports/` and are indexed in
[artifacts/reports/README.md](../../artifacts/reports/README.md). Update
[FINDINGS](../../FINDINGS.md) when a later result corrects an earlier claim.
