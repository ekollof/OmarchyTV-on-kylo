# TV payload — what is installed and how to rebuild it

The TV runs a root dropbear sshd at boot with no external dependency.
Everything lives in the **`apdat0`** UBI volume (`/conf`), which is *not*
signature-verified. The signed `fs0`/`ap0`/`ap1` volumes, kernels, U-Boot and
SPI are untouched.

**2026-10-31:** the live TV and current UBIFS image include SSH/telnet maintenance,
privacy/network repair and `oa_safe` to force the stock UI. Owner restored the
current image successfully, and access/protections were verified afterward.
The raw mtd13 image remains older. Use the exact old 584-byte script in
`../recovery-stability/` for the stick; see `../reports/STABILITY-2026-10-31.md`.

## Files here

| file | role | md5 |
|---|---|---|
| `apdat0_mtd13.img` | **older raw 10 MB mtd13 image**; predates current maintenance/protections | `875804842f4bb6fd71dd2c6097080edd` |
| `apdat0_full.ubifs` | current verified recovery image: working hook, keepalive, keys, sqlite/privacy DB, stock UI pinned | `994da7ff76a0b2fbafb8f6fa0c682627` |
| `apdat0_orig.ubifs` | pristine volume image, pre-hack (revert) | `fde74f196b11740de528735e49bcc976` |
| `aurora.elf` | current working boot hook (shell script, runs as root) | `0b1cd32a9de2b0251f6252e6603db8b7` |
| `keepalive.sh` | SSH/telnet, privacy block/settings and Ethernet/DHCP maintenance | `4ebd0f9d7ee9f470afde8b0f2807522b` |
| `authorized_keys` | ssh keys accepted by dropbear | `0d11cf71a7e3054cb1359d62f9033cee` |
| `dropbear` | static musl ARM sshd (313 KB) | `c2405a04794bdfc3b9daf2a8089cf99b` |
| `hostkey_rsa`, `hostkey_ed25519` | ssh host keys | |
| `remote.txt` | `0:127.0.0.1:x:x` — forces debug ini + instant CIFS failure | |

## Boot chain

1. `/conf/remote.txt` exists → appmgr loads `nvt.app.debug.ini`
   (`enableMultiuser=n` ⇒ apps run as **root**).
2. Its `idtv` program is `/mtd_apexe/board/run_aurora.sh`.
3. That mounts `//127.0.0.1/mb230cifs` → fails instantly → runs the **local**
   `/conf/remotelf/aurora.elf`.
4. The hook restores the real MAC, renews DHCP, mounts devpts, bind-mounts a
   patched `/etc/passwd` (root home `/tmp/tvh_root`), starts dropbear:22 and
   telnetd:2323, starts `com.nvt.dialserver` standalone (restores DIAL on
   56789/56790; kill switch `/conf/no_dial`), and — when `/conf/launcher_enable`
   exists — serves the web launcher from `/buffer/launcher` (busybox httpd on 8181)
   and opens it once the portal is up. Failure streak of 3 deletes the flag, so a
   broken page can never lock the TV out. Logs to `/conf/tvhack.log`, then execs the
   real UI.

## Rebuild the image

The image shipped here was built from a **live `/conf` snapshot** (so it matches
what actually runs), not from `apdat0_orig.ubifs`:

```sh
tools/tvssh.sh 'tar -C /conf -cf - .' | sudo tar -xpf - -C /tmp/opencode/apdat_files
sudo rm -f /tmp/opencode/apdat_files/tvhack.log*   # log noise, not part of the payload
sqlite3 /tmp/opencode/apdat_files/sqlite_db/settingsDB.db 'pragma integrity_check;'
sudo mkfs.ubifs -r /tmp/opencode/apdat_files -m 4096 -e 253952 -c 31 -x zlib -o apdat0_new.ubifs
# pad to 31 LEBs (31*253952 = 7872512) with 0xFF, then verify:
venv/bin/ubireader_extract_files -o verify_img apdat0_full.ubifs && diff -r apdat_files verify_img
```

The image ships **without** the launcher runtime state (`launcher_enable`,
`launcher_ok`, `launcher_fail`) — those are per-device flags. Set
`KEEP_LAUNCHER_FLAG=1` to keep the flag on purpose.

