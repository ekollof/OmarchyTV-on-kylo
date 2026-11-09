# Reproduce the work on your own TV

Status: onboarding and reproducibility guide for **owners of the same hardware**.
Last verified: 2026-11-10 (host commands and paths; the physical USB steps were
last exercised on the original set in the reports cited by
[root-payload.md](root-payload.md), not re-run for this guide). No firmware,
vendor libraries, private keys or tested payload images are distributed here;
you extract or build everything from your own TV.

## Who this is for

- Telefunken D65U546A / Vestel MB230 (2199818 on the original set), firmware
  **0.51.52.3**. Confirm the model/serial on the rating label and the software
  version in the stock Settings/System-information screen (the vendor
  `default.ui` manifest also names `V.0.51.52.3`). Check your model, mainboard
  and firmware before using any address, image or script. Other builds may
  differ.
- You own the TV. This path requires physical access to the set; it is not a
  remote exploit. It can void warranty and exposes a root shell on your LAN
  until you secure it. You are responsible for local law, the network and the
  set.
- The replacement is incomplete. Root access is the bootstrap for research and
  build/test work, not acceptance of the replacement or authorization for a
  persistent native boot.

## What this repository contains (and does not)

**In git:** source, host tests, USB/recovery scripts, the boot-hook template
(`openaurora/tools/boot/aurora-boot-hook.sh`), dated reports, the compiled RE
docs, hashes and analysis text.

**Not in git (local-only, extract/build yourself):** the NAND dump, vendor
libraries and firmware, Ghidra project databases, the ARM toolchain, payload
images, dropbear, SSH/host keys, the settings database and the original set's
`artifacts/payload/` and `artifacts/recovery-stability/apdat0_full.ubifs`.
`artifacts/workbench/`, `artifacts/dumps/` and `artifacts/payload/` are absent
in a fresh clone. Never publish your dump, vendor libraries or keys.

Clone the source (or your fork) and run the commands from its root:

```sh
git clone https://github.com/ekollof/OmarchyTV-on-kylo.git tvhack
cd tvhack
```

## Safety first

- Only the unsigned **apdat0 (`/conf`)** volume is written. Never write
  fs0/ap0/ap1, kernel, bootloader or SPI; they are signature-verified.
- A broken `/conf` image can require physical recovery. Keep a pristine dump
  and a tested restore stick before the first write.
- Use a dedicated FAT32 stick containing only `kylo_usb_update.scr` and the
  intended image; remove any other update files so the bootloader cannot pick a
  different payload.
- Do not interrupt the USB update. Read `artifacts/scripts/README.md` before
  running any flash-writing script.
- Telnet is unauthenticated root. Prefer SSH keys, firewall ports 22/2323 to
  your management host and disable telnet when done. Never expose the set to
  the internet.
- `remote.txt` + the hook persists in `/conf`; stock restore means writing your
  pristine image back, not merely deleting a file.
- The documented work trees (`conf/`, `hook/`, `verify/`, `verify-orig/`,
  `.venv/`, `remote.txt`, `my-hook.sh`, keys, dropbear, settings DB, images) are
  in `.gitignore`. Never `git add -f` them: they contain your TV's settings
  database, vendor files and private keys.

## End-to-end checklist

1. **Verify hardware/firmware.** Record model, mainboard, firmware and your
   TV's real Ethernet MAC. Do not apply the original set's MAC or addresses.
2. **Host dependencies.** Follow [dependencies.md](dependencies.md). Run
   `tools/check-host.sh --root` before the dump; it checks only the NAND ->
   `/conf` path. Run the full checker before the build/Ghidra stage. Ghidra,
   the ARM toolchain and qemu are analysis-only and do not block root access.
3. **Dump your own NAND (read-only).** Put `nand_dump.scr` on a FAT32 stick as
   `kylo_usb_update.scr`, hold OK while reconnecting mains, and collect
   `n00.bin..n63.bin`. Merge to a 512 MiB `tv_nand.bin` and hash it. This is
   your source of truth.
