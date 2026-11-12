# REA second-opinion layer

`rea.sh` wraps the optional, pinned `rea-agents@6.9.0` install. It bootstraps
the package under `artifacts/workbench/rea` (gitignored), copies targets to
`/tmp/opencode/rea-targets`, and uses the ARM cross objdump automatically.

```sh
tools/re/rea.sh help
tools/re/rea.sh doctor
tools/re/rea.sh kernel artifacts/workbench/rootfs/vendor2/modules/ntkdriver_nondovi.ko
tools/re/rea.sh function artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf 0x0102fce0
tools/re/rea.sh objdump artifacts/workbench/rootfs/vendor2/modules/ntkdriver_nondovi.ko _CEC_TX_Data
tools/re/rea.sh raw <rea args...>
```

- `doctor` exits nonzero when Hopper is missing; check the Ghidra lines.
- `function` starts Ghidra 12.2; the first import of a large binary can take
  minutes.
- `objdump` needs the ARM cross-binutils; the host objdump cannot decode
  ELF32/ARM.
- `raw` bypasses the copy and should only be used with a copy path.

REA is a lead generator, not the source of truth. It imports its own Ghidra
copy, does not reuse the canonical `ghidra_*` projects, and must not be pointed
at the TV or used for runtime/debugger work on vendor binaries. Verify exact
instructions in the canonical Ghidra/objdump workflow before recording a
contract.

Details and the 2026-11-12 pilot result:
[docs/reverse-engineering/tooling.md](../../docs/reverse-engineering/tooling.md).
