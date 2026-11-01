# CEC/audio and durable-path evidence

Current interpretation: ../../reports/CEC-AUDIO-CONTRACT-2026-10-31.md.

- oa-audio-abi-2026-10-31.txt: libNDAL3 instructions for Protect/Mixer/Proxy/Ctrl.
- oa-cec-audio-callers-2026-10-31.txt: Aurora volume/mute callers. The trailing
  query used the nonexistent SendData name and failed; preceding function output
  is valid. Correct SendCMD/Open/receive/validation output is in oa-cec-transport.
- oa-cec-transport-2026-10-31.txt: static transport wrappers and policy validator;
  lower helper/MCU/driver ownership still needs recovery.
- oa-cec-audio-host-2026-10-31.txt: full strict host suite; final-host-cases.txt
  adds the subsequent platform-prelude regression, nine groups total.
- final-sanitize-cases.txt / final-arm-build.txt / final-arm-cases.txt: final
  nine groups with fake HAL calls, no hardware access. Earlier build/sanitizer
  logs retain the previous fixture set for chronology.
- durable-path-validation.txt: relocated compiler, guest runtime, UBI/kernel
  Python tools and extraction with empty /tmp, so temporary aliases cannot help.
- relocated-ghidra-validation.txt: relocated NDAL3 project read/decompile also
  succeeds with empty /tmp. The first attempt is retained separately: a read-only
  host Java-settings directory prevented startup, not a missing project. Retry
  used a temporary Java home entirely inside the sandbox.
- workbench-checksums.txt: quiet verification of the workbench's 19,377-file
  SHA256SUMS migration snapshot; empty output with successful exit means all match.

Firmware/source and new ARM binary hashes are in CEC-AUDIO-CONTRACT/MEMORY.
No TV access or deployment. Specifications and analysed projects persist in
../../workbench; historical paths in these logs remain verbatim.