4. **Extract `/conf` and the firmware trees.** Carve apdat0 (0xE340000,
   0xA00000), unpack it with `ubireader_extract_files -k` as root so the
   `101:101` idtv ownership survives, and extract root/vendor/vendor2 with
   `extract_from_dump.sh` (`DUMP=<your dump>`). Keep the pristine `/conf` tree
   and a pristine `apdat0_orig.ubifs`.
5. **Build your own root image.** Start from your pristine `/conf`, add
   `remote.txt`, the tracked hook (edit the MAC), telnetd and optionally
   dropbear/keys; build/pad/verify the UBIFS image. Full recipe:
   [root-payload.md](root-payload.md).
6. **Write and boot.** Use the tested 584-byte USB script with **your** image;
   wait for the progress screen; find the IP by DHCP/ARP; verify `id` is root.
   The vendor UI returns under the debug app list (Netflix/Amazon/YouTube/DIAL
   stay off while `remote.txt` exists). Then harden the listeners.
7. **Reconstruct the analysis/build environment.** Extract your binaries,
   import them into your own Ghidra projects, run the host tests and ARM build
   (see [../reverse-engineering/tooling.md](../reverse-engineering/tooling.md)
   and [../../openaurora/docs/ARCHITECTURE.md](../../openaurora/docs/ARCHITECTURE.md)).
8. **Record your own identities.** The hashes/addresses in this repo describe
   firmware 0.51.52.3. If yours differs, record your own and re-verify every
   address from your firmware before reusing a contract.
9. **Restore stock** with your pristine image named `apdat0_full.ubifs` on the
   stick and the same 584-byte tested script; verify stock UI/access afterward.
   Remove staging and leave no helper running.

## Bootstrap root in one page

The mechanism is the vendor's own debug app path, not a memory exploit:

```text
/conf/remote.txt exists
  -> run.sh selects nvt.app.debug.ini
  -> appmgr starts /mtd_apexe/board/run_aurora.sh for the idtv app
     (the script and hook were observed as uid 0 on this firmware)
  -> run_aurora.sh tries the loopback CIFS mount, fails
  -> executes /conf/remotelf/aurora.elf
  -> hook starts telnetd :2323 (root) and optionally dropbear :22
  -> hook execs vendor aurora unless /conf/oa_boot enables the replacement
```

Minimal hook (your own file, root shell only):

```sh
#!/bin/sh
# /conf/remotelf/aurora.elf - owner bootstrap, vendor UI afterwards
OWNER_MAC=AA:BB:CC:DD:EE:FF   # <- your TV's MAC, not the vendor debug MAC
ifconfig eth0 down
ifconfig eth0 hw ether "$OWNER_MAC"
ifconfig eth0 up
udhcpc -i eth0 -b -q -n -t 10 -T 3 -A 5 2>/dev/null
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts 2>/dev/null
telnetd -l /bin/sh -p 2323
exec /vendor/com.nvt.idtv/aurora.elf
```

`run_aurora.sh` has already forced the firmware debug MAC
`D4:E2:CC:EC:45:E9`; every TV that skips the MAC line above keeps that shared
address. Replace `OWNER_MAC` with the MAC on your rating label/stock network
screen.

The vendor UI returns, but `remote.txt` selects the debug app list: Netflix,
Amazon, YouTube and the vendor dialserver stay disabled until you restore the
pristine `/conf`.

Use the tracked `openaurora/tools/boot/aurora-boot-hook.sh` only after
replacing its hard-coded MAC and reading what it does (dropbear/telnet,
settingsDB internet flag, Samba iptables rule, DIAL, keepalive, supervised
native boot gate). Create `/conf/no_force_net` and `/conf/no_dial` for a
telnet/SSH-only hook. The original set's `artifacts/payload/aurora.elf`, images
and keys are local-only and must not be copied to another TV.

## Build and analysis workflow