Numeric owners matter (app files are uid/gid **101**, root files **0**) — extract
and pack with `sudo`/`tar -p` so they survive.

Automated: `artifacts/scripts/rebuild_payload.sh` (`--dry-run` to only build+verify).
Because the snapshot is taken from a running TV, the md5 changes on every rebuild
(SQLite DB state, timestamps) — the md5 above identifies *this* image only; what
matters is the hook, dropbear, keys and the privacy DB it carries.

Legacy variant (from the pristine volume):

```sh
# apdat_files = extraction of apdat0_orig.ubifs, plus remote.txt and remotelf/
sudo mkfs.ubifs -r apdat_files -m 4096 -e 253952 -c 31 -x zlib -o apdat0_new.ubifs
python3 - <<'EOF'
d=open('apdat0_new.ubifs','rb').read(); full=31*253952
assert len(d)<=full
open('apdat0_full.ubifs','wb').write(d+b'\xff'*(full-len(d)))
EOF
```

## Flash with dd (no stick needed)

The apdat0 partition is `mtd13`: **size 0x00A00000 (10 MB, 40 x 256 KB PEBs), NAND offset 0x0E340000**.
`apdat0_mtd13.img` is that partition, dumped from the TV *with the payload installed* — a UBI image, so
it carries its own erase counters and volume table.

**A. From the TV itself (root, no hardware):**

```sh
artifacts/scripts/flash_image.sh tv artifacts/payload/apdat0_mtd13.img --yes
```
It copies the image, verifies the size/md5, `ubidetach`s UBI 13, `dd`s to `/dev/mtd13` and reboots.
`/conf` is gone between detach and reboot — that is expected. *Not yet exercised end to end.*

**B. With a programmer / any dd target:**

```sh
# full 512 MB NAND = pristine dump with only apdat0 replaced (everything else byte-identical)
artifacts/scripts/make_nand_image.sh /tmp/tv_nand_hacked.bin
# or write just the partition at its offset:
artifacts/scripts/flash_image.sh host artifacts/payload/apdat0_mtd13.img <file-or-device> 0x0E340000
```

**C. The U-Boot stick path** (`make_stick.sh`) still works and is the fallback if the TV never comes
back: hold OK at power-on, it runs `kylo_usb_update.scr`.

## Flash / restore

FAT32 stick, script must be named `kylo_usb_update.scr` and end with `%`
(< 8 KB). TV off → stick in → hold **OK** → reconnect power.

- flash this payload: `artifacts/scripts/apply_apdat0_hook.scr`
- revert to stock `/conf`: `artifacts/scripts/restore_apdat0.scr`
- full NAND restore (last resort): `artifacts/scripts/restore_full_nand.scr`
  with `n00..n63.bin` from `artifacts/dumps/`

The no-op variant (`noop_write_apdat0.scr`) writes the original image back and
was used to validate the `ubi part`/`ubi write`/read-back path.

## Privacy / telemetry

`apdat0_full.ubifs` ships a `settingsDB.db` with:

```
m_privacy_internet_connection_en  = 1   (must stay 1, or the TV disables its network)
m_privacy_advertising_enabled     = 0
m_privacy_personalized_advertisi  = 0
m_privacy_marketing_and_profilin  = 0
m_privacy_3p_data_share_for_adv_  = 0
m_logging_enabled                 = 0   (Vestel plog telemetry)
m_samba_acr_enabled               = 0
m_do_not_track_enabled            = 1
```

`hosts` is bind-mounted over `/etc/hosts` by the hook to blackhole the telemetry,
portal, search, ACR and assistant endpoints (list inside the file).

## On-device sqlite3 + network self-heal

`/conf/sqlite3` (static musl ARM, sqlite 3.54.1, 1.35 MB) is installed so the settings
database can be edited on the TV. The boot hook uses it to force
`m_privacy_internet_connection_en = 1` before aurora starts, and runs an eth0 watchdog
(re-up + udhcpc every 15 s if the interface loses its lease). Disable with
`touch /conf/no_force_net`.

Verified 2026-10-30: set the flag to 0, reboot → hook restored it to 1 and the TV came
back online by itself.
