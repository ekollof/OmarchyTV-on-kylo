# Owner root payload: dump, build, write, verify

Status: first-time physical-access recipe for **your own** TV.
Last verified: 2026-11-10 (image build/extract commands and ownership checked on
the host); the USB write/boot sequence was last exercised on the original set in
[STABILITY](../../artifacts/reports/STABILITY-2026-10-31.md) and
[BOOT-HOOK-COLD-ATTEMPT](../../artifacts/reports/BOOT-HOOK-COLD-ATTEMPT-2026-11-09.md).
Read [README.md](README.md) and
[artifacts/scripts/README.md](../../artifacts/scripts/README.md) first. Run the
commands from the repository root (the `.venv` and `.gitignore` paths assume
that). Only the unsigned `apdat0` (`/conf`) volume is written. No firmware,
vendor library, key or tested image from the original set is provided.

The root shell is the vendor's own debug-app path: `/conf/remote.txt` makes
`run.sh` choose the debug applist, appmgr starts
`/mtd_apexe/board/run_aurora.sh` for the idtv app (observed as uid 0 on this
firmware), the loopback CIFS mount fails, and `/conf/remotelf/aurora.elf` runs.
The hook starts a root telnetd (and optionally dropbear) and then execs stock UI.

## 1. Dump your NAND (read-only)

Use a dedicated FAT32 stick (MBR) containing only `kylo_usb_update.scr` and the
dump chunks. Copy the tracked `artifacts/scripts/nand_dump.scr` to
`kylo_usb_update.scr` (exact name, <8 KB, final `%` line). The script reads
64 × 8 MiB = 512 MiB. If your NAND differs, stop and adapt first.

```text
1. TV off, unplug mains.
2. Insert the stick.
3. Hold OK on the remote while reconnecting mains.
4. Keep OK held until the progress screen appears, then release.
5. Wait for the 64 chunks; do not interrupt power.
```

Copy `n00.bin` … `n63.bin` to the host and merge in order:

```sh
for f in $(ls n??.bin | sort); do cat "$f"; done > tv_nand.bin
stat -c%s tv_nand.bin          # expect 536870912 on this hardware
sha256sum tv_nand.bin | tee tv_nand.sha256
```

Keep this dump offline. It is the source of truth for every extraction.

## 2. Extract `/conf` and build the pristine restore image

The `apdat0` volume is at NAND offset `0xE340000`, size `0xA00000`:

```sh
dd if=tv_nand.bin of=apdat0.img bs=4096 \
   skip=$((0xE340000/4096)) count=$((0xA00000/4096)) status=progress

# ubi_reader from dependencies.md (repo-local .venv).
# -k requires root and preserves the UBIFS uid/gid (101 idtv, 110, ...).
# A UBI image extracts as <image-id>/<volume-name>/..., so locate the volume:
sudo rm -rf conf-ubi
sudo .venv/bin/ubireader_extract_files -k -o conf-ubi apdat0.img
conf=$(sudo find conf-ubi -maxdepth 3 -type d -name apdat0 | head -1)
echo "$conf"
```

Without `-k` the tree is owned by the invoking user and the rebuilt image
changes `/conf` ownership; that is not a valid restore image. Verify the tree:

```sh
sudo sqlite3 "$conf/sqlite_db/settingsDB.db" 'pragma integrity_check;'
sudo stat -c '%u:%g %a %n' "$conf" "$conf/sqlite_db/settingsDB.db"
```

The database must print `ok`; the volume root and most files are owned by
`101:101` (idtv), with a few `0:0`, `105:105` and `110:110`. Confirm the UBI
geometry before building, because `mkfs.ubifs` must match it:

```sh
.venv/bin/ubireader_display_info apdat0.img | \
  grep -iE "LEB Size|PEB Size|Volume Name|reserved_pebs"
# tested: LEB 253952, PEB 262144, Volume apdat0, reserved_pebs 31
```

Copy the volume directory to `conf/` and keep `apdat0.img`:

```sh
sudo rm -rf conf
sudo cp -a "$conf" conf
```

A bare UBIFS payload image (such as `apdat0_full.ubifs`) extracts directly into
the output directory, without the `<image-id>/<volume>` level.

