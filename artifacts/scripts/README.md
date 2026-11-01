# Script status — inspect before running

The TV is in use; only view-only access is currently authorized. This directory
contains both offline helpers and scripts that write flash. Their existence is
not authorization or evidence that their current combination was tested.

| script | effects and status |
|---|---|
| extract_from_dump.sh | local read/extract/cache only; preferred firmware source |
| rebuild_payload.sh | reads live /conf, uses sudo/local staging; --dry-run still reads TV and rebuilds local files; without it replaces local payload image/hook. Does not deploy the image to TV. Preserve verified recovery artifacts. |
| make_stick.sh | overwrites files on the supplied mount; uses apply_apdat0_hook.scr rather than the exact tested recovery-stability script; does not identify the USB device for you |
| make_nand_image.sh | builds a local merged NAND image using older raw apdat0; not current-protection recovery proof |
| flash_image.sh | host mode writes its target; TV mode detaches /conf, writes mtd13 and reboots. Live path unverified; default raw image older. |
| apply_apdat0_hook.scr | U-Boot writes UBIFS; not the exact 584-byte script used in successful latest recovery |
| restore_apdat0.scr | writes pristine /conf, removing root/access modifications |
| noop_write_apdat0.scr | actually writes original /conf; “noop” does not mean read-only |
| restore_full_nand.scr | last-resort full NAND write; requires correctly split source chunks |
| nand_dump.scr / spi_dump.scr | bootloader dump procedures requiring physical boot interruption; not a view-only query while TV is in use |

Use [the artifact map](../README.md) for exact image/script identities and
[STABILITY](../reports/STABILITY-2026-10-31.md) for recovery evidence.
Future physical recovery instructions are in ../recovery-stability/RECOVERY.txt.
Keep unsigned /conf writable recovery distinct from writing signed firmware.