- Host tests: `cd openaurora && make` / `make check`.
- Vendor link libraries: `cd openaurora && make vendor` runs
  `scripts/fetch-vendor.sh`, which extracts the eight link libraries from
  `artifacts/dumps/tv_nand.bin` (or `DUMP=`) into `openaurora/vendor/lib`. They
  are vendor/Novatek binaries: local-only, never commit or redistribute them.
- ARM/TV build: `make TARGET=tv BUILD=build-tv-v3
  CC=<pinned-toolchain>/arm-none-linux-gnueabihf-gcc`. If you keep the vendor
  tree elsewhere, add `VENDOR=<extracted vendor dir>` (it must contain `lib/`).
- Ghidra: import your `aurora.elf` and modules into your own projects; use
  `openaurora/tools/re/DumpFirmware.java`, `FindCallers.java` and the recipes in
  [../reverse-engineering/tooling.md](../reverse-engineering/tooling.md).
- The TV is glibc 2.23/hard-float. Check new binaries with `readelf
  --version-info`/`objdump -T`; a toolchain newer than the target can emit
  symbols the TV does not have. The project avoids newer symbols; verify your
  own builds. Static helper binaries are an option for simple tools.
- The replacement's persistent boot/hardware gates remain in `TODO.md`; do not
  turn a successful root shell into a persistent deployment.

## Recovery

1. Keep your pristine `apdat0_orig.ubifs` (built and round-trip verified from
   the extracted tree) on a **separate** FAT32 restore stick, renamed
   `apdat0_full.ubifs`.
2. Put the tested 584-byte script (`artifacts/recovery-stability/kylo_usb_update.scr`,
   MD5 `de1591298d633cf6422a1ade4e8835a9`) on the stick as
   `kylo_usb_update.scr`; it writes and reads back `0x782000` bytes.
3. Power off, insert the stick, hold OK while reconnecting mains, release at
   the progress screen, wait ~2 minutes, do not interrupt. Compare
   `apdat0_readback.bin` with your pristine image.
4. Verify the retail UI and access afterward.

The tested `artifacts/recovery-stability/` image and keys belong to the
original set. Do not use that image, its keys, IP or MAC on your TV. The
script's 584 bytes are only U-Boot commands and load **your**
`apdat0_full.ubifs`; the 549-byte `apply_apdat0_hook.scr` did not complete on
the original set, so it is not the first-write recommendation. The 392-byte
`restore_apdat0.scr` has no readback and has not completed a recovery; use the
584-byte script for the first restore.

## Troubleshooting

- **Stick not read:** FAT32 (MBR), `kylo_usb_update.scr` exactly, <8 KB, final
  `%` line, try a USB 2.0 stick; the U-Boot device may be `usb 0:0` instead of
  `usb 0:1` (adjust the tracked script only if your bootloader differs).
- **Vendor UI but no root:** verify `/conf/remote.txt` exists, the hook is
  executable (`/conf/remotelf/aurora.elf`), `run.sh` selected the debug applist,
  and appmgr launched `run_aurora.sh`. Check the hook's log
  (`/buffer/tvhack-boot.log`) and listeners `22`/`2323`.
- **No UI / black screen after the hook:** `run_aurora.sh` always executes
  `/conf/remotelf/aurora.elf` and does not fall back to stock Aurora when the
  hook is missing or not executable. Restore your pristine image from the USB
  stick.
- **No network:** the hook must program your TV's MAC, bring up `eth0` and run
  DHCP. `run_aurora.sh` otherwise leaves the shared debug MAC
  `D4:E2:CC:EC:45:E9`; find the lease by ARP scan or your router.
- **`/conf` fails to mount:** the stock mount helper may format it after
  repeated failures; restore the pristine image and settings backup.
- **No SSH:** dropbear may be missing or built against a newer glibc; telnet
  remains the fallback, or build a static dropbear and test it first.

## Legal and redistribution

This guide documents a physical, owner-controlled recovery/debug path on the
same hardware. It links upstream tools and tells you where to obtain them; it
does not provide vendor firmware, libraries, keys, databases or images. Extract
those only from a TV you own and keep them private. Do not redistribute them or
commit them to a public repository.
