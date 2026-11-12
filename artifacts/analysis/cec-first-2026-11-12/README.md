# CEC first-opener evidence — 2026-11-12

- `aurora.elf.current` — original hook before the test, 8753 bytes,
  sha256 `c3943712d6962e009ff1a62e29fd59aa3ddf623e774384708dbec35ee9f7bebc`,
  md5 `0b1cd32a9de2b0251f6252e6603db8b7`.
- `aurora.elf.cec-first` — temporary hook with the CEC-first branch,
  9099 bytes, sha256
  `dea27cadc5845109293e0d7eea48e5c81b76d2660ec4fa2db712719c55f9ee7f`.
- `cec.log.boot1` — first boot log. It shows
  `cec raw transport open (/tmp/ntkcec)` and `open 1`, then three
  `failed errno 16` sends (owner job slot busy with receive polling).

The USB recovery stick was rebuilt clean (FAT32 TVHACK, only the tested
584-byte `kylo_usb_update.scr` MD5 `de1591298d633cf6422a1ade4e8835a9` and
`apdat0_full.ubifs` 7,872,512 bytes MD5
`994da7ff76a0b2fbafb8f6fa0c682627`). The old small logs/scripts were backed
up on the host at `/tmp/opencode/tvhack-usb-backup-20261112`; the canonical
NAND dump remains `artifacts/dumps/tv_nand.bin`.

After recovery the TV is stock: original hook restored, no CEC-first branch,
`/conf/oa_boot` absent, `/conf/oa_safe` present, `/buffer/oa-cec-first`
removed, no `openaurora`/`oa-wdtfeed`, `/tmp/ntkcec` absent.
