# Panel takeover experiments — historical evidence

> **Status: Historical watchdog/crash evidence. Deployment snapshots and old scanout plans superseded by MEMORY, RECOVERY and current OSD reports.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

The full previous report is preserved in
[the archive](../archive/docs-before-tidy-2026-10-31/reports/PANEL-TAKEOVER-2026-10-31.md).
Its “pick up here” deployment snapshot and later plan paragraphs are obsolete.
[MEMORY](../../MEMORY.md) owns current state; the TV is in use and takeover
is not currently authorized.

## Retained findings

- Vendor Aurora feeds the MCU watchdog. With watchdog disabled through the
  recovered pcbconfig packet path, stopping/killing Aurora did not reset the
  tested system; appmgr did not automatically respawn it in that experiment.
- wdtctl uses /proc/nvtstbcipc, command 0xae: six-byte disable packet
  {0,0xae,0,0,0,0xae}, or timeout {0,0xae,hi,lo,1,0xae}.
  This is an implemented/tested control path, not an instruction to use it now.
- Cold NDAL_VideoExt_STBC calls reset the set. Do not retry them as a watchdog
  shortcut; saved no-argument signatures did not establish initialized state.
- core_helper.sh normally detaches partitions and arms a one-second reset on
  process crash. /conf/coredumpnoreset prevents that reset path; a deliberate
  historical SIGSEGV with it present left the system running.
- /vendor/run.sh trigger_reboot is a deliberate reboot trigger, not a supervisor.
- Native platform/display calls and framebuffer readback were observed, but
  independent visible scanout was not proved. A frame metric or successful
  region write is insufficient.
- A stacking/dp_order attempt reset the set; no verified visibility fix resulted.

## What supersedes the old next steps

[RECOVERY](../../openaurora/docs/RECOVERY.md) now defines a separate supervisor
with heartbeat/first-present receipts and fake-hardware tests. Real cold takeover
remains unverified; “watchdog solved” is not “safe replacement boot solved.”

[DRM-PLANE-CONTRACT](DRM-PLANE-CONTRACT-2026-10-31.md) proves generic _kms_init is
discovery, not SETPLANE. [OSD-BUFFER-CONTRACT](OSD-BUFFER-CONTRACT-2026-10-31.md)
and the kernel/MAU follow-ups trace actual allocation/commit. Do not repeat
dp_order, blindly call HAL stages, infer scanout from pixels, or recreate fb
nodes just because old text attributed registration to Aurora.
