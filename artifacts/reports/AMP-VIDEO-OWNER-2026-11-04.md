# Amplifier video24 lifetime boundary — 2026-11-04

This closes the static driver-callback trace requested after AMP-STBC. It does
not add a native MCU backend or establish exclusive hardware authority. No
source behavior changed; no implementation test or hardware acceptance gate is
closed by this report.

## Endpoint and firmware evidence

The authorized metadata-only TV query identified `/dev/video24` as char81:1,
`ndp_comp_extin`, platform driver `ndp-extin`, module `ntkndpextin`. The
preferred tvssh helper initially reported unreachable/asleep and sent WoL before
connecting. Subsequent commands only listed device/sysfs metadata; they did not
open video24, issue ioctls, stage helpers or access fb1. No boot/PID continuity
sample was taken, so this is not evidence of unchanged boot identity.

Evidence is under `artifacts/analysis/amp-video-owner-2026-11-04/`.
Shipped ntkndpextin.ko MD5 cb0689e9e1c425a27e670168e51cc32f; libv4l2.so.0
MD5 59b81f5abd13cc9cccaae4517734ff74. New durable Ghidra project is
`artifacts/workbench/ghidra_extin`, Extin/ntkndpextin.ko. Initial import was
analyzed; subsequent reads used readOnly/noanalysis. This import's .text
addresses add0x10228 to ELF section offsets; do not apply that to .rodata.

* `.rodata` relocation records tie `ndp_comp_fops` at0x1978 to video_ioctl2
  at0x1988, comp_open at0x1994 and comp_release at0x1998. Ghidra FindCallers
  reported no references to the latter two: absent imported data references
  are not proof of unused callbacks. Inspect section-specific ELF relocations.
* `_ndp_extin_drv_comp_open` at1604c and `_ndp_extin_drv_comp_release` at17688
  return0 after tracing/debug checks; release also calls video_devdata. Their
  inspected ARM bodies contain no input disconnect or MCU setup/teardown.
  This only bounds these callbacks, not the complete generic V4L2 lifecycle.
* `_ndp_extin_drv_comp_release_func` at19774 is a different function. It can
  call NDP_ExtSrc_ChangeInput(...,0x16), clear active connection information and
  reset g_comp_connect_tid. Its caller is comp_s_input at19c18. It is not the
  OS close callback. Do not conflate it with comp_release.
* comp_s_ext_ctrls at17938 takes its controls pointer from r2. Class0xa00000
  enters component handling; other classes, including MCU0x0f170000, forward
  the controls directly to EX_PLAT_ext_ctrls. The call at17988 is corroborated
  by .text relocation0x7760. The inspected forwarding path returns0 regardless
  of the lower result. Device status inside the packet still matters; ioctl
  success is not MCU success. AMP-STBC retains the lower dispatcher/IPC trace.
* querycap at1b1cc writes capabilities0x84000001 and device_caps0x04000001.
  These advertise capture/streaming and make the libv4l2 wrapper relevant;
  they do not prove capture setup is harmless.

Earlier inspected `_VPLT_Open`/`_VPLT_Close` manipulate gb8Open, but no evidence
ties them to this endpoint's file operations. Keep that extract as a rejected
lead, not the ownership contract.

## Remaining boundary and next work

Shipped v4l2_open at4e9827a8 performs the OS open and then calls v4l2_fd_open.
v4l2_close has wrapper tracking and a close retry-on-EINTR path. These wrappers
are not plain libc open/close equivalents. Full v4l2_fd_open conversion/format
behavior remains untraced. Trace it before selecting a native fd backend;
do not copy the wrapper's retry policy blindly into our single-close contract.

Static no-op driver callbacks resolve part of the previous lower-lifetime
question. They do not establish safe concurrent ownership, MCU readiness,
exclusive board authority, paired shutdown/resume or the reason for historical
cold-call resets. No live open/close or MCU getter/setter was attempted. The
cold STBC-call warning remains in force. Integrate only after coherent fd,
worker, retained-image and consumer/provider lifetime are established.

Verification was source-hash, decompilation, ARM instruction and section-specific
relocation comparison, plus the endpoint metadata read. No code changed, so no
new runtime suite was run; the prior AMP-STBC394-check result is historical
evidence for that implementation, not a test of this ownership boundary.