Build your **pristine restore** image (same layout as the tested one):

```sh
sudo mkfs.ubifs -r conf -m 4096 -e 253952 -c 31 -x zlib -o apdat0_orig.ubifs
# pad to the tested volume size 0x782000 (7,872,512) with 0xff
sudo python3 - <<'EOF'
d = open('apdat0_orig.ubifs','rb').read()
assert len(d) <= 0x782000, len(d)
open('apdat0_orig.ubifs','wb').write(d + b'\xff' * (0x782000 - len(d)))
EOF
```

If your extracted volume table differs (LEB size/count/volume size), stop and
adapt `-c`, the pad size and the UBI write length; do not write beyond the
volume.

## 3. Prepare the hook tree

Start from a copy of `conf/` (the pristine tree); do not edit the pristine
copy. `cp -a` as root keeps the numeric owners:

```sh
sudo rm -rf hook
sudo cp -a conf hook
```

Add:

```text
remote.txt                     "0:127.0.0.1:x:x" + newline
remotelf/aurora.elf            your hook, executable (mode 755)
remotelf/authorized_keys        your SSH public key(s), mode 600  [optional]
remotelf/hostkey_rsa            generated host key, mode 600     [optional]
remotelf/hostkey_ed25519        generated host key, mode 600     [optional]
remotelf/dropbear               ARM dropbear, mode 755           [optional]
remotelf/keepalive.sh           local keepalive helper           [optional, not shipped]
remotelf/hosts                  /etc/hosts blocklist             [optional, not shipped]
oa_safe                         empty; keeps the full hook on stock UI (do NOT create oa_boot)
```

Create the required entries from your own hook file:

```sh
sudo mkdir -p hook/remotelf
printf '0:127.0.0.1:x:x\n' | sudo tee hook/remote.txt >/dev/null
sudo chown 101:101 hook/remote.txt
sudo install -m 755 my-hook.sh hook/remotelf/aurora.elf
sudo touch hook/oa_safe
sudo chown 101:101 hook/oa_safe
```

**Minimal hook** (no dropbear/privacy dependencies; edit to taste):

```sh
#!/bin/sh
# /conf/remotelf/aurora.elf - owner bootstrap, then stock UI
ifconfig eth0 up 2>/dev/null
udhcpc -i eth0 -b -q -n -t 10 -T 3 -A 5 2>/dev/null
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts 2>/dev/null
telnetd -l /bin/sh -p 2323
exec /vendor/com.nvt.idtv/aurora.elf
```

That is telnet-only. Key-based dropbear needs root's home and
`authorized_keys` prepared as the tracked hook does (`/tmp/tvh_root`, passwd
bind); do not assume that copying `authorized_keys` alone is enough. If the
hook's `id` is not uid 0, stop: a different firmware may drop the idtv
privileges, and binding port 2323/starting dropbear will not work.

The tracked `openaurora/tools/boot/aurora-boot-hook.sh` is the full research
hook. If you use it:

- replace the hard-coded MAC `70:54:B4:FE:70:C6` with **your** TV's real MAC, or
  remove the MAC restore and let DHCP run. The vendor `run_aurora.sh` forces
  `D4:E2:CC:EC:45:E9` before its CIFS attempt; the full hook restores the real
  MAC afterward;
- it starts telnetd `2323`, tries dropbear `22`, dialserver and privacy blocks;
- `keepalive.sh` and `hosts` are local helpers, not shipped; if absent the hook
  logs and skips them, so telnetd/SSH still work but privacy/keepalive does not;
- missing optional files are logged/skipped, but telnetd is the root fallback;
- it only takes the native path if `/conf/oa_boot` exists; keep `oa_safe` so it
  resumes stock UI.

