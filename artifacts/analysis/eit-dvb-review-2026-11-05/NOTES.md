# EIT demux review — 2026-11-05

Offline. No TV, `/dev/dvb`, frontend, demux or DVR device was opened.

Ghidra: `analyzeHeadless -readOnly -noanalysis` on
`artifacts/workbench/ghidra_ndal3` project `ndal3`, program `libNDAL3.so`,
MD5 `c842bad804302ec822076c3d9d3ce31d`, script
`openaurora/tools/re/DumpFirmware.java --asm`.

| symbol | address | instruction result |
|---|---|---|
| Hal_Demux_Dvb_Open | 4e7b9bcc | `open64` flags `0x80802`; returns fd or `-1` in r0 |
| Hal_Demux_Dvb_Close | 4e7b9c48 | `ioctl(fd, 0x6f2a)` then `close`; r0 is `close`'s result |
| Hal_Demux_Dvb_Set_Sectionfilter | 4e7ba12c | 7 args; flag byte nonzero stores flags 5; returns 0 or -1 |
| Hal_Demux_Dvb_Dvr_Open | 4e7b9f44 | `clz(mode)>>5`; mode 0 is `O_WRONLY` |
| Hal_Demux_Dvb_Dvr_Close | 4e7b9fc4 | `close` only |
| Hal_Demux_Dvb_Dvr_Write | 4e7ba000 | returns `write`'s r0 |
| Hal_Demux_Dvb_Dvr_Flush | 4e7b9e48 | ioctl `0xc0086f68`; returns 0 or -1 |

Tests after the fix: host `check-nim-stub` and `check-pvr-stub`; the same two
under `BUILD=build-eit-san` with ASan/UBSan; ARM glibc binaries in
`openaurora/build-tv-v3/eit-review` under bwrap/qemu. Each printed its ok line.
The full host suite and verify.cjs were not re-run.
