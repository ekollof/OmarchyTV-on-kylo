# Reproduce the work on your own TV

Status: onboarding and reproducibility guide for **owners of the same hardware**.
Last verified: 2026-11-10 (host commands and paths; the physical USB steps were
last exercised on the original set in the reports below, not re-run for this
guide). No firmware, vendor libraries, private keys or tested payload images are
distributed here; you extract or build everything from your own TV.

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
- The documented work trees (`conf/`, `hook/`, `verify/`, `.venv/`, `remote.txt`,
  keys, images) are in `.gitignore`. Never `git add -f` them: they contain your
  TV's settings database, vendor files and private keys.

## End-to-end checklist

1. **Verify hardware/firmware.** Record model, mainboard, firmware and your
   TV's real Ethernet MAC. Do not apply the original set's MAC or addresses.
2. **Host dependencies.** Follow [dependencies.md](dependencies.md) and run
   `tools/check-host.sh`. Ghidra, the ARM toolchain, qemu, mtd-utils and
   ubi_reader are not shipped.
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
6. **Write and boot.** Use the USB apply script with **your** image; wait for
   the progress screen; find the IP by DHCP/ARP; verify `id` is root. Then
   harden the listeners.
7. **Reconstruct the analysis/build environment.** Extract your binaries,
   import them into your own Ghidra projects, run the host tests and ARM build
   (see [../reverse-engineering/tooling.md](../reverse-engineering/tooling.md)
   and [../../openaurora/docs/ARCHITECTURE.md](../../openaurora/docs/ARCHITECTURE.md)).
8. **Record your own identities.** The hashes/addresses in this repo describe
   firmware 0.51.52.3. If yours differs, record your own and re-verify every
   address from your firmware before reusing a contract.
9. **Restore stock** with your pristine image and `restore_apdat0.scr`, then
   verify access/stock. Remove staging and leave no helper running.

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
  -> hook execs stock aurora unless /conf/oa_boot enables the replacement
```

Minimal hook (your own file, root shell only):

```sh
#!/bin/sh
# /conf/remotelf/aurora.elf - owner bootstrap, stock UI afterwards
ifconfig eth0 up 2>/dev/null
udhcpc -i eth0 -b -q -n -t 10 -T 3 -A 5 2>/dev/null
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts 2>/dev/null
telnetd -l /bin/sh -p 2323
exec /vendor/com.nvt.idtv/aurora.elf
```

Use the tracked `openaurora/tools/boot/aurora-boot-hook.sh` only after
replacing its hard-coded MAC and reading what it does (dropbear/telnet,
privacy blocks, keepalive, supervised native boot gate). The original set's
`artifacts/payload/aurora.elf`, images and keys are local-only and must not be
copied to another TV.

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

1. Keep your pristine `apdat0_orig.ubifs` (built from the extracted tree) and a
   FAT32 stick with `restore_apdat0.scr` renamed to `kylo_usb_update.scr`.
2. Power off, insert the stick, hold OK while reconnecting mains, release at
   the progress screen, wait ~2 minutes, do not interrupt.
3. Verify stock UI and access afterward.

The tested `artifacts/recovery-stability/` image and keys belong to the
original set. Do not use them on your TV; the tracked `kylo_usb_update.scr`
script is generic only in that it writes **your** `apdat0_full.ubifs`.

## Troubleshooting

- **Stick not read:** FAT32 (MBR), `kylo_usb_update.scr` exactly, <8 KB, final
  `%` line, try a USB 2.0 stick; the U-Boot device may be `usb 0:0` instead of
  `usb 0:1` (adjust the tracked script only if your bootloader differs).
- **Boots stock, no root:** verify `/conf/remote.txt` exists, the hook is
  executable (`/conf/remotelf/aurora.elf`), `run.sh` selected the debug applist,
  and appmgr launched `run_aurora.sh`. Check the hook's log
  (`/buffer/tvhack-boot.log`) and listeners `22`/`2323`.
- **No network:** the hook must bring up `eth0` and DHCP; find the lease by ARP
  scan or your router, and replace any hard-coded MAC with your TV's.
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