Generate your own SSH material (do not reuse the original set's keys):

```sh
ssh-keygen -t ed25519 -f owner_ed25519 -N ''
sudo dropbearkey -t rsa     -f hook/remotelf/hostkey_rsa
sudo dropbearkey -t ed25519 -f hook/remotelf/hostkey_ed25519
sudo cp owner_ed25519.pub hook/remotelf/authorized_keys
sudo chmod 600 hook/remotelf/hostkey_* hook/remotelf/authorized_keys
```

If you build dropbear for the TV, remember the TV is glibc 2.23/hard-float. Test
the binary on the TV (or build static); otherwise use the busybox telnetd path.
Preserve numeric owners (`0`, `101`, `105`, `110`) and executable bits; run the
image build as root or with a matching fakeroot.

Build the hook image exactly like the pristine one:

```sh
sudo mkfs.ubifs -r hook -m 4096 -e 253952 -c 31 -x zlib -o apdat0_hook.ubifs
sudo python3 - <<'EOF'
d = open('apdat0_hook.ubifs','rb').read()
assert len(d) <= 0x782000, len(d)
open('apdat0_hook.ubifs','wb').write(d + b'\xff' * (0x782000 - len(d)))
EOF
```

Verify the round trip before writing, including ownership and modes:

```sh
sudo rm -rf verify
sudo .venv/bin/ubireader_extract_files -k -o verify apdat0_hook.ubifs
sudo diff -r --no-dereference hook verify
sudo sh -c 'cd hook && find . -mindepth 1 -printf "%U:%G %m %y %p\n" | sort' > hook.meta
sudo sh -c 'cd verify && find . -mindepth 1 -printf "%U:%G %m %y %p\n" | sort' > verify.meta
diff hook.meta verify.meta
```

## 4. Write it with the USB apply script

On a FAT32 stick:

- `apdat0_full.ubifs` = your `apdat0_hook.ubifs` (the tracked apply script uses
  this name);
- `kylo_usb_update.scr` = `artifacts/scripts/apply_apdat0_hook.scr` (verify
  <8 KB and the final `%`);
- the script writes `0x782000` bytes to `ubi part apdat0` and reads it back to
  `apdat0_readback.bin`.

Power-cycle with OK held as in step 1. After the progress screen, wait ~2 min.
Copy `apdat0_readback.bin` back and compare its hash with the image; a mismatch
means restore immediately.

## 5. Boot and verify root

1. Boot normally. The hook runs as the idtv app; stock UI should return.
2. Find the TV's DHCP address (router or an ARP scan). Do **not** assume the
   original set's IP or MAC.
3. Root shell:
   - `telnet <ip> 2323` → `id` should be uid 0;
   - with the full hook's dropbear setup, `ssh -i owner_ed25519 root@<ip>`;
   - check `/conf/remote.txt`, `/conf/remotelf/aurora.elf`, and the listeners.
4. If the full hook is used, read `/buffer/tvhack-boot.log` (MAC/network,
   dropbear, telnetd, privacy).
5. Harden: firewall ports 22/2323 to your management host, use SSH keys, and
   disable telnet in your hook once SSH is proven. Never expose it to the
   internet.

## 6. Restore stock

Build/keep `apdat0_orig.ubifs` from your pristine tree. Put it and
`artifacts/scripts/restore_apdat0.scr` (renamed `kylo_usb_update.scr`) on the
FAT32 stick; power-cycle with OK held; wait; verify the stock UI and access.

Keep the dump, both images and the restore stick. A broken `/conf` image can
require this physical recovery; `/conf` is unsigned but not risk-free.

## What can differ

- **Firmware version:** the analysis addresses and hashes in this repo are for
  firmware 0.51.52.3. Another build may have different binaries; recover and
  re-verify from your own dump before reusing a contract.
- **NAND/UBI layout:** verify the 512 MiB dump, apdat0 offset/size, LEB size,
  count and volume size before writing. The tested layout is 31 × 253,952 B.
- **U-Boot USB numbering:** the scripts use `usb 0:1`; a different stick may be
  `usb 0:0`. Adjust and re-verify the script.
- **Rootfs glibc:** 2.23/hard-float; a newer cross toolchain can emit symbols
  the TV lacks. Check `readelf --version-info` and prefer static helpers when
  in doubt.

## Do not

- write fs0/ap0/ap1, kernel, bootloader or SPI;
- reuse the original set's payload image, dropbear, keys, MAC or settings DB;
- commit your dump, vendor libraries, keys or settings DB;
- leave telnetd open or the TV exposed to the internet.
