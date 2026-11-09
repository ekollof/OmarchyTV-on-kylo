# Boot-hook cold attempt and recovery — 2026-11-09

Status: **the persistent boot-hook path was exercised for a true boot-path cold
start; the second attempt left the set unreachable and required the tested
`/conf` USB recovery. The native path is disabled again (`/conf/oa_safe`
present, `/conf/oa_boot` absent) and stock is running. This is not a successful
cold boot and not a replacement installation.**

## Recovered stock boot chain

Read-only from the set and the offline rootfs/vendor trees:

```text
kernel init=/init  (root=/dev/ubiblock0_0 squashfs ro)
  -> /etc/inittab: ::sysinit:/etc/rc.local
  -> /etc/rc.local
       mount_ramdisk (proc/sys, tmpfs /tmp,/media)
       nvt_bootlogo.sh -> insmod PreInit.ko; bootlogo from mtd_xbootdat/mtd_logo
       mount_ubifs:
         nvt_ubi_mount 2 fs0 ap1 /vendor2 ro
         nvt_ubi_mount 1 fs0 ap0 /vendor ro
         mount_apdat -> nvt_ubifs_mount apdat0 apdat0 /conf rw
       setup_network, enable_swap
       sh /usr/script/startup.sh
  -> /usr/script/startup.sh
       if /vendor/run.sh: cd /vendor; ./run.sh        (always present here)
       else USB /media/sda{1,}/board/run.sh
       if /oad/oadLoader: run it
  -> /vendor/run.sh
       /conf/remote.txt exists -> NVT_APP_INI=nvt.app.debug.ini
       (else /conf/run_with_dwelf + /vendor/data/nvt.app.dwelf.ini, which is absent)
       -> com.nvt.appmgr --applist=nvt.app.debug.ini
  -> appmgr [idtv] program=/mtd_apexe/board/run_aurora.sh
       /mtd_apexe/board is a second mount of ubiblock0_1 (same ro squashfs as /vendor)
  -> run_aurora.sh
       /conf/remote.txt exists -> insmod cifs, force dev MAC, udhcpc, mount
       //<ip>/mb230cifs to /conf/remotelf, run /conf/remotelf/aurora.elf
       (else run /vendor/com.nvt.idtv/aurora.elf)
```

All boot scripts and app lists are on read-only squashfs. `/conf` (apdat0) and
`/buffer` (ubi14, also mounted at `/mtd_rwarea`) are the only writable volumes;
the only supported program-execution override is the `/conf/remote.txt` CIFS
hook. `/oad` is on the read-only rootfs; `/vendor/data/nvt.app.dwelf.ini` does
not exist on this firmware.

## The installed hook (prior session, restored by recovery)

```text
/conf/remote.txt                 16 bytes, "0:127.0.0.1:x:x"
/conf/remotelf/aurora.elf        8753 bytes, sha256 c3943712d6962e009ff1a62e29fd59aa3ddf623e774384708dbec35ee9f7bebc
```

`run_aurora.sh` selects the CIFS branch, the loopback mount fails immediately,
and it executes `/conf/remotelf/aurora.elf`. That script starts its own
`dropbear -p 22` (root authorized_keys), `telnetd -p 2323`, dialserver and
keepalive, restores the real MAC `70:54:b4:fe:70:c6`, then:

```sh
OA_NATIVE=1 only if
  /conf/oa_boot exists && /conf/oa_safe absent && /conf/coredumpnoreset &&
  /conf/openaurora.conf readable &&
  /buffer/openaurora/openaurora && /buffer/openaurora/oa-supervise && /buffer/wdtctl executable
if OA_NATIVE: exec /buffer/openaurora/oa-supervise -- /buffer/openaurora/openaurora --config /conf/openaurora.conf
else:         exec /vendor/com.nvt.idtv/aurora.elf
```

`oa-supervise` defaults are `/tmp/openaurora.alive`, `/tmp/openaurora.ready`,
`/buffer/wdtctl`, fallback `/vendor/com.nvt.idtv/aurora.elf`, fail
`/conf/oa_fail`, enable `/conf/oa_boot`, safe `/conf/oa_safe`. It disables the
MCU watchdog before the child, re-arms it on fallback, and removes
`/conf/oa_boot` after three recorded failures. This only covers user-space
failure; a kernel/MCU hang is outside it.

## Attempt 1 — vendor path selected

