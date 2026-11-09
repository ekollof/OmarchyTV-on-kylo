# Script status — inspect before running

The TV is in use; only view-only access is currently authorized. This directory
contains both offline helpers and scripts that write flash. Their existence is
not authorization or evidence that their current combination was tested.

| script | effects and status |
|---|---|
| extract_from_dump.sh | local read/extract/cache only; preferred firmware source |
| rebuild_payload.sh | **original host only**: reads live `/conf` through `tools/tvssh.sh`, stages in `/tmp/opencode`, and writes `artifacts/payload/`; not the first-root path. For another TV use [docs/reproduce/root-payload.md](../../docs/reproduce/root-payload.md). |
| make_stick.sh | **original host only**: reads `artifacts/payload`, uses `apply_apdat0_hook.scr` and writes the original IP/MAC into its README; not a fresh-owner tool. |
| make_nand_image.sh | builds a local merged NAND image using older raw apdat0; not current-protection recovery proof |
| flash_image.sh | host mode writes its target; TV mode detaches /conf, writes mtd13 and reboots. Live path unverified; default raw image older. Not in the reproduce path. |
| apply_apdat0_hook.scr | 549-byte U-Boot form; an earlier attempt did not complete and the cause is not established. The 584-byte `../recovery-stability/kylo_usb_update.scr` (MD5 `de1591298d633cf6422a1ade4e8835a9`) is the tested first-write script. |
| restore_apdat0.scr | 392-byte pristine `/conf` write with no readback; not the tested 584-byte recovery form. Use the tested script with the pristine image renamed `apdat0_full.ubifs`. |
| noop_write_apdat0.scr | actually writes original /conf; “noop” does not mean read-only |
| restore_full_nand.scr | last-resort full NAND write; requires correctly split source chunks. Not in the reproduce path. |
| nand_dump.scr / spi_dump.scr | bootloader dump procedures requiring physical boot interruption; not a view-only query while TV is in use |

Use [the artifact map](../README.md) for exact image/script identities and
[STABILITY](../reports/STABILITY-2026-10-31.md) for recovery evidence.
For a first-time owner on the same hardware, start with
[docs/reproduce/README.md](../../docs/reproduce/README.md): it covers the NAND
dump, building your own `/conf` image, the USB write and restore.
Future physical recovery instructions are in ../recovery-stability/RECOVERY.txt.
That file documents the **original set's** tested image, IP `192.168.178.87` and
MAC `70:54:b4:fe:70:c6`; do not reuse the image, keys, address or MAC on another
TV. Build your own with the reproduce guide.
Keep unsigned /conf writable recovery distinct from writing signed firmware.
