# CEC first-opener evidence — 2026-11-12

- `aurora.elf.current` — original hook before the test, 8753 bytes,
  sha256 `c3943712d6962e009ff1a62e29fd59aa3ddf623e774384708dbec35ee9f7bebc`,
  md5 `0b1cd32a9de2b0251f6252e6603db8b7`.
- `aurora.elf.cec-first` — first temporary hook, 9099 bytes, sha256
  `dea27cadc5845109293e0d7eea48e5c81b76d2660ec4fa2db712719c55f9ee7f`.
- `aurora.elf.cec-first-v2` — second hook, sha256
  `96399f3c9c45c468447b58d354aa7850c5a85b333d539a65a8c6a46bcc35795b`
  (30 s hard-kill timeout).
- `aurora.elf.cec-first-v3` — third hook, sha256
  `afbf1656b73b01eefd09633b8c2d89828c86c06c3cb69a6463e19f9c46b5f2b8`
  (40 s hard-kill timeout; used for boots 4–5).
- `run-cec-first-v2.sh` — first hardened harness, sha256
  `41f140e80c01d5881fe33b56fc5b84e5184bc82e78ce69661c7c92f7f858a310`.
- `run-cec-first-v3.sh` — LA8/LA4/broadcast harness, sha256
  `0bf3174e1bee0d71bb3c57cfd9ef99d68c18bebf5285329360f2e021e9c1b13d`.
- `run-cec-first-v4.sh` — TV-announcement + LA4 query harness, sha256
  `7d498a3f318006c6139c4133c843b4c2118ecad6853855e43edfebaa0f10c9b2`.
- `cec.log.boot1` — raw open succeeded, three `EBUSY` sends.
- `cec.log.boot2-wrongclient` — harness passed `0883` as a separate command;
  no frame was sent.
- `cec.log.boot3-tx512` — queued `0883` (`send ok`) then `cec tx failed (512)`.
- `cec.log.boot4-la4-ack` — LA8 NACK; LA4 and broadcast ACK.
- `cec.log.boot5-la4-queries` — TV announcement, LA4 poll and 83/46/8f;
  all five ACKed, zero RX frames.
- `dmesg.boot4.txt`, `dmesg.boot5.txt` — driver logs for those boots.
- `nondovi-cec-isr.txt` — read-only Ghidra dump of
  `KER_CEC_InterrutHandler` @ `0xd9064`: TX status 0 on INT bit 3, status 2 on
  INT bit 2, then completion.
- `nondovi-cec-tx.txt` — `_CEC_TX_Data` @ `0xda3b0` and `_CEC_CheckSFT`.
- `nondovi-refs-aea324.txt` — `DAT_00aea324` write/data references used to
  locate the interrupt handler.
The driver project is `artifacts/workbench/ghidra_nondovi` /
`ntkdriver_nondovi.ko`, MD5 `fded7005651e37b8a1b5e8e89f73f4ff`.

The retry module was `mod_cec.so` sha256
`f3337e294f182c55957904972a5e30c2b5bad2238e3a605f5b2a99c34671fead`
(TX queue commit `b9269fc`).

The USB recovery stick was rebuilt clean (FAT32 TVHACK, only the tested
584-byte `kylo_usb_update.scr` MD5 `de1591298d633cf6422a1ade4e8835a9` and
`apdat0_full.ubifs` 7,872,512 bytes MD5
`994da7ff76a0b2fbafb8f6fa0c682627`). The old small logs/scripts were backed
up on the host at `/tmp/opencode/tvhack-usb-backup-20261112`; the canonical
NAND dump remains `artifacts/dumps/tv_nand.bin`.

After boot 5 the original hook was restored, `/buffer/oa-cec-first` removed,
and a clean reboot verified stock: original hook `c3943712…`, no CEC-first
branch, `/conf/oa_boot` absent, `/conf/oa_safe` present, stock
`appmgr` 1569 / `run_aurora.sh` 1726 / `aurora.elf` 1773 / `dialserver` 1864,
kernel thread `DRV_CEC_CALLBAC` 2855, no `openaurora`/`oa-wdtfeed`, and
`/tmp/ntkcec` absent.