Staged the 34-module cold bundle at `/buffer/openaurora`, wrote the cold profile
to `/conf/openaurora.conf` (absolute `ui_menu.file` and `audio_mixer.broker`),
created `/conf/oa_boot`, removed `/conf/oa_safe`, and rebooted. The set came up
on **vendor aurora** and no supervisor line appeared in `/conf/tvhack.log`. All
seven `OA_NATIVE` file tests evaluated true after boot, so the likely cause was
a mount race: `mount_buffer.sh` runs in parallel with appmgr. No diagnostic was
installed on this attempt.

## Attempt 2 — native path and hang

Installed an instrumented copy of the same hook (sha256
`fd0e44dcf4e645de60ce81ed4dd46cc27be1f80c0afa22ce532eaa48a12db694`, 9632 bytes;
preserved under `artifacts/analysis/boot-hook-2026-11-09/`) that logs the seven
conditions before and after a bounded wait for `/tmp/mount_buffer_ready`, plus
the final decision. Behaviour was otherwise unchanged.

After reboot the set never returned:

- no ICMP reply and no ARP entry for either `70:54:b4:fe:70:c6` or
  `d4:e2:cc:ec:45:e9` after three full `/24` sweeps;
- TCP 22 and 2323: `No route to host`;
- repeat WoL: no effect (the set was powered, not asleep).

The boot log and instrumented hook were overwritten by the recovery, so the
exact decision and hang point are **not captured**. The most likely reading is
that the added wait closed the mount race, `OA_NATIVE=1`, and the cold
platform init (DRM/OSD/panel/SPI/tuner in one start) hung the kernel or MCU
before the network was reachable. There is no serial or core evidence to prove
a specific driver.

## Recovery

Used the tested `recovery-stability` stick exactly as documented:

```text
apdat0_full.ubifs       md5 994da7ff76a0b2fbafb8f6fa0c682627
kylo_usb_update.scr     md5 de1591298d633cf6422a1ade4e8835a9 (584 bytes)
mains off -> stick in -> hold OK at power-on -> ~2 min
```

The script rewrites only the unsigned `apdat0` volume and reads it back to the
stick. The restored image forces stock (`oa_safe` present, `oa_boot` absent),
restores the Oct-31 hook and config, and preserves the SSH/telnet access path.

Post-recovery evidence:

```text
appmgr 1554, run_aurora.sh 1711, /vendor/com.nvt.idtv/aurora.elf 1756
fb1 1920,2160 stride 7680 pan 0,1080
/conf/oa_safe present (Oct 31), /conf/oa_boot absent
/conf/openaurora.conf 816 bytes (Oct 31 12-module profile)
/conf/remotelf/aurora.elf sha256 c3943712... (Oct 31 hook)
dropbear :22 and telnetd :2323 running; watchdog re-armed
```

fb0 and fb1 OSD captures taken immediately after recovery were blank; stock
may be composited on a GPU/video plane rather than those OSD buffers, and the
owner confirmed the set was back. This is not an independent visible-UI check.

Cleanup: the 34-module staged bundle was removed, the prior 12-module payload
was moved back to `/buffer/openaurora`, and
`/buffer/openaurora-cold.conf`, `/buffer/openaurora.conf.12mod-20261031` and
`/buffer/oa_safe.boot-restore` were deleted. No `oa-supervise`, `openaurora`,
`oa-ir-blast` or broker helper is running.

## Consequences and next requirements

- A stopped-stock takeover is **not** evidence for a power-on cold boot.
  `appmgr`/`privserver`/DFB are already running and own hardware even when
  stock aurora is not.
- The supervisor's vendor fallback cannot recover a kernel or MCU hang. A
  recovery path that survives the replacement's own crash must not depend on
  the replacement, the network or a writable `/buffer` payload.
- Do not disable the MCU watchdog before the replacement has proven itself on
  a cold boot. Keep the vendor watchdog armed until a bounded post-init health
  check passes, and only then take over feeding it.
- Retry only with **staged cold bring-up**: UI/OSD/DRM first, then one hardware
  stage at a time (panel, SPI/NIM, tuner, audio), each surviving a full
  power-cycle before the next is enabled.
- Keep a boot marker that the next boot can report, or serial capture, so a
  hang is diagnosable without losing the log to recovery.
- The native path remains disabled and the boot-hook/persistent-replacement
  gate stays closed until these conditions are met.

## Follow-up: staged-path attempts (same day)

After the recovery, the staged safety path was implemented and three more
armed attempts were made. The TV is stock after each; the `/conf` recovery
image was used again after the last one.

### Attempt A — gate argument bug (safe fallback)

