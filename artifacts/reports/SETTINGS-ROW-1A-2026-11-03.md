# Settings-row renderItems type 0x1a — 2026-11-03

`common/settings_row_render.c` now covers type 0x1a (26) end to end: the
shared row+0x104 tail's 0x1a sub-branch writes row word 0x2e8c into all four
child name slots and skips setFont before the shared focus/elipses/create,
then the case-0x1a body looks up the "button" rectangle and drives the
row+0x684 button (0x2ed0/0x2ecc/0x2ed8/0x2ed4 names + create when uncreated,
EASIRect math + setWindowRect/setText when created). **3032** native/C
comparisons pass with exact call order/args, rect words, text bytes and row
state, plus strict host, focused ASan/UBSan, ARM/qemu, all prior groups and a
focused real-TV CPU run (exit 0). No screen/input/device/settings/network/
service/boot/persistent change; staging removed, no helper remains.

## Source and recovered behavior

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only objdump/ELF. The two
new branches, both objdump-verified:

- **Tail sub-branch** (0xd565e0..0xd5661e): reached only from the shared
  0x104 tail's not-created path. `cmp r3,#26; bne 0xd545e0` sends type 0x1a to
  a block that loads `[r5+0x2e8c]` for slots 1, 0, 3 and 2, with **no**
  setFont, then rejoins `0xd5461e` (setFocus 0, setElipses 0, create).
- **Case 0x1a body** (0xd54b34..0xd54b9c, joining 0xd54ace/0xd54af4):
  `getRectangle("button")`, refuse on null, then `isCreated(row+0x684)`. Not
  created writes `[r5+0x2ed0]`/`[r5+0x2ecc]`/`[r5+0x2ed8]`/`[r5+0x2ed4]` into
  slots 1/0/3/2 and creates from the button rectangle; created applies the
  EASIRect copy/offset/point math plus setWindowRect/setText. The create
  passes the rectangle directly (`mov r3,r6`), not the dead stack temp.

The case-0x1a "button" key resolves through its own literal at 0xd54e24
(second-level slot 0x178d5e8) and had to be redirected to the owned string,
or the body dereferences vendor rodata and faults. The dispatch order is:
key2rect present → shared tail → switch (case 0x1a when applicable);
key2rect null → switch directly, so the case body still runs with no tail.

## Verification and limits

**3032** native/C comparisons: types {0,1,4,0x1a} × 16 key-rect presence
masks × created × 3 point variants × row-word set × label pair × rebuild
flag, minus the 48 firmware-crashing type-4 inputs, plus eight default-type
cases in both created states. They compare complete ordered traces, rect
words, text bytes and full row state. Host units add the 0x1a tail names
(0x2e8c x4, no setFont), the case-body name order (1/0/3/2) and the created
button path. Strict full host/actual Unix IPC, focused ASan/UBSan, fresh
ARM/qemu units, every prior group and the focused TV CPU test pass. Isolated
build `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/settings_row_render | 52876 | 83222713de62681546e6ca4f43fbccd2 |
| tests/tab_policy_firmware | 1006308 | 5c97553c04ccd4331190efb376342993 |

The standalone TV test requires only GLIBC_2.4 and the correct ARM loader.
tools/tvssh.sh found the TV already awake, verified no-reset flag/free space,
staged only the focused binary and checked size/hash in a separate connection
before execution. Test exit 0 passed, staging was removed and the /proc/*/exe
scan found no helper; /conf/coredumpnoreset remained present. No vendor
fragment, device API, screen/input/audio control, service restart, network/
settings/boot mutation or persistent deployment was run on the TV.

This closes type 0x1a. Still open: switch case bodies 2..25, row construction,
child visibility internals, live settings/resource acquisition, scrollbar
internals, skin/module/display/scanout/startup and independent visible parity.