The first armed stage-0 boot selected native, but `run-aurora.sh` passed
`/conf` to `oa-boot-gate.sh`, which treated its argument as a filesystem root
and checked `/conf/conf/oa_boot`. It blocked and the wrapper exec'd stock — the
fail-closed path worked. The wrapper log showed:

```text
run-aurora /buffer/openaurora stage=0
oa-boot-gate: native blocked (safe=0 boot=0 fail=0)
run-aurora: boot gate blocked
```

The gate now takes the state directory directly (`/conf`). No replacement
code ran.

### Attempt B — tuner init failure and loader cleanup (safe fallback)

With the gate fixed, stage 0 reached the base bring-up and display:

```text
plat: audio prelude (Protect/Mixer/Proxy)
plat: common init (vpltio/v4l2/sharemem, drm 1, DVIF)
plat: NDAL_VideoExt_OpenDevice(0,10)
plat: sequence done (gpio/iic errors 0; common/video unverifiable: void returns)
display: /dev/fb1 osd reinit ... region -> 1920x1080 (ok)
```

The full autoload then hit the tuner (`tuner.backend=hal`, `hw_init=0`), whose
`m_init` intentionally returns `-EACCES`, and the core never reached its host
loop. The failed init left `slot->ctx` NULL; the loader called
`quiesce(NULL)`/`destroy(NULL)`, and the tuner's destroy dereferenced the NULL
context. The supervisor's startup deadline fired, recorded failure 1/3,
stopped the feeder and exec'd vendor aurora. The set stayed reachable.

Fixes: the loader skips quiesce/destroy when a failed init returned no ctx;
the stage profiles no longer autoload hardware owners before their stage;
`spi` is now included in the bundle; the supervisor puts the child in its own
process group and kills the group so IR/broker helpers are not orphaned.

### Attempt C — UI rendered, wrong OSD geometry (not a hang)

The UI-base stage (stage 0 autoload: plat, display, panel, ir, settings,
service_db, image_decode, epg, pvr, channels, i18n, ticker, ui_*, lua, power;
no tuner/SPI/audio/source/CEC) was armed and power-cycled. The owner saw
**our UI**, but only in the top-left quadrant and with icons missing. The set
was alive and drawing; it was not a kernel hang. Only `192.168.178.87` was
probed, so the network miss was misread as a hang and the USB `/conf` recovery
was used unnecessarily (it did restore stock). The stage log was lost with
`/conf`.

The quadrant is consistent with `display.osd_reinit=1` forcing the OSD
var/SetPar to 1920x1080 on a true boot, which shrinks/repositions the vendor's
display window. The takeover path never did that and showed the UI correctly.
The watchdog feeder was feeding a live kernel and is not implicated; its reset
behavior still has no isolated test.

Consequences:

- Stage 0 is now the takeover display path (`plat.drm_init=0`,
  `display.cold_init=0`, `display.osd_reinit=0`,
  `display.restore_snapshot=1`). The forced DRM/OSD reinit moved to stage 2.
- The `plat` software/audio prelude, VPLTIO/V4L2/sharemem, GPIO/IIC and video
  device bring-up still run in every stage; they are the next candidates to
  gate if a later stage fails.
- Boot and app logs now live on `/buffer` (`tvhack-boot.log`,
  `openaurora-app.log`, `oa-boot-history.log`), which the apdat0 recovery does
  not touch. A next attempt leaves diagnosable evidence.
- A full subnet ARP sweep is mandatory before concluding the set is offline;
  `.87` alone is not evidence.

Cleanup after recovery: the staged bundles and backups were removed and the
prior 12-module payload was restored to `/buffer/openaurora`. `/conf` is the
Oct-31 image (`oa_safe` present, no `oa_boot`), stock appmgr/aurora running,
watchdog re-armed, no helpers.

## Evidence files

- `artifacts/analysis/boot-hook-2026-11-09/aurora.elf.restored` — the Oct-31
  hook as restored from the set.
- `artifacts/analysis/boot-hook-2026-11-09/aurora.elf.instrumented` — the
  diagnostic hook used on attempt 2.
- `artifacts/analysis/boot-hook-2026-11-09/openaurora-cold.conf` — the cold
  profile deployed on attempt 1/2.
- Offline boot scripts: `artifacts/workbench/rootfs/root/etc/rc.local`,
  `root/usr/script/startup.sh`, `vendor/run.sh`, `vendor/run_aurora.sh`.
- Recovery pair: `artifacts/recovery-stability/` (hashes above).
