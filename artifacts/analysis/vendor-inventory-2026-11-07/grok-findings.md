# Vendor inventory notes, 2026-11-07

Independent read of the on-disk vendor binaries. No TV access, no writes under the firmware trees, and no Ghidra project was opened. `artifacts/workbench/ghidra_proj` (Aurora) was left alone.

Constants come from `nm -D`, `readelf`, `strings`, `modinfo`, the existing caches `artifacts/workbench/aurora.dis` and `artifacts/workbench/ndal3.dis`, and `artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-objdump`. Host `/usr/bin/objdump` reports `architecture UNKNOWN` on these ARM files. An ioctl number is the `r1` value at the `bl`/`blx` to `ioctl`: `movw` or `mov.w` supplies the low 16 bits and `movt` supplies the high 16. The decode column uses the Linux `_IOC` layout: bits 31..30 direction, 29..16 size, 15..8 type, 7..0 number.

## Contradictions

> **Resolution in the main inventory (2026-11-07):** (1) `vpltio-command-map.tsv`
> is now populated: 45 call sites / 39 unique commands. (2) The older Ghidra
> scan is retained as `vpltio-sendcmd-r0-ghidra-superseded.tsv`; the objdump TSV
> is canonical and has `0x1c8`. (3) The two caller names are patched to the
> unnamed functions `8f682c` and `92e8ac`. (4) The 46th `@plt` reference is the
> stub and is not counted as a call. (5) `gpio-function-names.tsv` is truncated
> to IDs 0..207; 208 is `TOTAL` and later slots are not 24-byte pin records.
> (6) The census is extended to the whole rootfs: 461 ELFs, including 15 kernel
> modules (9 not yet in Ghidra). The findings below are the independent read
> that drove those corrections.

1. `calls/vpltio-command-map.tsv` is 0 bytes. The "45/45" line is `calls/vpltio-sendcmd-r0-objdump.stats` (`calls=45 known=45`). That is 45 call sites, not 45 distinct commands. `aurora.dis` has 45 `blx` instructions to `Hal_Video_Vpltio_SendCmd@plt`, the same 45 sites, using 39 distinct `r0` immediates.

2. `calls/vpltio-sendcmd-r0.tsv` has 46 rows. `006b3d48` is the PLT stub (`adr` / `add`), not a command. Fourteen rows are `UNKNOWN` even though the context contains `mov.w r0, #imm`. One immediate is wrong: `plat_vpltio_set_lvds_clockdata_delay` at `0092e3da` is recorded as `0x0`. The bytes are `0092e3c0 movs r0, #0` and then `0092e3d4 mov.w r0, #456` (`0x1c8`). `r0` at the call is `0x1c8`. The objdump TSV already has `0x1c8`.

3. Two caller names in the objdump TSV are the previous global symbol. The call is outside that symbol's `readelf` size.
   - `008f6d90` is not in `PLAT_VLIB_EnableMsg` (`008f67ed`, size 64, ends `008f682d`). The next global is `PLAT_VOUT_Init` at `008f7aa1`. A prologue starts at `008f682c` (`mov.w ip, #16384`; `push {r7, lr}`). The immediate `008f6d6c mov.w r0, #396` (`0x18c`) is right. The other TSV leaves this caller empty.
   - `0092e918` is not in `plat_set_dimming` (`0092e669`, size 580, ends `0092e8ad`). Prologue at `0092e8ac` (`mov.w ip, #16384`; `stmdb sp!, {r4, r5, r6, r7, r8, lr}`). The other TSV names this `FUN_0092e8ac`. Command `0092e906 movw r0, #1220` (`0x4c4`) is right. The next call in that function is `Hal_Vin_SelectInput` at `0092e920`.

4. `calls/aurora-plt-call-counts.txt` lists 46 references of `Hal_Video_Vpltio_SendCmd`. The disassembly has 45 direct calls. The extra reference is the same PLT stub the 46-row TSV includes.

5. GPIO ids 0..207 match `gpio-function-names.tsv` as 24-byte records. The TSV continues past that table. Id 222 is written as `à`. The record at `0x1760820` begins `e0 00 0f fd c8 00 0f fd`, so `0xE0` is a data byte, not U+00E0. Slot 208 is the ASCII sentinel `TOTAL`, not a pin function. Slots whose first byte is printable and whose second byte is NUL (` ` at 216 and 224, `` ` `` at 218 and 226, `R` at 241, `D` at 242) match the file and are still not function names. The TSV skips non-string slots 210, 212, 214 and 220.

6. The 389-ELF census includes `ntkethmac.ko` only. The other six modules live under `root/lib/modules` and `vendor2/modules`. Hashes below are of those rootfs copies.

These shared VPLTIO immediates are one command from two callers, not a map error: `0x4c1` twice inside `PLAT_TVOUT_EnableTVOut`; `0x2c2` from `PLAT_VIN_SetVDChannelUpDnInit` and `plat_vpltio_inform_vd_do_ch_updown_init`; `0x18c` from the unnamed function at `008f682c` and from `PLAT_VOUT_SetWindowBGcolor`; `0x1c9` from `PLAT_VOUT_SetPanelLvdsDriving` and `plat_vpltio_set_lvds_strength`; `0x1ca` from `PLAT_VOUT_SetPanelLvdsPreEmphasis` and `plat_vpltio_set_lvds_preemphasis`; `0x152` from `PLAT_VOUT_SetOverDrive` and `PLAT_VOUT_LoadOverDriveTable`.

## Binaries

| File | MD5 | Size | ELF |
|---|---|---|---|
| `artifacts/workbench/rootfs/vendor/lib/libNDAL3.so` | `c842bad804302ec822076c3d9d3ce31d` | 388332 | ELF32 ARM DYN, soname `libNDAL3.so`, BuildID `a86d04af847c6e449089f95cdf90d579c1e9cdc4` |
| `artifacts/workbench/rootfs/vendor/lib/libNDAL_EX.so` | `e6a465861d1869b8b138b3f299c89f87` | 197788 | ELF32 ARM DYN, soname `libNDAL_EX.so`, BuildID `8dc7c72e6bdc82bab81223b93462bb51d9e2596e` |
| `artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf` | `64c252340d3af6c5d66072611b7bad7a` | 29259744 | ELF32 ARM EXEC, BuildID `4a16c2b5be5987c079a744b7323621d5ee0fd243`, stripped |
| `rootfs/root/lib/modules/ntkndpdrm.ko` | `a70aca9b814e51cb9680476ce27051de` | 926996 | ET_REL, vermagic `4.9.118+ SMP preempt mod_unload ARMv7` |
| `rootfs/root/lib/modules/ntkndpextin.ko` | `cb0689e9e1c425a27e670168e51cc32f` | 189820 | ET_REL, same vermagic |
| `rootfs/vendor2/modules/ntkdriver_nondovi.ko` | `fded7005651e37b8a1b5e8e89f73f4ff` | 18423496 | ET_REL, same vermagic |
| `rootfs/root/lib/modules/ntkndpalsa.ko` | `8758a13d12799217ce702849df7d54da` | 716516 | ET_REL, same vermagic |
| `rootfs/vendor/modules/ntkethmac.ko` | `2efe1ee138ee29c2d490f6e4a667a38d` | 83444 | ET_REL, same vermagic |
| `rootfs/root/lib/modules/PreInit.ko` | `1e50bee8b7888315c7a070f8e69947f6` | 721732 | ET_REL, same vermagic |
| `rootfs/root/lib/modules/ntkndphdmiswitch.ko` | `d4ff737dcbd9be2b628f2dd4eccf5d7a` | 27624 | ET_REL, same vermagic |

Paths are under `artifacts/workbench/`. `ndal3.dis` uses link base `0x4e7a0000` (`ioctl@plt` at `4e7ab600`). The NDAL_EX disassembly uses link base `0x4db50000` (`ioctl@plt` at `4db56698`). Those are ELF VMAs.

`libNDAL3.so` NEEDED: `libstdc++.so.6`, `libm.so.6`, `libgcc_s.so.1`, `libpthread.so.0`, `libc.so.6`, `ld-linux-armhf.so.3`. Dynamic definitions: 517 functions, 22 `D`, 22 `B`.

`libNDAL_EX.so` NEEDED: `libnteec.so`, `libmhash.so.2`, `libNDAL3.so`, plus the same C++/pthread/libc set. Dynamic definitions: 330 functions, 19 `B`, 4 `D`, 2 `R`.

## libNDAL3.so exports by prefix

| count | prefix | first | last |
|---|---|---|---|
| 62 | `Hal_Video` | 4e7c2f49 | 4e7c85fd |
| 58 | `Hal_Demux` | 4e7b6119 | 4e7ba261 |
| 54 | `Hal_OS` | 4e7bc005 | 4e7be2f1 |
| 48 | `Hal_PQ` | 4e7bf605 | 4e7c0b59 |
| 32 | `sh` | 4e7ba4d9 | 4e7c1ce5 |
| 30 | `` | 4e7aaf8c | 4e7d0400 |
| 30 | `compress` | 4e7b0c99 | 4e7b21bd |
| 30 | `Hal_Audio` | 4e7ac929 | 4e7b03ed |
| 20 | `venc` | 4e7bb031 | 4e7bbabd |
| 15 | `HAL_DEMOD` | 4e7b55dd | 4e7b5f25 |
| 13 | `Hal_VideoDec` | 4e7c8759 | 4e7c95f1 |
| 11 | `Hal_Vin` | 4e7c97e9 | 4e7c9d59 |
| 10 | `Hal_Gpio` | 4e7be3ed | 4e7bee15 |
| 9 | `HAL_NIPC` | 4e7ac2d1 | 4e7ac8b9 |
| 7 | `HAL_Demod` | 4e7b2cd9 | 4e7b44c1 |
| 6 | `HalAudioEnc` | 4e7acde5 | 4e7ad4f9 |
| 6 | `Hal_Debug` | 4e7b2501 | 4e7b299d |
| 6 | `Hal_Pwm` | 4e7c0c39 | 4e7c1109 |
| 6 | `Hal_SPI` | 4e7bf2f9 | 4e7bf54d |
| 5 | `HAL_fe` | 4e7b33f5 | 4e7b43b1 |
| 5 | `Hal_IfCmd` | 4e7ca16d | 4e7ca3b9 |
| 4 | `DRV` | 4e7caec1 | 4e7cb6f1 |
| 4 | `HalDemod` | 4e7b32dd | 4e7b5341 |
| 3 | `Hal_Iic` | 4e7bee99 | 4e7bf2b1 |
| 3 | `Hal_Menu` | 4e7c9dc1 | 4e7d0229 |
| 3 | `Hal_Userdata` | 4e7c1d49 | 4e7c1ff9 |
| 3 | `is` | 4e7b0cd5 | 4e7b18d5 |
| 1 | `AUD` | 4e7ca46d | 4e7ca46d |
| 1 | `calc` | 4e7cba31 | 4e7cba31 |
| 1 | `DBE` | 4e7cb989 | 4e7cb989 |
| 1 | `HAL_AllocBuffer` | 4e7b0761 | 4e7b0761 |
| 1 | `hal` | 4e7ad5f1 | 4e7ad5f1 |
| 1 | `HAL_dvif` | 4e7b3351 | 4e7b3351 |
| 1 | `HAL_FreeBuffer` | 4e7b0bc1 | 4e7b0bc1 |
| 1 | `HAL_Handle2IPCName` | 4e7b09b9 | 4e7b09b9 |
| 1 | `HAL_Handle2PhyAdr` | 4e7b0a55 | 4e7b0a55 |
| 1 | `HAL_Handle2VirAdr` | 4e7b08c1 | 4e7b08c1 |
| 1 | `HAL_IPCName2Handle` | 4e7b0a9d | 4e7b0a9d |
| 1 | `Hal_Peripheral` | 4e7be3b5 | 4e7be3b5 |
| 1 | `Hal_ReadDecimalKey` | 4e7b27f9 | 4e7b27f9 |
| 1 | `Hal_UserData` | 4e7c26b1 | 4e7c26b1 |
| 1 | `my` | 4e7cab79 | 4e7cab79 |
| 1 | `SD` | 4e7ca42d | 4e7ca42d |
| 1 | `AENC_GetContexth` | 4e7acd85 | 4e7acd85 |
| 1 | `sh_drv_tsmux_Termv` | 4e7c1af5 | 4e7c1af5 |
| 1 | `sh_drv_venc_ThreadP` | 4e7ba2fd | 4e7ba2fd |
| 1 | `DRV_VECN_Callback_p` | 4e7ba29d | 4e7ba29d |
| 1 | `HalAudioEnc_Write_BSPci` | 4e7ace65 | 4e7ace65 |
| 1 | `Hal_OS_MutexCondInitPvi` | 4e7bd30d | 4e7bd30d |
| 1 | `Hal_OS_MutexCondWaitPvj` | 4e7bd03d | 4e7bd03d |
| 1 | `Hal_OS_SemaphoreInitPvi` | 4e7bd4b5 | 4e7bd4b5 |
| 1 | `Hal_OS_MutexCondCreatjPKc` | 4e7bcf7d | 4e7bcf7d |
| 1 | `Hal_OS_MutexCondSignalPv` | 4e7bd2a1 | 4e7bd2a1 |
| 1 | `Hal_OS_MutexCondDestroyPv` | 4e7bcfe9 | 4e7bcfe9 |
| 1 | `sh_drv_venc_Get_Contexti` | 4e7ba491 | 4e7ba491 |
| 1 | `Hal_OS_MutexCondGetValuePv` | 4e7bd375 | 4e7bd375 |
| 1 | `sh_drv_venc_Tasecase_Stopv` | 4e7bafa9 | 4e7bafa9 |
| 1 | `SD_SHELL_OS_IntTaskDestroyPvPFimE` | 4e7bc831 | 4e7bc831 |
| 1 | `sh_drv_venc_Tasecase_Startv` | 4e7bae91 | 4e7bae91 |
| 1 | `SD_SHELL_OS_ConvertDateTimeToSecP` | 4e7bc2a1 | 4e7bc2a1 |
| 1 | `SD_SHELL_OS_ConvertSecToDateTimeStringPaj` | 4e7bc3c1 | 4e7bc3c1 |

## libNDAL_EX.so exports by prefix

C++ prefixes are the Itanium identifier up to the next length digit. Full mangled names are in the appendix.

| count | prefix | first | last |
|---|---|---|---|
| 65 | `NDAL_VideoExt` | 4db5871d | 4db5b045 |
| 30 | `NDALex_GPU` | 4db5b2a5 | 4db5d539 |
| 27 | `GPU_SYS` | 4db63875 | 4db64569 |
| 26 | `GPU_CMD` | 4db5d585 | 4db5f7dd |
| 22 | `Nd_NIM` | 4db6725d | 4db68069 |
| 20 | `` | 4db56288 | 4db6ae38 |
| 17 | `GPU_HW` | 4db5f8e5 | 4db63761 |
| 14 | `NDFW` | 4db65e85 | 4db66b75 |
| 13 | `ND_SYS` | 4db687b1 | 4db69519 |
| 11 | `NDAL_VenderVideoExt` | 4db57c35 | 4db58409 |
| 9 | `ND_TSPU` | 4db696f1 | 4db69cf5 |
| 5 | `NDSCALER_VIN` | 4db68289 | 4db68495 |
| 4 | `NDAL_CommonExt` | 4db56dc1 | 4db5707d |
| 4 | `ndcommon` | 4db66be5 | 4db66cdd |
| 3 | `ND_CommonExt` | 4db570d1 | 4db57225 |
| 3 | `NDSYS_SYS` | 4db68d75 | 4db69345 |
| 2 | `GPU_Human` | 4db646ed | 4db647a9 |
| 1 | `GPU_Dump` | 4db64641 | 4db64641 |
| 1 | `HAL_ClearSplashLogo` | 4db64d55 | 4db64d55 |
| 1 | `HAL_CreateBuffer` | 4db6511d | 4db6511d |
| 1 | `HAL_ReleaseBuffer` | 4db64e49 | 4db64e49 |
| 1 | `HAL_RenderBuffer` | 4db64fa9 | 4db64fa9 |
| 1 | `HAL_ShowSplashLogo` | 4db64c3d | 4db64c3d |
| 1 | `HAL_TestCreateBuffer` | 4db64b0d | 4db64b0d |
| 1 | `HAL_TestReleaseBuffer` | 4db648d9 | 4db648d9 |
| 1 | `HAL_TestRenderBuffer` | 4db649c5 | 4db649c5 |
| 1 | `NDOSD_EnableProtectionArea` | 4db69eb1 | 4db69eb1 |
| 1 | `NDOSD_GetAlphaCount` | 4db69d31 | 4db69d31 |
| 1 | `NDOSD_GetAutomuteStatus` | 4db6a9b9 | 4db6a9b9 |
| 1 | `NDOSD_GetColorMode` | 4db6a221 | 4db6a221 |
| 1 | `NDOSD_GetCurrentBufferOrder` | 4db6a8c5 | 4db6a8c5 |
| 1 | `NDOSD_GetDispWin` | 4db6a5fd | 4db6a5fd |
| 1 | `NDOSD_GetPhyBufferAddress` | 4db6a049 | 4db6a049 |
| 1 | `NDOSD_GetPlaneOrder` | 4db6ad5d | 4db6ad5d |
| 1 | `NDOSD_GetStride` | 4db6a3f9 | 4db6a3f9 |
| 1 | `NDOSD_GetStrLogoPhyAddrFromCMABuffer` | 4db6aaad | 4db6aaad |
| 1 | `NDOSD_ReleaseCMABuffer` | 4db6aba1 | 4db6aba1 |
| 1 | `NDOSD_SetAutomuteTrigger` | 4db69e25 | 4db69e25 |
| 1 | `NDOSD_SetColorMode` | 4db6a13d | 4db6a13d |
| 1 | `NDOSD_SetDispWin` | 4db6a4ed | 4db6a4ed |
| 1 | `NDOSD_SetForceBufferOrder` | 4db6a7e1 | 4db6a7e1 |
| 1 | `NDOSD_SetPhyBufferAddress` | 4db69f55 | 4db69f55 |
| 1 | `NDOSD_SetPlaneOrder` | 4db6ac91 | 4db6ac91 |
| 1 | `NDOSD_SetStride` | 4db6a315 | 4db6a315 |
| 1 | `NDOSD_SetUpdateEnable` | 4db6a715 | 4db6a715 |
| 1 | `NDSCALER_BRIG` | 4db6851d | 4db6851d |
| 1 | `nvtfw` | 4db65c79 | 4db65c79 |
| 1 | `system` | 4db65975 | 4db65975 |
| 1 | `_ksys_openv` | 4db56ed9 | 4db56ed9 |
| 1 | `_ksys_closei` | 4db56f41 | 4db56f41 |
| 1 | `NdCP_GetPRNGPh` | 4db66dd5 | 4db66dd5 |
| 1 | `NdExtCP_Initv` | 4db66d21 | 4db66d21 |
| 1 | `NdExtCP_Decrypt` | 4db66d99 | 4db66d99 |
| 1 | `NdExtCP_Encrypt` | 4db66d5d | 4db66d5d |
| 1 | `_SYS_GetDevNodeFIDPKcii` | 4db69381 | 4db69381 |
| 1 | `_ksys_get_hw_versioni` | 4db56fa5 | 4db56fa5 |
| 1 | `NDOSD_EnableOSDEngineb` | 4db68199 | 4db68199 |
| 1 | `NDOSD_SetTransparencyhj` | 4db680f5 | 4db680f5 |
| 1 | `NDAL_CommonExt_GetCPOPPj` | 4db56e19 | 4db56e19 |
| 1 | `NDAL_CommonExt_GetCAUIDPj` | 4db56d3d | 4db56d3d |
| 1 | `NDAL_GfxExt_SetPreAlpha` | 4db577d5 | 4db577d5 |
| 1 | `NDAL_GfxExt_SET_CROP_WIN` | 4db5751d | 4db5751d |
| 1 | `NDAL_GfxExt_GetAlphaCounthPj` | 4db57665 | 4db57665 |
| 1 | `NDAL_SoundExt_SetPCMParam` | 4db57a89 | 4db57a89 |
| 1 | `NDAL_SoundExt_DmixSoundOut` | 4db57b19 | 4db57b19 |
| 1 | `NDAL_GfxExt_EnableOSDEngineb` | 4db573bd | 4db573bd |
| 1 | `NDAL_GfxExt_SetTransparency` | 4db57319 | 4db57319 |
| 1 | `NDAL_GfxExt_EnableProtectAreab` | 4db57449 | 4db57449 |
| 1 | `NDAL_GfxExt_SetOSD` | 4db57749 | 4db57749 |
| 1 | `NDAL_SoundExt_OpenDmixChannel` | 4db579f9 | 4db579f9 |
| 1 | `NDAL_SoundExt_CloseDmixChannel` | 4db57bd9 | 4db57bd9 |
| 1 | `NDAL_SoundExt_SetDmixChannelVol` | 4db57879 | 4db57879 |

## What aurora calls

`aurora.elf` has 340 undefined symbols whose names start with `Hal_`, `HAL_`, `NDAL_`, `ND_`, or `NDOSD_`. 228 are defined by `libNDAL3.so` and 112 by `libNDAL_EX.so`. Every one of those 340 names is in `calls/aurora-plt-call-counts.txt`. The sums of those counts are 1328 calls into NDAL3 exports and 339 into NDAL_EX exports.

Largest NDAL3 groups aurora calls: `Hal_Demux` 46, `Hal_Video` 45, `Hal_PQ` 42, `Hal_OS` 17, `HAL_DEMOD` 13, `Hal_Audio` 13, `Hal_VideoDec` 12, `Hal_Vin` 11, `Hal_Gpio` 8, `HAL_Demod` 5, `Hal_Pwm` 3, `Hal_Iic` 3. Largest NDAL_EX groups: `NDAL_VideoExt` 57, `ND_TSPU` 9, `NDAL_SoundExt` 5, `NDAL_VenderVideoExt` 4, `NDAL_GfxExt` 4, `ND_SYS` 3, `ND_CommonExt` 3, plus the `NDOSD_*` plane calls and `NDALex_GPU_Open`, `NDALex_GPU_Close`, `NDALex_GPU_Scale`.

## Device paths

`strings -a` on `libNDAL3.so`: `/proc/tdp_alsa`, `/proc/ksif`, `/proc/klog`, `/proc/klog2`, `/proc/aud/apu_dsp`, `/dev/snd/comprC%uD%u`, `/dev/dvb/adapter%i/frontend%i`, `/dev/dvb/adapter%i/dvr%i`, `/dev/dvb/adapter%i/demux%i`, `/dev/ntkdvif`, `/dev/ntkci`, `/dev/ntktspu`, `/dev/ndp_hwmem`, `/dev/ntkcrypto`, `/dev/ntksys`, `/dev/video%i`, `/dev/video10`, `/dev/video11`, `/dev/video12`, `/dev/video13`, `/dev/random`, `/sys/class/gpio/export`, `/sys/class/gpio/gpio%d/value`, `/dev/i2c-%d`, `/dev/ntkspi`, `/dev/ntgpio`, `/dev/ntkiic0`, `/dev/nvt/ntkiic0`, `/dev/nvt/ntkled`, `/dev/ntkled`, `/dev/nvt/ntktsmux`, `/dev/ntktsmux`, `/dev/dri/card0`, `/dev/dri/card1`, `/dev/nvt/ntkvplt`, `/dev/ntkvplt`.

`strings -a` on `libNDAL_EX.so`: `/dev/ntksys`, `/dev/nvt/kdolby`, `/dev/kdolby`, `/dev/ndppq`, `/dev/video%i`, `/dev/sd`, `/dev/nvt/ntkgpu`, `/dev/ntkgpu`, `/dev/nvt`, `/dev/ntktspu`.

## Ioctl immediates

`libNDAL3.so`: 276 calls that reach `ioctl@plt`. 132 distinct immediates. Five calls pass `r1` in a register: `compress_write` `4e7b11ee` (`mov r1, fp`), `compress_read` `4e7b138a` (`mov r1, fp`), `sh_drv_tsmux_TaskRoutine` `4e7c1c12` and `4e7c1c40`, `Hal_Video_Libdrm_drmIoctl` `4e7c7408` (`mov r1, r6`).

Checked pair: `_FreeGemBuffer` at `4e7b068c` is `movw r1, #25609` (`0x6409`) then `movt r1, #16392` (`0x4008`), command `0x40086409` (write, type `d`, nr 9, size 8). `Hal_Demux_Dvb_Close` uses `0x6f2a`.

Immediate calls by `_IOC` type (call count):

| type | calls |
|---|---|
| ba | 51 |
| V | 49 |
| bb | 43 |
| o | 29 |
| C | 27 |
| g | 17 |
| c8 | 14 |
| bd | 12 |
| d | 9 |
| D | 8 |
| be | 5 |
| L | 3 |
| s | 1 |
| 00 | 1 |
| 07 | 1 |
| S | 1 |

`libNDAL_EX.so`: 102 ioctl-like calls (`ioctl@plt`, `ndcommon_v4l2_ioctl@plt`, or `v4l2_ioctl@plt`). 36 distinct immediates. The rest copy `r1` from a register, or once from the stack (`NDAL_VideoExt_VD_GetStatus` at `4db59e4a`, `ldr r1, [r7, #0]`). `ndcommon_v4l2_ioctl` at `4db66c98` does `mov r1, r0`. `NDOSD_*`, `NDAL_VideoExt_Set*`, and `NDAL_GfxExt_*` load the command into a register before the call, so the number is not an immediate in the callee.

Checked pair: `_ksys_get_hw_version` at `4db56fda` is `mov.w r1, #29440` (`0x7300`) then `movt r1, #49204` (`0xc034`), command `0xc0347300` (read/write, type `s`, nr 0, size `0x34`).

### libNDAL3.so immediate ioctl commands

- `_FreeGemBuffer`: 0x40086409 (W d nr=9 sz=8) @ 4e7b068c
- `HAL_AllocBuffer`: 0xc0106440 (RW d nr=64 sz=16) @ 4e7b0832
- `HAL_Handle2VirAdr`: 0xc01064b3 (RW d nr=179 sz=16) @ 4e7b0928
- `HAL_Handle2IPCName`: 0xc008640a (RW d nr=10 sz=8) @ 4e7b0a0e
- `HAL_IPCName2Handle`: 0xc010640b (RW d nr=11 sz=16) @ 4e7b0afe
- `compress_open`: 0xc0c44310 (RW C nr=16 sz=196) @ 4e7b0e10; 0x40844312 (W C nr=18 sz=132) @ 4e7b0e5a
- `compress_get_hpointer`: 0x801c4321 (R C nr=33 sz=28) @ 4e7b0ff2
- `compress_get_tstamp`: 0x80144320 (R C nr=32 sz=20) @ 4e7b10ce
- `compress_start`: 0x4332 (NONE C nr=50 sz=0) @ 4e7b1484
- `compress_stop`: 0x4333 (NONE C nr=51 sz=0) @ 4e7b1500
- `compress_pause`: 0x4330 (NONE C nr=48 sz=0) @ 4e7b1574
- `compress_resume`: 0x4331 (NONE C nr=49 sz=0) @ 4e7b15e2
- `compress_drain`: 0x4334 (NONE C nr=52 sz=0) @ 4e7b1644
- `compress_partial_drain`: 0x4336 (NONE C nr=54 sz=0) @ 4e7b16be
- `compress_next_track`: 0x4335 (NONE C nr=53 sz=0) @ 4e7b174e
- `compress_set_gapless_metadata`: 0x80044300 (R C nr=0 sz=4) @ 4e7b17f4; 0x40244314 (W C nr=20 sz=36) @ 4e7b182a; 0x40244314 (W C nr=20 sz=36) @ 4e7b1844
- `is_codec_supported`: 0xc0c44310 (RW C nr=16 sz=196) @ 4e7b1932
- `compress_get_codec_caps`: 0xeb884311 (RW C nr=17 sz=11144) @ 4e7b1b3c
- `compress_get_pts`: 0xc0244315 (RW C nr=21 sz=36) @ 4e7b1bc4
- `compress_set_pts`: 0x40244314 (W C nr=20 sz=36) @ 4e7b1c54
- `compress_get_bufinfo`: 0xc0244315 (RW C nr=21 sz=36) @ 4e7b1cd8
- `compress_get_secure_buf`: 0xc0244315 (RW C nr=21 sz=36) @ 4e7b1e2c
- `compress_get_debug_conf`: 0xc0244315 (RW C nr=21 sz=36) @ 4e7b1eb8
- `compress_get_pcm_metadata`: 0xc0244315 (RW C nr=21 sz=36) @ 4e7b1f4e
- `compress_close_device`: 0x40244314 (W C nr=20 sz=36) @ 4e7b1ffa
- `compress_set_sync`: 0x40244314 (W C nr=20 sz=36) @ 4e7b2080
- `compress_write_strm`: 0x40244314 (W C nr=20 sz=36) @ 4e7b217a
- `compress_read_pcm`: 0xc0244315 (RW C nr=21 sz=36) @ 4e7b222e; 0xc0244315 (RW C nr=21 sz=36) @ 4e7b2266
- `_HAL_dvif_enableDVIF`: 0xc0014484 (RW D nr=132 sz=1) @ 4e7b2a02
- `_HAL_dvif_set_dmdc_procedure`: 0xc0044482 (RW D nr=130 sz=4) @ 4e7b2a7e
- `_HAL_fe_get_status`: 0x80046f45 (R o nr=69 sz=4) @ 4e7b2c88
- `HAL_Demod_SetVif`: 0xc00c4483 (RW D nr=131 sz=12) @ 4e7b2f40
- `HalDemod_GetPLP_List`: 0x80086f53 (R o nr=83 sz=8) @ 4e7b35b0
- `HalDemod_send_diseqc_sequence`: 0x40076f3f (W o nr=63 sz=7) @ 4e7b374a; 0x40076f41 (W o nr=65 sz=7) @ 4e7b375e
- `HAL_fe_set_parms`: 0x40086f52 (W o nr=82 sz=8) @ 4e7b382c
- `HAL_fe_get_frontend`: 0x80246f4d (R o nr=77 sz=36) @ 4e7b4336
- `HAL_fe_get_parms`: 0x80086f53 (R o nr=83 sz=8) @ 4e7b4404
- `HAL_Demod_GetStatus`: 0x80104470 (R D nr=112 sz=16) @ 4e7b456a; 0x80054471 (R D nr=113 sz=5) @ 4e7b45d6; 0x80046f46 (R o nr=70 sz=4) @ 4e7b46b8; 0x80026f48 (R o nr=72 sz=2) @ 4e7b46e6; 0x80086f53 (R o nr=83 sz=8) @ 4e7b475e; 0x80246f4d (R o nr=77 sz=36) @ 4e7b47a2; 0x80046f49 (R o nr=73 sz=4) @ 4e7b4894; 0x80086f53 (R o nr=83 sz=8) @ 4e7b4de6
- `_hal_fe_get_frontend.isra.1`: 0x80246f4d (R o nr=77 sz=36) @ 4e7b53fa
- `_hal_fe_get_parms`: 0x80086f53 (R o nr=83 sz=8) @ 4e7b54aa
- `_hal_fe_set_parms`: 0x40086f52 (W o nr=82 sz=8) @ 4e7b5570
- `HAL_DEMOD_EXT_DVIF_SetTunerType`: 0xc0044485 (RW D nr=133 sz=4) @ 4e7b5620
- `HAL_DEMOD_EXT_DVIF_SetStandby`: 0xc0014484 (RW D nr=132 sz=1) @ 4e7b56d0
- `HAL_DEMOD_EXT_DVIF_SetupConfig`: 0xc0044486 (RW D nr=134 sz=4) @ 4e7b57ae
- `HAL_DEMOD_EXT_SetUnicableIF`: 0x40086f52 (W o nr=82 sz=8) @ 4e7b5a4a
- `HAL_DEMOD_EXT_DeliverConfig`: 0x80246f51 (R o nr=81 sz=36) @ 4e7b5e10; 0x80246f51 (R o nr=81 sz=36) @ 4e7b5e6e
- `_Hal_Demux_CI_TaskRoutine`: 0xbd03 (NONE bd nr=3 sz=0) @ 4e7b60be
- `Hal_Demux_CI_Init`: 0xbd02 (NONE bd nr=2 sz=0) @ 4e7b615a
- `Hal_Demux_CI_Reset`: 0xbd04 (NONE bd nr=4 sz=0) @ 4e7b62c2
- `Hal_Demux_CI_GetStatus`: 0xbd05 (NONE bd nr=5 sz=0) @ 4e7b638a
- `Hal_Demux_CI_Set`: 0xbd0a (NONE bd nr=10 sz=0) @ 4e7b652e; 0xbd0e (NONE bd nr=14 sz=0) @ 4e7b6582; 0xbd0e (NONE bd nr=14 sz=0) @ 4e7b6598
- `Hal_Demux_CI_Read_Reg`: 0xbd08 (NONE bd nr=8 sz=0) @ 4e7b66c2
- `Hal_Demux_CI_Write_Reg`: 0xbd09 (NONE bd nr=9 sz=0) @ 4e7b67aa
- `Hal_Demux_CI_Read_CIs_Memory`: 0xbd06 (NONE bd nr=6 sz=0) @ 4e7b6896
- `Hal_Demux_CI_Write_FCR_Value`: 0xbd07 (NONE bd nr=7 sz=0) @ 4e7b6980
- `Hal_Demux_CI_GetEncryptMem`: 0xbd0c (NONE bd nr=12 sz=0) @ 4e7b6b42
- `Hal_Demux_Ctrl_Inv_Cache`: 0xc014730a (RW s nr=10 sz=20) @ 4e7b6d68
- `Hal_Demux_Ctrl_Hwmem_Flush_Range`: 0x2a (NONE 00 nr=42 sz=0) @ 4e7b6dde
- `Hal_Demux_Ctrl_QueryBufferPVR`: 0xbb44 (NONE bb nr=68 sz=0) @ 4e7b6e66
- `Hal_Demux_Ctrl_Crypto_PVR`: 0xbe10 (NONE be nr=16 sz=0) @ 4e7b6fcc
- `Hal_Demux_Ctrl_Crypto_SetPvrKey`: 0xbe11 (NONE be nr=17 sz=0) @ 4e7b708c
- `Hal_Demux_Ctrl_Crypto_Encrypt`: 0xbe0e (NONE be nr=14 sz=0) @ 4e7b718e
- `Hal_Demux_Ctrl_Crypto_Decrypt`: 0xbe0f (NONE be nr=15 sz=0) @ 4e7b72fe
- `Hal_Demux_Ctrl_Crypto_GetPRNG`: 0xbe12 (NONE be nr=18 sz=0) @ 4e7b73f8
- `Hal_Demux_Ctrl_CacheFlush`: 0xbb43 (NONE bb nr=67 sz=0) @ 4e7b747a
- `Hal_Demux_Ctrl_SetReadAddr`: 0xbb54 (NONE bb nr=84 sz=0) @ 4e7b74fe
- `Hal_Demux_Desc_Set_Bypass`: 0xbb2c (NONE bb nr=44 sz=0) @ 4e7b7878; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b78c0; 0xbb2b (NONE bb nr=43 sz=0) @ 4e7b792c
- `Hal_Demux_Desc_EnableMask`: 0xbb24 (NONE bb nr=36 sz=0) @ 4e7b7a76; 0xbb25 (NONE bb nr=37 sz=0) @ 4e7b7aaa
- `Hal_Demux_Desc_Open`: 0xbb72 (NONE bb nr=114 sz=0) @ 4e7b7bd4; 0xbb1e (NONE bb nr=30 sz=0) @ 4e7b7c26; 0xbb73 (NONE bb nr=115 sz=0) @ 4e7b7d34
- `Hal_Demux_Desc_Close`: 0xbb1f (NONE bb nr=31 sz=0) @ 4e7b7f02; 0xbb74 (NONE bb nr=116 sz=0) @ 4e7b7f94
- `Hal_Demux_Desc_Set_Pid`: 0xbb20 (NONE bb nr=32 sz=0) @ 4e7b8250; 0xbb75 (NONE bb nr=117 sz=0) @ 4e7b82c8
- `Hal_Demux_Desc_Disable`: 0xbb2c (NONE bb nr=44 sz=0) @ 4e7b84b0; 0xbb2b (NONE bb nr=43 sz=0) @ 4e7b84d8
- `Hal_Demux_Desc_Cfg`: 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b86dc; 0xbb2a (NONE bb nr=42 sz=0) @ 4e7b8724; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b87a8; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b87fa; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b883c; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b887e; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b88c0
- `Hal_Demux_Desc_Config`: 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8c06; 0xbb2a (NONE bb nr=42 sz=0) @ 4e7b8c4e; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8cf8; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8d4e; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8d90; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8dd2; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8e14; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8e56; 0xbb26 (NONE bb nr=38 sz=0) @ 4e7b8ed2
- `Hal_Demux_Desc_Set_Descrambler_Keys`: 0xbb2b (NONE bb nr=43 sz=0) @ 4e7b9242; 0xbb29 (NONE bb nr=41 sz=0) @ 4e7b93e0; 0xbb2a (NONE bb nr=42 sz=0) @ 4e7b9402
- `Hal_Demux_Desc_Start`: 0xbb23 (NONE bb nr=35 sz=0) @ 4e7b9774; 0xbb22 (NONE bb nr=34 sz=0) @ 4e7b978e; 0xbb78 (NONE bb nr=120 sz=0) @ 4e7b983c; 0xbb77 (NONE bb nr=119 sz=0) @ 4e7b9854
- `Hal_Demux_Desc_Stop`: 0xbb23 (NONE bb nr=35 sz=0) @ 4e7b99e6; 0xbb78 (NONE bb nr=120 sz=0) @ 4e7b9a66
- `Hal_Demux_Desc_EnableMask4ECP`: 0xbb72 (NONE bb nr=114 sz=0) @ 4e7b9b5e
- `Hal_Demux_Dvb_Close`: 0x6f2a (NONE o nr=42 sz=0) @ 4e7b9c6c
- `Hal_Demux_Dvb_Setsource`: 0x40046f31 (W o nr=49 sz=4) @ 4e7b9cd6
- `Hal_Demux_Dvb_Stop`: 0x6f2a (NONE o nr=42 sz=0) @ 4e7b9d9a
- `Hal_Demux_Dvb_GetStc`: 0xc0106f32 (RW o nr=50 sz=16) @ 4e7b9de2
- `Hal_Demux_Dvb_Dvr_Flush`: 0xc0086f68 (RW o nr=104 sz=8) @ 4e7b9e70
- `Hal_Demux_Dvb_Dvr_GetPlayBuffer`: 0xc0086f64 (RW o nr=100 sz=8) @ 4e7b9ef0
- `Hal_Demux_Dvb_Set_Pesfilter`: 0x6f2d (NONE o nr=45 sz=0) @ 4e7ba06c; 0x40146f2c (W o nr=44 sz=20) @ 4e7ba0aa
- `Hal_Demux_Dvb_Set_Sectionfilter`: 0x6f2d (NONE o nr=45 sz=0) @ 4e7ba172; 0x403c6f2b (W o nr=43 sz=60) @ 4e7ba1d6
- `venc_v4l2_set_mmap_secure`: 0xc0445609 (RW V nr=9 sz=68) @ 4e7bb162
- `venc_v4l2_set_mmap_normal`: 0xc0445609 (RW V nr=9 sz=68) @ 4e7bb278
- `venc_v4l2_streamon`: 0x40045612 (W V nr=18 sz=4) @ 4e7bb3a2
- `venc_v4l2_streamoff`: 0x40045613 (W V nr=19 sz=4) @ 4e7bb47a
- `venc_v4l2_set_bitrate`: 0xc008561c (RW V nr=28 sz=8) @ 4e7bb556
- `venc_v4l2_set_framerate`: 0xc0cc5616 (RW V nr=22 sz=204) @ 4e7bb5d0
- `venc_v4l2_set_mute`: 0xc008561c (RW V nr=28 sz=8) @ 4e7bb64a
- `venc_v4l2_set_src_mode`: 0xc008561c (RW V nr=28 sz=8) @ 4e7bb6ba
- `venc_v4l2_set_vid_fmt`: 0xc0cc5605 (RW V nr=5 sz=204) @ 4e7bb74c
- `venc_v4l2_subscribe_event`: 0x4020565a (W V nr=90 sz=32) @ 4e7bb7d8
- `venc_v4l2_de_queue_event`: 0x80805659 (R V nr=89 sz=128) @ 4e7bb844
- `venc_v4l2_queue_buffer`: 0xc044560f (RW V nr=15 sz=68) @ 4e7bb920
- `venc_v4l2_queue_phys_buffer`: 0xc044560f (RW V nr=15 sz=68) @ 4e7bb9c0
- `venc_v4l2_dequeue_buffer`: 0xc0445611 (RW V nr=17 sz=68) @ 4e7bba4e
- `venc_v4l2_set_event`: 0xc044560f (RW V nr=15 sz=68) @ 4e7bbafc
- `Hal_Gpio_Read`: 0x6712 (NONE g nr=18 sz=0) @ 4e7be5c6
- `Hal_Gpio_Write`: 0x670a (NONE g nr=10 sz=0) @ 4e7be798; 0x670e (NONE g nr=14 sz=0) @ 4e7be7b0; 0x670c (NONE g nr=12 sz=0) @ 4e7be7d0
- `Hal_Gpio_Config`: 0x670a (NONE g nr=10 sz=0) @ 4e7be8c6; 0x670e (NONE g nr=14 sz=0) @ 4e7be8d6
- `Hal_Gpio_GetDirection`: 0x670a (NONE g nr=10 sz=0) @ 4e7be9d4; 0x670f (NONE g nr=15 sz=0) @ 4e7be9e4
- `Hal_Gpio_PinctrlRead`: 0x670a (NONE g nr=10 sz=0) @ 4e7beac4; 0x6711 (NONE g nr=17 sz=0) @ 4e7bead4
- `Hal_Gpio_PinctrlWrite`: 0x670a (NONE g nr=10 sz=0) @ 4e7bebc6; 0x6710 (NONE g nr=16 sz=0) @ 4e7bebd6
- `Hal_Gpio_GetDriving`: 0x670a (NONE g nr=10 sz=0) @ 4e7bec9e; 0x6720 (NONE g nr=32 sz=0) @ 4e7becae
- `Hal_Gpio_SetDriving`: 0x670a (NONE g nr=10 sz=0) @ 4e7bed76; 0x6721 (NONE g nr=33 sz=0) @ 4e7bed86
- `Hal_Gpio_Conf_CITSIF`: 0x6722 (NONE g nr=34 sz=0) @ 4e7bee40
- `_i2c_RdWr_Transfer`: 0x707 (NONE 07 nr=7 sz=0) @ 4e7bf0c4
- `Hal_SPI_EraseFlash`: 0x5320 (NONE S nr=32 sz=0) @ 4e7bf58a
- `Hal_PQ_SetHdmiPC`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf644
- `Hal_PQ_SetOffsetGain`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf6b2
- `Hal_PQ_ContrastDimming`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf718
- `Hal_PQ_WCE`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf79c
- `Hal_PQ_VideoDecoder`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf80e
- `Hal_PQ_PostSharpness`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf87a
- `Hal_PQ_PostPostSharpness`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf8e6
- `Hal_PQ_SetHLPF`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bf94c
- `Hal_PQ_DataDump`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfa00
- `Hal_PQ_SetBLE`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfa7c
- `Hal_PQ_SetBLEStartPointMin`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfae4
- `Hal_PQ_ColorSpaceCustom`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfb54
- `Hal_PQ_BypassDither`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfbbc
- `Hal_PQ_SetRGBOnlyMode`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfc24
- `Hal_PQ_ColorEnhanceBypass`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfc8c
- `Hal_PQ_Comp_Phase`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfcf4
- `Hal_PQ_SetColorCompensation`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfd5c
- `Hal_PQ_SetSaturation`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfdc4
- `Hal_PQ_SetSaturationSub`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfe2c
- `Hal_PQ_SetxvYCC`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bfe9c
- `Hal_PQ_SetHdmiBlackLevel`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bff00
- `Hal_PQ_SetFilmMode`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bff68
- `Hal_PQ_SetScartUltraBlack`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7bffd6
- `Hal_PQ_PPQT_Version`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c003c
- `Hal_PQ_ClearWhite`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c00aa
- `Hal_PQ_ClearWhite2`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c011c
- `Hal_PQ_GammaEnable`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c01e8
- `Hal_PQ_WB_Enable`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0250
- `Hal_PQ_ReadAPL_CB`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c02b8
- `Hal_PQ_SetBlackAdjust`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0326
- `Hal_PQ_SetFRCDemoModeOnOff`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0392
- `Hal_PQ_SetCCEMode`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c03fe
- `Hal_PQ_SetBIPPMode`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0464
- `Hal_PQ_SetPictureQuality`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0514; 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0534; 0xba87 (NONE ba nr=135 sz=0) @ 4e7c055c; 0xba87 (NONE ba nr=135 sz=0) @ 4e7c05a6; 0xba87 (NONE ba nr=135 sz=0) @ 4e7c05c8; 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0606
- `Hal_PQ_ReadDP_RGB_APL`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0670
- `Hal_PQ_GetHistogram`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c06d8
- `Hal_PQ_GetHDRStatus`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0782
- `Hal_PQ_SetDVPanelConfig`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c07e8
- `Hal_PQ_SetCMOnOff`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0850
- `Hal_PQ_SetPictureMode`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c08b8
- `Hal_PQ_SetGammaData`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0920
- `Hal_PQ_GetGammaData`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0988
- `Hal_PQ_SetGammaSinglePoint`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0a02
- `Hal_PQ_GetGammaSinglePoint`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0a7a
- `Hal_PQ_GetPanelCursor`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0b28
- `Hal_PQ_SetHDR10P_BYPASS`: 0xba87 (NONE ba nr=135 sz=0) @ 4e7c0b98
- `Hal_Pwm_Open`: 0x403c4c4c (W L nr=76 sz=60) @ 4e7c0de8
- `Hal_Pwm_Set_ByChannel`: 0xc03c4c53 (RW L nr=83 sz=60) @ 4e7c1048
- `Hal_Pwm_Set_HWFR_ByChannel`: 0xc03c4c55 (RW L nr=85 sz=60) @ 4e7c119a
- `sh_drv_tsmux_Init`: 0xc004c80d (RW c8 nr=13 sz=4) @ 4e7c1354
- `sh_drv_tsmux_Open`: 0xc004c801 (RW c8 nr=1 sz=4) @ 4e7c14c8
- `sh_drv_tsmux_Start`: 0xc004c803 (RW c8 nr=3 sz=4) @ 4e7c15e6
- `sh_drv_tsmux_Packetize`: 0xc004c80c (RW c8 nr=12 sz=4) @ 4e7c16e2
- `sh_drv_tsmux_SetPmtMode`: 0xc004c812 (RW c8 nr=18 sz=4) @ 4e7c178c
- `sh_drv_tsmux_SetUserSection`: 0xc004c806 (RW c8 nr=6 sz=4) @ 4e7c17ea
- `sh_drv_tsmux_StartUserSection`: 0xc004c807 (RW c8 nr=7 sz=4) @ 4e7c184e
- `sh_drv_tsmux_StopUserSection`: 0xc004c808 (RW c8 nr=8 sz=4) @ 4e7c1892
- `sh_drv_tsmux_ReleaseTsBuffer`: 0xc004c80b (RW c8 nr=11 sz=4) @ 4e7c18e0
- `sh_drv_tsmux_Stop`: 0xc004c804 (RW c8 nr=4 sz=4) @ 4e7c1948
- `sh_drv_tsmux_Flush`: 0xc004c805 (RW c8 nr=5 sz=4) @ 4e7c19d0
- `sh_drv_tsmux_Close`: 0xc004c802 (RW c8 nr=2 sz=4) @ 4e7c1a2a; 0xc004c804 (RW c8 nr=4 sz=4) @ 4e7c1a7c
- `sh_drv_tsmux_WaitStatus`: 0xc004c809 (RW c8 nr=9 sz=4) @ 4e7c1b9a
- `Hal_Userdata_EnableVbiLine`: 0xc0185648 (RW V nr=72 sz=24) @ 4e7c1e88; 0xc0185648 (RW V nr=72 sz=24) @ 4e7c1ed2
- `Hal_Userdata_DisableVbiLine`: 0xc0185648 (RW V nr=72 sz=24) @ 4e7c209a
- `_HAL_UserData_GetAnalogData`: 0xc0185647 (RW V nr=71 sz=24) @ 4e7c228a; 0xc0185647 (RW V nr=71 sz=24) @ 4e7c2388
- `ServerDrmAuthentically`: 0x40046411 (W d nr=17 sz=4) @ 4e7c2982; 0x80046402 (R d nr=2 sz=4) @ 4e7c2996
- `ClientDrmAuthentically`: 0x80046402 (R d nr=2 sz=4) @ 4e7c2a64; 0x40046411 (W d nr=17 sz=4) @ 4e7c2a8e
- `Hal_Video_Vpltio_SendCmd`: 0xc0245604 (RW V nr=4 sz=36) @ 4e7c867c
- `Hal_VideoDec_Streamon`: 0x40045612 (W V nr=18 sz=4) @ 4e7c8a42
- `Hal_VideoDec_Streamoff`: 0x40045613 (W V nr=19 sz=4) @ 4e7c8b52
- `Hal_VideoDec_Pause`: 0xc008561c (RW V nr=28 sz=8) @ 4e7c8c54
- `Hal_VideoDec_Resume`: 0xc008561c (RW V nr=28 sz=8) @ 4e7c8d52
- `Hal_VideoDec_v4l2_get_ctrls`: 0xc0185647 (RW V nr=71 sz=24) @ 4e7c8e50
- `Hal_VideoDec_v4l2_set_ctrls`: 0xc008561c (RW V nr=28 sz=8) @ 4e7c8f0a
- `Hal_VideoDec_v4l2_set_fmt`: 0xc0cc5605 (RW V nr=5 sz=204) @ 4e7c8fc4
- `Hal_VideoDec_get_status`: 0xc0185647 (RW V nr=71 sz=24) @ 4e7c912c; 0xc0185647 (RW V nr=71 sz=24) @ 4e7c91aa; 0xc0185647 (RW V nr=71 sz=24) @ 4e7c920e; 0xc0185647 (RW V nr=71 sz=24) @ 4e7c925a
- `Hal_VideoDec_subscribe_event`: 0x4020565a (W V nr=90 sz=32) @ 4e7c93e8; 0x4020565a (W V nr=90 sz=32) @ 4e7c9404; 0x4020565a (W V nr=90 sz=32) @ 4e7c9420; 0x4020565a (W V nr=90 sz=32) @ 4e7c943c; 0x4020565a (W V nr=90 sz=32) @ 4e7c9458; 0x4020565a (W V nr=90 sz=32) @ 4e7c9474; 0x4020565a (W V nr=90 sz=32) @ 4e7c9492
- `Hal_VideoDec_poll_event`: 0x80805659 (R V nr=89 sz=128) @ 4e7c9688
- `Hal_Vin_SelectInput`: 0xc0045627 (RW V nr=39 sz=4) @ 4e7c9966
- `Hal_Vin_SetColorSystem`: 0x40085618 (W V nr=24 sz=8) @ 4e7c99ce
- `Hal_Vin_GetVDLock`: 0xc008561b (RW V nr=27 sz=8) @ 4e7c9a46
- `Hal_Vin_GetLock`: 0xc050561a (RW V nr=26 sz=80) @ 4e7c9abe
- `Hal_Vin_GetRes`: 0xc0185647 (RW V nr=71 sz=24) @ 4e7c9b5a
- `Hal_Vin_SetInputRes`: 0xc0185648 (RW V nr=72 sz=24) @ 4e7c9bf2
- `Hal_Vin_GetHdmiStatus`: 0xc0185647 (RW V nr=71 sz=24) @ 4e7c9c88
- `Hal_Vin_SetPcMode`: 0xc0185648 (RW V nr=72 sz=24) @ 4e7c9d1e
- `Hal_Vin_SetStreamStart`: 0x40045612 (W V nr=18 sz=4) @ 4e7c9d88

### libNDAL_EX.so immediate ioctl commands

- `_Z20_ksys_get_hw_versioni`: 0xc0347300 (RW s nr=0 sz=52) @ 4db56fda
- `NDAL_VenderVideoExt_GetDPCountTsk`: 0xba86 (NONE ba nr=134 sz=0) @ 4db57e1a
- `NDAL_VenderVideoExt_Send_DolbyVision_GetCmd`: 0xd01b6001 (RW ` nr=1 sz=4123) @ 4db5814c; 0xd01b6000 (RW ` nr=0 sz=4123) @ 4db5817a
- `NDAL_VenderVideoExt_SendGetCmd`: 0xba87 (NONE ba nr=135 sz=0) @ 4db58260
- `NDAL_VenderVideoExt_SendSetCmd`: 0xba87 (NONE ba nr=135 sz=0) @ 4db584f6
- `GPU_SYS_GetVersion`: 0x4004471f (W G nr=31 sz=4) @ 4db63d5c
- `GPU_SYS_Lock`: 0x40044740 (W G nr=64 sz=4) @ 4db63e58
- `GPU_SYS_Unlock`: 0x40044741 (W G nr=65 sz=4) @ 4db63ec0
- `GPU_SYS_Trigger_Unlock`: 0x4004474a (W G nr=74 sz=4) @ 4db63f2a
- `GPU_SYS_MEM_CMA_Alloc`: 0xc0044742 (RW G nr=66 sz=4) @ 4db63fac
- `GPU_SYS_MEM_PAGES_Alloc`: 0xc0044742 (RW G nr=66 sz=4) @ 4db64012
- `GPU_SYS_MEM_Free`: 0xc0044743 (RW G nr=67 sz=4) @ 4db64062
- `GPU_SYS_MEM_Show`: 0xc0044744 (RW G nr=68 sz=4) @ 4db640c4
- `GPU_SYS_MEM_Cache_Invalidate`: 0x40044722 (W G nr=34 sz=4) @ 4db64130
- `GPU_SYS_MEM_Cache_Writeback`: 0x40044722 (W G nr=34 sz=4) @ 4db641b4
- `GPU_SYS_MEM_Cache_Invalidate_All`: 0x40044723 (W G nr=35 sz=4) @ 4db64238
- `GPU_SYS_MEM_Cache_Writeback_All`: 0x40044723 (W G nr=35 sz=4) @ 4db642bc
- `GPU_SYS_MEM_Cache_Sync_Ranges`: 0x40044724 (W G nr=36 sz=4) @ 4db6433c
- `GPU_SYS_MEM_Cache_Flush_All`: 0x40044725 (W G nr=37 sz=4) @ 4db643b8
- `_ZL21_ndal_fe_get_frontendiiPi.isra.0`: 0x80246f4d (R o nr=77 sz=36) @ 4db6707a
- `_ZL18_ndal_fe_get_parmsiiPi`: 0x80086f53 (R o nr=83 sz=8) @ 4db6712a
- `_ZL18_ndal_fe_set_parmsiij`: 0x40086f52 (W o nr=82 sz=8) @ 4db671f0
- `Nd_NIM_DVIF_SetTunerType`: 0xc0044485 (RW D nr=133 sz=4) @ 4db6773c
- `Nd_NIM_SetUnicableIF`: 0x40086f52 (W o nr=82 sz=8) @ 4db679ce
- `Nd_NIM_GetPLPStatus`: 0x80086f53 (R o nr=83 sz=8) @ 4db67bf0
- `Nd_NIM_Send_diseqc_sequence`: 0x40076f3f (W o nr=63 sz=7) @ 4db67cea; 0x40076f41 (W o nr=65 sz=7) @ 4db67d02
- `Nd_NIM_DeliverConfig`: 0x80246f51 (R o nr=81 sz=36) @ 4db67f52; 0x80246f51 (R o nr=81 sz=36) @ 4db67fb0
- `NDSYS_SYS_GetReservedMemory`: 0xc0147305 (RW s nr=5 sz=20) @ 4db68eae; 0xc0107303 (RW s nr=3 sz=16) @ 4db68f10; 0xc0107303 (RW s nr=3 sz=16) @ 4db68f2a
- `ND_SYS_Init`: 0xc0107303 (RW s nr=3 sz=16) @ 4db695a2
- `ND_TSPU_CRC32_Check`: 0xbb5e (NONE bb nr=94 sz=0) @ 4db69760
- `ND_TSPU_Get_Audio_Filter_Handle`: 0xbb6c (NONE bb nr=108 sz=0) @ 4db69824
- `ND_TSPU_FILT_GetPTS`: 0xbb55 (NONE bb nr=85 sz=0) @ 4db698ce
- `ND_TSPU_Desc_PID_Enable`: 0xbb72 (NONE bb nr=114 sz=0) @ 4db6998c; 0xbb1e (NONE bb nr=30 sz=0) @ 4db699ce; 0xbb20 (NONE bb nr=32 sz=0) @ 4db699f6; 0xbb22 (NONE bb nr=34 sz=0) @ 4db69a12
- `ND_TSPU_Desc_PID_Disable`: 0xbb23 (NONE bb nr=35 sz=0) @ 4db69adc; 0xbb1f (NONE bb nr=31 sz=0) @ 4db69af8
- `ND_TSPU_Desc_Get_ScrambleStatus`: 0xbb5f (NONE bb nr=95 sz=0) @ 4db69bae
- `ND_TSPU_Desc_Get_ScrambleBit`: 0xbb21 (NONE bb nr=33 sz=0) @ 4db69c58

## Kernel modules

Text function counts (`nm` `t` and `T`): `ntkdriver_nondovi.ko` 29885, `ntkndpdrm.ko` 1671, `ntkndpalsa.ko` 1348, `PreInit.ko` 1178, `ntkndpextin.ko` 409, `ntkethmac.ko` 342, `ntkndphdmiswitch.ko` 50. ET_REL symbol values are section offsets. Disassembly addresses below are the ones the ARM objdump printed.

`modinfo` `depends`: drm `PreInit,ntkdriver`; extin `ntkdriver`; nondovi `PreInit`; alsa `ntkdriver,PreInit`; eth none (`intree: Y`); PreInit none; hdmiswitch `ntkdriver`.

### ntkndpdrm.ko

Description `Template DRM Driver`. Parameters, all `uint`: `g_NdpDrmOSDPWaitVsyncDbg`, `g_NdpOsdCtlDbg`, `g_NdpDrmSGPDbg`, `g_NdpDrmOSDPDbg`, `g_NdpDrmCURSORDbg`, `g_NdpDrmGPDbg`, `g_NdpDrmGaDbgMask`, `NDP_GEM_LOG_MASK`, `g_NdpDrmFBDbg`.

`init_module` at `.init.text+0`: `DRV_DBG_Register` with `r0=5` and `r1=0x100000`, then `gfxp_init`, `dp_main_init`, `dp_sub_init`, `dp_sub2_init`, `dummy_panel_init`, two `platform_device_register_full` calls, then `__platform_driver_register`. The success path returns through `and r0, r0, r0, asr #31` at `+0xe0`. `platform_driver_unregister` is the failure path at `+0x174`. `NDP_DebugInit` at `+0x11c` and `__dynamic_nvt_dbg` at `+0x16c` are on that failure path.

`cleanup_module` success path: `platform_device_unregister`, `platform_device_unregister`, `platform_driver_unregister`, `gfxp_exit`, `NDP_DebugExit`.

Strings: `/proc/meminfo`, `/proc/pagetypeinfo`, `/dev/sd`. This module has no `/dev/dri` string.

`.data` symbol `ndp_drm_ioctls` is section 26 offset `0x294`, size 1440, which is 90 `drm_ioctl_desc` slots of 16 bytes. 24 slots are non-zero. The command word is a literal. The function pointer is a relocation. Every filled command is direction read/write, type `d`:

| index | command | nr | size | flags | function |
|---|---|---|---|---|---|
| 0 | `0xc0106440` | `0x40` | 16 | `0x10` | `ndp_drm_gem_create_ioctl` |
| 1 | `0xc0186441` | `0x41` | 24 | `0x10` | `ndp_drm_gem_hw_create_ioctl` |
| 4 | `0xc0306444` | `0x44` | 48 | `0x10` | `ndp_drm_gem_create_capture_ioctl` |
| 17 | `0xc1a46451` | `0x51` | 420 | `0x10` | `ndp_drm_crtc_set_dp_source_ioctl` |
| 18 | `0xc0546452` | `0x52` | 84 | `0x10` | `ndp_drm_crtc_hw_capture_ioctl` |
| 21 | `0xc00c6455` | `0x55` | 12 | `0x10` | `ndp_drm_crtc_set_onoff_ioctl` |
| 22 | `0xc04c6456` | `0x56` | 76 | `0x11` | `ndp_drm_crtc_get_fb_info_ioctl` |
| 23 | `0xc00c6457` | `0x57` | 12 | `0x11` | `ndp_drm_crtc_set_resume_data_ioctl` |
| 24 | `0xc0106458` | `0x58` | 16 | `0x11` | `ndp_drm_crtc_output_send_data_ioctl` |
| 25 | `0xc00c6459` | `0x59` | 12 | `0x10` | `ndp_drm_crtc_get_status_ioctl` |
| 32 | `0xc00c6460` | `0x60` | 12 | `0x10` | `ndp_drm_crtc_hw_capture_sync_ioctl` |
| 64 | `0xc0546480` | `0x80` | 84 | `0x11` | `ndp_drm_ga_exec_ioctl` |
| 65 | `0xc0106481` | `0x81` | 16 | `0x11` | `ndp_drm_gem_cache_op_ioctl` |
| 72 | `0xc0046488` | `0x88` | 4 | `0x11` | `ndp_drm_crtc_qpi_gfx_crc_pf_test_ioctl` |
| 73 | `0xc0106489` | `0x89` | 16 | `0x11` | `ndp_drm_crtc_qpi_get_gfx_crc_ioctl` |
| 80 | `0xc2d86490` | `0x90` | 728 | `0x11` | `ndp_drm_crtc_qpi_get_dst_crc_ioctl` |
| 82 | `0xc2d86492` | `0x92` | 728 | `0x11` | `ndp_drm_crtc_qpi_get_out_crc_ioctl` |
| 83 | `0xc0106493` | `0x93` | 16 | `0x11` | `ndp_drm_crtc_qpi_set_input_test_pattern_ioctl` |
| 84 | `0xc0106494` | `0x94` | 16 | `0x11` | `ndp_drm_crtc_qpi_set_incapt_clock_sel_ioctl` |
| 85 | `0xc0046495` | `0x95` | 4 | `0x11` | `ndp_drm_crtc_qpi_ready_get_out_crc_ioctl` |
| 86 | `0xc0046496` | `0x96` | 4 | `0x11` | `ndp_drm_crtc_qpi_set_gp_synconoff_ioctl` |
| 87 | `0xc2d86497` | `0x97` | 728 | `0x11` | `ndp_drm_crtc_qpi_get_input_crc_ioctl` |
| 88 | `0xc2d86498` | `0x98` | 728 | `0x11` | `ndp_drm_crtc_qpi_get_post_crc_ioctl` |
| 89 | `0xc0146499` | `0x99` | 20 | `0x11` | `ndp_drm_crtc_qpi_func_ioctl` |

### ntkndpextin.ko

Description `Third Party Analog Video Input Template Driver`. OF alias `samsung,sdp-extin`. No parameters. No `/dev`, `/proc`, or `/sys` string.

`init_module` straight line: `DRV_DBG_Register`, `__platform_driver_register`, `memset`, `platform_device_register_full`. `cleanup_module`: `platform_device_unregister`, `platform_driver_unregister`. `NDP_DebugInit` and `__dynamic_nvt_dbg` are on the debug side paths.

Ioctl ops live in `.data`: `ndp_av_atv_scart_ioctl_ops` at `0x74`, `ndp_comp_ioctl_ops` at `0x240`, `ndp_vga_ioctl_ops` at `0x40c`, reached through `video_ioctl2`. The command number is the v4l2 code the core passes in. Global text symbols are the `ndp_extin_dev_*` AFE/AVD/VBI set, from `data_verification` at `0x190` through `ndp_extin_drv_irq_handler` at `0xf2d0`.

### ntkdriver_nondovi.ko

Description `NOVATEK MICROELECTRONICS CORP - KERNEL MODE DRIVERS`. OF alias `nvt,nvt_imvq`. 29885 text functions. `init_module` order, with `printk` omitted:

`proc_mkdir`, `KER_STBC_DrvInit`, `SPI_DrvInit`, `DRV_STBC_Open`, `KER_STC_DrvInit`, `KER_PTSQ_DrvInit`, `KER_SB_DrvInit`, `KER_TSPU_DrvInit`, `KER_AUD_DrvInit`, `KER_DVIF_DrvInit`, `KER_BLAST_DrvInit`, `KER_DOLBY_DrvInit`, `KER_TVOUT_DrvInit`, `KER_SCLR_DrvInit`, `KER_TVADC_DrvInit`, `KER_HDMI_DrvInit`, `KER_VPLT_DrvInit`, `KER_VD_DrvInit`, `KER_FR2IP_DrvInit`, `KER_FRE_DrvInit`, `kvdec_imvq_device_init`, `kvenc_device_init`, `VODFB_VidInit`, `KER_WDT_DrvInit`, `KER_TSMUX_DrvInit`, `KER_OSD_DrvInit`, `KER_XDMA_DrvInit`, `KER_GPU_DrvInit`, `Ker_DispAge_DrvInit`, `KER_CPE_DrvInit`, `KER_CAL_DrvInit`, `KER_CRYPTO_DrvInit`, `KER_CI_DrvInit`, `KER_CEC_DrvInit`, `nt_ddcci_DriverInit`, `KER_LEDTOP_DrvInit`, `DRV_LEDTOP_Init`, `KER_HDMA_DrvInit`, `KER_DEMOD_DrvInit`, `ndp_alsa_init`, `DRV_SDADC_Init`, `NVT_DBG_Init`, `BRIG_Init`, `KER_CONVERTER_DrvInit`, `KER_LEON2_DrvInit`, `KER_LEON2Core2_DrvInit`, `KER_DDRINFO_DrvInit`, `DRV_DDCCI_Init`, `DRV_BLAST_Init`.

`cleanup_module` order, `printk` omitted: `KER_GPU_DrvExit`, `Ker_DispAge_DrvExit`, `KER_XDMA_DrvExit`, `KER_WDT_DrvExit`, `KER_OSD_DrvExit`, `KER_FR2IP_DrvExit`, `KER_DOLBY_DrvExit`, `KER_FRE_DrvExit`, `kvdec_imvq_device_exit`, `kvenc_device_exit`, `VODFB_VidTerm`, `KER_TSMUX_DrvExit`, `KER_SCLR_DrvExit`, `KER_TVADC_DrvExit`, `KER_VPLT_DrvExit`, `KER_HDMI_DrvExit`, `KER_TVOUT_DrvExit`, `KER_BLAST_DrvExit`, `nt_ddcci_DriverExit`, `KER_AUD_DrvExit`, `KER_DVIF_DrvExit`, `KER_TSPU_DrvExit`, `KER_CONVERTER_DrvExit`, `KER_SB_DrvExit`, `KER_PTSQ_DrvExit`, `KER_STC_DrvExit`, `KER_CRYPTO_DrvExit`, `KER_CPE_DrvExit`, `KER_CAL_DrvExit`, `KER_CI_DrvExit`, `KER_CEC_DrvExit`, `KER_LEDTOP_DrvExit`, `KER_HDMA_DrvExit`, `DRV_SDADC_Term`, `NVT_DBG_Term`, `KER_LEON2_DrvExit`, `KER_LEON2Core2_DrvExit`, `SPI_DrvExit`, `KER_STBC_DrvExit`, `remove_proc_entry`, `remove_proc_entry`. Exit is not the reverse of init. `KER_DEMOD_DrvInit`, `ndp_alsa_init`, `BRIG_Init`, `DRV_DDCCI_Init`, `DRV_BLAST_Init`, and `KER_DDRINFO_DrvInit` have no call in this `cleanup_module` body. STBC and SPI run last on the way out.

Device and proc strings include `/dev/ntkadc`, `/dev/ntkddcci0` through `/dev/ntkddcci4`, `/dev/ntkiics5`, `/dev/ntkcal`, `/dev/nvt/ntkcec`, `/dev/ntkcec`, `/dev/ntkaud`, `/dev/ntkaud0` through `/dev/ntkaud2`, `/dev/mali`, `/dev/nvt/ntkspi`, `/dev/nvt/ntkstbc`, `/dev/ntkstbc`, `/proc/kpp`, `/proc/aud/aud_ver`, `/proc/aud/aud_top`, `/proc/aud/apu_dsp`, `/proc/aud/opu_out0` through `opu_out2`, and the `ipu_*`, `ppu.*`, `dsu.*`, and `cpt_*` nodes. Major parameters: `KSTBC_major`, `KSPI_major`, `LED_major`, `KDDCCI_major`, `KCEC_major`, `KCAL_major`. Other parameters: `g_XDMADbgMask`, `gu32SplitSize`, `forceVpsLeon2`, the `Hevc*` set, `g_tsmux_logmask`, `gau32PlaneBufAddrSize`, `gau32BufAddrSize`, `gau32PlaneWinSize`, `gau32PlaneOrder`, `gu32NumOfBuffer`, `gau32NumOfBuffer`, `gu32FastLogoPlaneID`, `gu32OSDTrace`, `gau32SeperateBuffer`, `g_AudioVirtNum`, `gKER_LEDTOP_Debug`, `g_HDMADbgMask`, `g_GpuOpDbgMask`, `g_GpuDbgMask`, `fr2ip_print_timestamp`, `g_DispAgeOpDbgMask`, `g_DispAgeDbgMask`, `g_iic_debug`, `magic_val`, `gKER_CAL_Debug`, `g_sifStdDetectDelayMs`, `g_sifModeDetectDelayMs`, `g_sifWaitDetect`.

Individual ioctl numbers inside this 18 MB object were not walked function by function. The userspace commands aimed at these nodes are the NDAL immediates (`0x67xx` GPIO, `0x6fxx` demux, `0xbbxx` TSPU, `0xc0xx` sys/gpu).

### ntkndpalsa.ko

Descriptions: `ASoC TDP Machine driver`, `ASoC SPD codec driver`, `ASoC SIF Machine driver`. Parameter `CUSTOMER_NAME` (`charp`). No `/dev` or `/proc` string in the module; it registers ALSA cards.

`init_module` calls `_alsa_init_task_p.constprop.1` at `.text+0x530` and returns 0. That function calls `KER_Distribute_SetMsgName` (`r0=8`), `AUD_ALSA_Init`, `__platform_driver_register` seven times, `KER_Distribute_ChkMsgGrp` (`r0=8`, `r1=32`), then a `platform_device_alloc` / `platform_device_add` loop. `cleanup_module` calls `audio_alsa_exit` at `.text+0x878`. Probe/remove globals: `tdp_audio_dma_probe` / `tdp_audio_dma_remove`, `tdp_audio_pcm_probe` / `tdp_audio_pcm_remove`, `tdp_codec_device_probe` / `tdp_codec_device_remove`, `tdp_codec_sif_device_probe` / `tdp_codec_sif_device_remove`, `tdp_sif_machine_probe` / `tdp_sif_machine_remove`, `tdp_sif_platform_probe` / `tdp_sif_platform_remove`.

### ntkethmac.ko

Description `SYNOPSYS GMAC NETWORK DRIVER WITH PCI INTERFACE`. OF alias `nvt,synopsys-mac`. No parameters and no `/dev` or `/proc` string.

`init_module` is `SynopGMAC_Host_Interface_init` at `.init.text+0x208`. The body is one call, `synopGMAC_init_network_interface`, then `ldm sp, {fp, sp, pc}`. `cleanup_module` is `SynopGMAC_Host_Interface_exit` at offset `0x50`. Ioctl symbols: `nvt_private_ioctl` at `.text+0x6ec0`, `synopGMAC_linux_do_ioctl` at `.text+0x726c`. Compares inside `nvt_private_ioctl` are `#0`, `#1`, and `#5`, a private sub-command after the networking core has selected the handler.

### PreInit.ko

One object carries several descriptions: `NT7266 kernel ioremap DMA cache API`; `platform:nt726xx-module-gpio` version 1.1; `platform:nt726xx-gpio` version 1.0; `platform:nt726xx_i2c` version 1.1; OF `nvt,module-gpio`. Parameters: `i2c_major`, `KSYS_major`. Strings: `/dev/ntktcon`, `/proc/cmdline`, `/sys/kernel/debug`, `/sys/kernel/debug/dynamic_debug/control`.

`init_module` at `.init.text+0x1474`, after four `printk`s: `KER_EMU_Init`, `KER_Nerve_Init`, `KER_PMI_Init`, `KER_SYS_DrvInit`, `KER_AXI_SLAVE_Init`, `nvt_cma_init`, `nt726xx_gpio_init` (`.text+0xc8`), `module_gpio_init`, `KER_CLK_DrvInit`, `i2c_nt726xx_platform_init`, `KER_TCON_DrvInit`, then return 0.

`cleanup_module`: `i2c_nt726xx_platform_exit`, `nvt_cma_exit`, `KER_AXI_SLAVE_Exit`, `KER_SYS_DrvExit`, `KER_CLK_DrvExit`, `KER_PMI_Exit`, `KER_Nerve_Exit`, `KER_EMU_Exit`. `module_gpio_exit`, `nt726xx_gpio_exit`, and a TCON exit are not in this body.

`GPIO_Ioctl` at `.text+0x2bbf0` does `uxtb r1, r1`, `sub r1, r1, #1`, `cmp r1, #33`, then `ldrls pc, [pc, r1, lsl #2]`. It switches on `(command & 0xff) - 1` for numbers 1..34. It does not compare the full `_IOC` word. The type-`g` command words are the NDAL3 immediates. `IIC_Ioctl` is at `.text+0x3bd70`. `vu_ioctl` is at `.text+0x3ad88`. Local `MAU_IOCtl` is at `.text+0x31b84`. Chrdev helpers: `vk_register_chrdev_region` `.text+0x3a7b4`, `vk_cdev_add` `.text+0x3a854`, `vk_unregister_chrdev_region` `.text+0x3a220`.

### ntkndphdmiswitch.ko

Description `Template HDMI Driver`. No parameters and no device strings. Globals `init_module`, `cleanup_module`, `ndp_hdmi_switch_init`, and `ndp_hdmi_switch_exit` are each at offset 0 of their section.

`init_module` straight line: `__platform_driver_register`, `memset`, `platform_device_register_full`. Failure path: `platform_driver_unregister`. `cleanup_module`: `platform_device_unregister`, `platform_driver_unregister`. The ops table `ndp_hdmi_switch_ioctl_ops` is at `.data+0` and is dispatched by `video_ioctl2`.

## VPLTIO cross-check

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. `Hal_Video_Vpltio_SendCmd` is undefined in aurora and exported by `libNDAL3.so`. The two disputed sites were re-read in `aurora.dis`. The other 43 objdump-TSV immediates match the `mov`/`movw`/`mov.w` into `r0` quoted in that TSV.

| call site | r0 | caller | note |
|---|---|---|---|
| 8ee92c | 0x44a | `PLAT_TVOUT_MacrovisionStart` |  |
| 8ee9d4 | 0x44b | `PLAT_TVOUT_MacrovisionStop` |  |
| 8eead8 | 0x44e | `PLAT_TVOUT_SendVbiData` |  |
| 8eedfc | 0x4c1 | `PLAT_TVOUT_EnableTVOut` |  |
| 8eee96 | 0x4c1 | `PLAT_TVOUT_EnableTVOut` |  |
| 8eef68 | 0x603 | `PLAT_TVOUT_EnableTVOut` |  |
| 8f1b7c | 0x2c2 | `PLAT_VIN_SetVDChannelUpDnInit` |  |
| 8f4076 | 0x284 | `PLAT_VIN_Set_HDMI_ARC` |  |
| 8f6d90 | 0x18c | `unnamed, prologue 008f682c` | not PLAT_VLIB_EnableMsg |
| 8f9324 | 0x1c9 | `PLAT_VOUT_SetPanelLvdsDriving` |  |
| 8f93ca | 0x1ca | `PLAT_VOUT_SetPanelLvdsPreEmphasis` |  |
| 8f9642 | 0x18c | `PLAT_VOUT_SetWindowBGcolor` |  |
| 8f991c | 0x1ce | `PLAT_VOUT_Set10BitDither` |  |
| 8fa52a | 0x4c3 | `PLAT_VOUT_SetDoubleFrameRate` |  |
| 8faee2 | 0x152 | `PLAT_VOUT_SetOverDrive` |  |
| 8fb088 | 0x152 | `PLAT_VOUT_LoadOverDriveTable` |  |
| 9008be | 0x503 | `PLAT_VIU_SetPhotoLBMFormat` |  |
| 90090e | 0x60b | `PLAT_VIU_GetOffsetFromVsync` |  |
| 9009c2 | 0x4c8 | `PLAT_VIU_SetTconOnOffStatus` |  |
| 90119c | 0x3c4 | `PLAT_VIU_AutoFineTune` |  |
| 924932 | 0x302 | `_Z16plat_common_initPKc` |  |
| 924a10 | 0x303 | `_Z33plat_common_set_video_path_config14_EN_DEV_SRC_IDbj` |  |
| 92d3ae | 0x1cd | `plat_drm_set_lvdsformat` |  |
| 92d7bc | 0x347 | `plat_vpltio_set_pc_position` |  |
| 92d7e4 | 0x349 | `plat_vpltio_set_pc_position` |  |
| 92d810 | 0x346 | `plat_vpltio_set_pc_position` |  |
| 92d82a | 0x348 | `plat_vpltio_set_pc_position` |  |
| 92d968 | 0x191 | `plat_vpltio_set_graphic_blending` |  |
| 92da42 | 0x18d | `plat_vpltio_set_frame_mute` |  |
| 92dbda | 0x1c7 | `plat_vpltio_set_ssc` |  |
| 92dc40 | 0x641 | `plat_vpltio_set_ssc` |  |
| 92dc92 | 0x642 | `plat_vpltio_set_ssc` |  |
| 92e052 | 0x643 | `plat_vpltio_check_hdcpkey_ready` |  |
| 92e146 | 0x1cc | `plat_vpltio_mute_lvds` |  |
| 92e208 | 0x2c2 | `plat_vpltio_inform_vd_do_ch_updown_init` |  |
| 92e2de | 0x2c1 | `plat_vpltio_channel_scan` |  |
| 92e3da | 0x1c8 | `plat_vpltio_set_lvds_clockdata_delay` |  |
| 92e4cc | 0x1c9 | `plat_vpltio_set_lvds_strength` |  |
| 92e5be | 0x1ca | `plat_vpltio_set_lvds_preemphasis` |  |
| 92e918 | 0x4c4 | `unnamed, prologue 0092e8ac` | not plat_set_dimming |
| 92ed62 | 0x481 | `plat_vin_slot_pc_mode_set` |  |
| 92edc4 | 0x482 | `plat_vin_slot_pc_mode_set` |  |
| 92f3ce | 0x285 | `plat_vin_slot_set_hdmi_phy` |  |
| 92f3ee | 0x287 | `plat_vin_slot_set_hdmi_phy` |  |
| 92f668 | 0x283 | `plat_vin_slot_is_mhl_connected` |  |

Rows the other TSV left unknown, empty, or wrong. The third column is the objdump immediate, or `n/a` when the site is the PLT stub.

| site | recorded | instruction r0 | recorded caller |
|---|---|---|---|
| 006b3d48 | UNKNOWN (empty) | n/a | `<EXTERNAL>::Hal_Video_Vpltio_SendCmd` |
| 008f4076 | UNKNOWN (empty) | 0x284 | `PLAT_VIN_Set_HDMI_ARC` |
| 008f6d90 | UNKNOWN (empty) | 0x18c | `(none)` |
| 008f93ca | UNKNOWN (empty) | 0x1ca | `PLAT_VOUT_SetPanelLvdsPreEmphasis` |
| 008f9642 | UNKNOWN (empty) | 0x18c | `PLAT_VOUT_SetWindowBGcolor` |
| 008f991c | UNKNOWN (empty) | 0x1ce | `PLAT_VOUT_Set10BitDither` |
| 008faee2 | UNKNOWN (empty) | 0x152 | `PLAT_VOUT_SetOverDrive` |
| 008fb088 | UNKNOWN (empty) | 0x152 | `PLAT_VOUT_LoadOverDriveTable` |
| 009009c2 | UNKNOWN (empty) | 0x4c8 | `PLAT_VIU_SetTconOnOffStatus` |
| 0090119c | UNKNOWN (empty) | 0x3c4 | `PLAT_VIU_AutoFineTune` |
| 0092d82a | UNKNOWN (empty) | 0x348 | `plat_vpltio_set_pc_position` |
| 0092e146 | UNKNOWN (empty) | 0x1cc | `plat_vpltio_mute_lvds` |
| 0092e3da | IMM 0x0 | 0x1c8 | `plat_vpltio_set_lvds_clockdata_delay` |
| 0092e5be | UNKNOWN (empty) | 0x1ca | `plat_vpltio_set_lvds_preemphasis` |
| 0092edc4 | UNKNOWN (empty) | 0x482 | `plat_vin_slot_pc_mode_set` |

`0092edc4` is `UNKNOWN` there because `movw r0, #1154` (`0x482`) is at `0092ed86`, outside the scanner window. No instruction between `0092ed86` and the call writes `r0`.

## GPIO names

`DEV_GPIO_GetDescription` is aurora global `00850c38`. At `00850c9a` it computes `index * 3` (`add.w r2, r8, r8, lsl #1`), loads `0x0175f350` (`movw r3, #62288`; `movt r3, #373`), then adds `index * 24` (`add.w r3, r3, r2, lsl #3`). It also loads `0x0175f0a4` into `r0` at `00850ca6`, and `movw r1, #26399` (`0x671f`) at `00850cb2`.

The 24-byte records start at file offset `0x173f350`, VMA `0x175f350`. Index `n` is `0x175f350 + n*24`.

Ids 0..207 match the TSV, from `TUNER_IIC_SDA` at `0x175f350` through `HDMI_HOTPLUG_4` at `0x17606b8`. Records checked in the dump include id 114 `AMP_RST`, id 127 `AMP_IIC_SCL`, id 128 `AMP_IIC_SDA`, id 169 `AMP_STANDBY`, id 186 `AMP_PD`, and id 196 `AMP_MUTE`.

Id 208 at `0x17606d0` is `TOTAL`. Ids 209, 211, 213, 215, 217, 219, 221, 223, 225, and 227 are the ASCII bank tags `STBC_GPF`, `STBC_GPG`, `STBC_GPI`, `MIPS_GPA`, `MIPS_GPB`, `MIPS_GPC`, `MIPS_GPD`, `MIPS_GPE`, `MIPS_GPH`, `MIPS_GPK`, and they match the TSV. They sit after the sentinel.

| id | VMA | first bytes | TSV |
|---|---|---|---|
| 210 | `0x1760700` | `38 04 04 fc 2c 04 04 fc` | omitted |
| 212 | `0x1760730` | `3c 04 04 fc 30 04 04 fc` | omitted |
| 214 | `0x1760760` | `40 04 04 fc 34 04 04 fc` | omitted |
| 216 | `0x1760790` | `20 00` | space |
| 218 | `0x17607c0` | `60 00` | backtick |
| 220 | `0x17607f0` | `a0 00 0f fd 88 00 0f fd` | omitted |
| 222 | `0x1760820` | `e0 00 0f fd c8 00 0f fd` | `à` |
| 224 | `0x1760850` | `20 00` | space |
| 226 | `0x1760880` | `60 00` | backtick |
| 241 | `0x17609e8` | `52 00` | `R` |
| 242 | `0x1760a00` | `44 00` | `D` |

The function-name table is ids 0..207, 208 names. `TOTAL` is the next record. Later slots are a different structure.

## Aurora imports of libNDAL3.so

| calls | address | symbol |
|---|---|---|
| 100 | 4e7b27f9 | `Hal_ReadDecimalKey` |
| 81 | 4e7b299d | `Hal_Debug_TraceMask` |
| 59 | 4e7af6cd | `Hal_Audio_Ctrl` |
| 50 | 4e7b2959 | `Hal_Debug_PrintMask` |
| 46 | 4e7c85fd | `Hal_Video_Vpltio_SendCmd` |
| 45 | 4e7aeac5 | `Hal_Audio_EasySelect` |
| 40 | 4e7bc13d | `Hal_OS_GetClockInMS` |
| 32 | 4e7bdb09 | `Hal_OS_MutexUnlock` |
| 23 | 4e7b9c49 | `Hal_Demux_Dvb_Close` |
| 22 | 4e7bdac5 | `Hal_OS_MutexLock` |
| 20 | 4e7b34f5 | `HAL_fe_close` |
| 19 | 4e7aea35 | `Hal_Audio_EasySet` |
| 17 | 4e7b9bcd | `Hal_Demux_Dvb_Open` |
| 17 | 4e7c3201 | `Hal_Video_Get_DrmDeviceId` |
| 15 | 4e7b44c1 | `HAL_Demod_GetStatus` |
| 15 | 4e7ae9c9 | `Hal_Audio_KItem` |
| 15 | 4e7bc211 | `Hal_OS_GetClockDiff` |
| 15 | 4e7be039 | `Hal_OS_QueueGetCount` |
| 14 | 4e7c3465 | `Hal_Video_Get_DrmPlaneInfo` |
| 13 | 4e7c8ed1 | `Hal_VideoDec_v4l2_set_ctrls` |
| 13 | 4e7c3e4d | `Hal_Video_Get_V4l2Info` |
| 12 | 4e7b33f5 | `HAL_fe_open` |
| 12 | 4e7c8d01 | `Hal_VideoDec_Resume` |
| 11 | 4e7afecd | `Hal_Audio_DualCmd` |
| 11 | 4e7ba03d | `Hal_Demux_Dvb_Set_Pesfilter` |
| 11 | 4e7c048d | `Hal_PQ_SetPictureQuality` |
| 9 | 4e7bda79 | `Hal_OS_MutexDestroy` |
| 9 | 4e7c8dfd | `Hal_VideoDec_v4l2_get_ctrls` |
| 9 | 4e7c73c9 | `Hal_Video_Libdrm_drmIoctl` |
| 8 | 4e7ae951 | `Hal_Audio_FreeCurrentControl` |
| 8 | 4e7ae511 | `Hal_Audio_GetCurrentControl` |
| 8 | 4e7aec11 | `Hal_Audio_Load_AudInfo` |
| 8 | 4e7c1ff9 | `Hal_Userdata_DisableVbiLine` |
| 8 | 4e7c8c09 | `Hal_VideoDec_Pause` |
| 8 | 4e7c7689 | `Hal_Video_Libdrm_drmModeFreeProperty` |
| 8 | 4e7c468d | `Hal_Video_VideoPath_to_PlaneType` |
| 7 | 4e7c3fa9 | `Hal_Video_Get_V4l2FdCommon` |
| 7 | 4e7c97e9 | `Hal_Vin_Open` |
| 6 | 4e7bda2d | `Hal_OS_MutexCreate` |
| 5 | 4e7b2e51 | `HAL_Demod_SetVif` |
| 5 | 4e7aec59 | `Hal_Audio_Send` |
| 5 | 4e7ade61 | `Hal_Audio_SetControl` |
| 5 | 4e7b744d | `Hal_Demux_Ctrl_CacheFlush` |
| 5 | 4e7b9169 | `Hal_Demux_Desc_Set_Descrambler_Keys` |
| 5 | 4e7b9f45 | `Hal_Demux_Dvb_Dvr_Open` |
| 5 | 4e7bf2b1 | `Hal_Iic_Write` |
| 5 | 4e7bc4d5 | `Hal_OS_TaskCreate` |
| 5 | 4e7c1db9 | `Hal_Userdata_EnableVbiLine` |
| 5 | 4e7c8925 | `Hal_VideoDec_Close` |
| 5 | 4e7c7639 | `Hal_Video_Libdrm_drmModeFreeObjectProperties` |
| 5 | 4e7c3d8d | `Hal_Video_Set_V4l2Info` |
| 5 | 4e7c9d59 | `Hal_Vin_SetStreamStart` |
| 4 | 4e7b753d | `Hal_Demux_Ctrl_Context` |
| 4 | 4e7b6e39 | `Hal_Demux_Ctrl_QueryBufferPVR` |
| 4 | 4e7b74d1 | `Hal_Demux_Ctrl_SetReadAddr` |
| 4 | 4e7b85d9 | `Hal_Demux_Desc_Cfg` |
| 4 | 4e7b8409 | `Hal_Demux_Desc_Disable` |
| 4 | 4e7b7a49 | `Hal_Demux_Desc_EnableMask` |
| 4 | 4e7b9721 | `Hal_Demux_Desc_Start` |
| 4 | 4e7b9fc5 | `Hal_Demux_Dvb_Dvr_Close` |
| 4 | 4e7b9c91 | `Hal_Demux_Dvb_Setsource` |
| 4 | 4e7bf269 | `Hal_Iic_Read` |
| 4 | 4e7c26b1 | `Hal_UserData_GetStatus` |
| 4 | 4e7c8815 | `Hal_VideoDec_Open` |
| 4 | 4e7c4df5 | `Hal_Video_Drm_Set_CaptureScreenSize_Fmt` |
| 4 | 4e7c4c89 | `Hal_Video_Drm_Set_Property` |
| 4 | 4e7c8231 | `Hal_Video_Libdrm_drmCommandWriteRead` |
| 4 | 4e7c835d | `Hal_Video_Libdrm_drmModeGetFB` |
| 4 | 4e7c7fbd | `Hal_Video_Libdrm_drmModeGetProperty` |
| 4 | 4e7c7e79 | `Hal_Video_Libdrm_drmModeObjectGetProperties` |
| 4 | 4e7c98dd | `Hal_Vin_Close` |
| 3 | 4e7b3155 | `HAL_Demod_GetVif` |
| 3 | 4e7b3351 | `HAL_dvif_open` |
| 3 | 4e7b26a9 | `Hal_Debug_Menu` |
| 3 | 4e7b6b15 | `Hal_Demux_CI_GetEncryptMem` |
| 3 | 4e7b6349 | `Hal_Demux_CI_GetStatus` |
| 3 | 4e7b6119 | `Hal_Demux_CI_Init` |
| 3 | 4e7b6681 | `Hal_Demux_CI_Read_Reg` |
| 3 | 4e7b6291 | `Hal_Demux_CI_Reset` |
| 3 | 4e7b64e5 | `Hal_Demux_CI_Set` |
| 3 | 4e7b6769 | `Hal_Demux_CI_Write_Reg` |
| 3 | 4e7b6db1 | `Hal_Demux_Ctrl_Hwmem_Flush_Range` |
| 3 | 4e7b6ecd | `Hal_Demux_Ctrl_Mmap` |
| 3 | 4e7b76a9 | `Hal_Demux_Desc_CheckDpHandle` |
| 3 | 4e7b75c5 | `Hal_Demux_Desc_CheckHandle` |
| 3 | 4e7b9d79 | `Hal_Demux_Dvb_Stop` |
| 3 | 4e7ba211 | `Hal_Demux_Dvb_TsData_Poll` |
| 3 | 4e7ba261 | `Hal_Demux_Dvb_TsData_Read` |
| 3 | 4e7be3ed | `Hal_Gpio_Init` |
| 3 | 4e7be4cd | `Hal_Gpio_Read` |
| 3 | 4e7bee99 | `Hal_Iic_Init` |
| 3 | 4e7bdb4d | `Hal_OS_QueueCreate` |
| 3 | 4e7bdd6d | `Hal_OS_QueueDestroy` |
| 3 | 4e7bdee9 | `Hal_OS_QueuePeek` |
| 3 | 4e7bdf65 | `Hal_OS_QueueRecv` |
| 3 | 4e7bde11 | `Hal_OS_QueueSend` |
| 3 | 4e7bc8f1 | `Hal_OS_TaskWaitForExit` |
| 3 | 4e7c3bed | `Hal_Video_DrmInit` |
| 3 | 4e7c2ff9 | `Hal_Video_DrmShareMemCreate` |
| 3 | 4e7c472d | `Hal_Video_Drm_Set_DpSource` |
| 3 | 4e7c481d | `Hal_Video_Drm_Set_VideoSize` |
| 3 | 4e7c4491 | `Hal_Video_Get_PlaySpeed` |
| 3 | 4e7c41f9 | `Hal_Video_Get_V4l2Drm` |
| 3 | 4e7c7769 | `Hal_Video_Libdrm_drmModeAddFB2` |
| 3 | 4e7c83d5 | `Hal_Video_Libdrm_drmModeFreeFB` |
| 3 | 4e7c75f1 | `Hal_Video_Libdrm_drmModeFreePlane` |
| 3 | 4e7c7d79 | `Hal_Video_Libdrm_drmModeGetPlane` |
| 3 | 4e7c8165 | `Hal_Video_Libdrm_drmModeObjectSetProperty` |
| 3 | 4e7c43ed | `Hal_Video_Set_PlaySpeed` |
| 3 | 4e7c4071 | `Hal_Video_Set_V4l2FdCommon` |
| 3 | 4e7c8415 | `Hal_Video_Vpltio_Init` |
| 3 | 4e7c993d | `Hal_Vin_SelectInput` |
| 3 | 4e7c99a1 | `Hal_Vin_SetColorSystem` |
| 2 | 4e7b569d | `HAL_DEMOD_EXT_DVIF_SetStandby` |
| 2 | 4e7b55dd | `HAL_DEMOD_EXT_DVIF_SetTunerType` |
| 2 | 4e7b5dad | `HAL_DEMOD_EXT_DeliverConfig` |
| 2 | 4e7b5ba9 | `HAL_DEMOD_EXT_GetBlindScanNextFreq` |
| 2 | 4e7b5901 | `HAL_DEMOD_EXT_GetFrequency` |
| 2 | 4e7b5f25 | `HAL_DEMOD_EXT_GetNIMDVBSInfo` |
| 2 | 4e7b5835 | `HAL_DEMOD_EXT_GetSymbolRate` |
| 2 | 4e7b5ad1 | `HAL_DEMOD_EXT_SetScanMode` |
| 2 | 4e7b5c85 | `HAL_DEMOD_EXT_SetSpectralINV` |
| 2 | 4e7b5d45 | `HAL_DEMOD_EXT_SetStandy` |
| 2 | 4e7b5ce5 | `HAL_DEMOD_EXT_SetTsOutput` |
| 2 | 4e7b59cd | `HAL_DEMOD_EXT_SetUnicableIF` |
| 2 | 4e7b5c21 | `HAL_DEMOD_EXT_Set_diseqc_control` |
| 2 | 4e7b2cd9 | `HAL_Demod_Init` |
| 2 | 4e7b38dd | `HAL_Demod_Set` |
| 2 | 4e7ad941 | `Hal_Audio_Mixer_Ctrl_Init` |
| 2 | 4e7aeeb1 | `Hal_Audio_Protect_Init` |
| 2 | 4e7af62d | `Hal_Audio_Proxy_Init` |
| 2 | 4e7b6a0d | `Hal_Demux_CI_InstallCallback` |
| 2 | 4e7b6a71 | `Hal_Demux_CI_Mmap` |
| 2 | 4e7b6855 | `Hal_Demux_CI_Read_CIs_Memory` |
| 2 | 4e7b6941 | `Hal_Demux_CI_Write_FCR_Value` |
| 2 | 4e7b6b9d | `Hal_Demux_Ctrl_Init` |
| 2 | 4e7b6cf9 | `Hal_Demux_Ctrl_SysClose` |
| 2 | 4e7b6cad | `Hal_Demux_Ctrl_SysOpen` |
| 2 | 4e7b7e59 | `Hal_Demux_Desc_Close` |
| 2 | 4e7b76ed | `Hal_Demux_Desc_Init` |
| 2 | 4e7b7b61 | `Hal_Demux_Desc_Open` |
| 2 | 4e7b808d | `Hal_Demux_Desc_Set_Pid` |
| 2 | 4e7b999d | `Hal_Demux_Desc_Stop` |
| 2 | 4e7b9e49 | `Hal_Demux_Dvb_Dvr_Flush` |
| 2 | 4e7b9ec5 | `Hal_Demux_Dvb_Dvr_GetPlayBuffer` |
| 2 | 4e7ba001 | `Hal_Demux_Dvb_Dvr_Write` |
| 2 | 4e7b9db9 | `Hal_Demux_Dvb_GetStc` |
| 2 | 4e7ba12d | `Hal_Demux_Dvb_Set_Sectionfilter` |
| 2 | 4e7bee15 | `Hal_Gpio_Conf_CITSIF` |
| 2 | 4e7be881 | `Hal_Gpio_Config` |
| 2 | 4e7bec5d | `Hal_Gpio_GetDriving` |
| 2 | 4e7beb91 | `Hal_Gpio_PinctrlWrite` |
| 2 | 4e7bed41 | `Hal_Gpio_SetDriving` |
| 2 | 4e7be755 | `Hal_Gpio_Write` |
| 2 | 4e7bceed | `Hal_OS_TaskDelay` |
| 2 | 4e7bcec1 | `Hal_OS_TaskSuicide` |
| 2 | 4e7be2f1 | `Hal_OS_runShell` |
| 2 | 4e7c0065 | `Hal_PQ_ClearWhite` |
| 2 | 4e7c00d1 | `Hal_PQ_ClearWhite2` |
| 2 | 4e7bfc4d | `Hal_PQ_ColorEnhanceBypass` |
| 2 | 4e7bfb0d | `Hal_PQ_ColorSpaceCustom` |
| 2 | 4e7bfcb5 | `Hal_PQ_Comp_Phase` |
| 2 | 4e7bf6d9 | `Hal_PQ_ContrastDimming` |
| 2 | 4e7c0949 | `Hal_PQ_GetGammaData` |
| 2 | 4e7c0a29 | `Hal_PQ_GetGammaSinglePoint` |
| 2 | 4e7c073d | `Hal_PQ_GetHDRStatus` |
| 2 | 4e7c0699 | `Hal_PQ_GetHistogram` |
| 2 | 4e7c0aa1 | `Hal_PQ_GetPanelCursor` |
| 2 | 4e7bfffd | `Hal_PQ_PPQT_Version` |
| 2 | 4e7bf8a1 | `Hal_PQ_PostPostSharpness` |
| 2 | 4e7bf835 | `Hal_PQ_PostSharpness` |
| 2 | 4e7c0279 | `Hal_PQ_ReadAPL_CB` |
| 2 | 4e7c0631 | `Hal_PQ_ReadDP_RGB_APL` |
| 2 | 4e7c0425 | `Hal_PQ_SetBIPPMode` |
| 2 | 4e7bfa3d | `Hal_PQ_SetBLE` |
| 2 | 4e7bfaa5 | `Hal_PQ_SetBLEStartPointMin` |
| 2 | 4e7c02e1 | `Hal_PQ_SetBlackAdjust` |
| 2 | 4e7c03b9 | `Hal_PQ_SetCCEMode` |
| 2 | 4e7c0811 | `Hal_PQ_SetCMOnOff` |
| 2 | 4e7bfd1d | `Hal_PQ_SetColorCompensation` |
| 2 | 4e7c07a9 | `Hal_PQ_SetDVPanelConfig` |
| 2 | 4e7c034d | `Hal_PQ_SetFRCDemoModeOnOff` |
| 2 | 4e7bff29 | `Hal_PQ_SetFilmMode` |
| 2 | 4e7c08e1 | `Hal_PQ_SetGammaData` |
| 2 | 4e7c09b1 | `Hal_PQ_SetGammaSinglePoint` |
| 2 | 4e7c0b59 | `Hal_PQ_SetHDR10P_BYPASS` |
| 2 | 4e7bf90d | `Hal_PQ_SetHLPF` |
| 2 | 4e7bfec1 | `Hal_PQ_SetHdmiBlackLevel` |
| 2 | 4e7bf605 | `Hal_PQ_SetHdmiPC` |
| 2 | 4e7bf66d | `Hal_PQ_SetOffsetGain` |
| 2 | 4e7c0879 | `Hal_PQ_SetPictureMode` |
| 2 | 4e7bfbe5 | `Hal_PQ_SetRGBOnlyMode` |
| 2 | 4e7bfd85 | `Hal_PQ_SetSaturation` |
| 2 | 4e7bfded | `Hal_PQ_SetSaturationSub` |
| 2 | 4e7bff91 | `Hal_PQ_SetScartUltraBlack` |
| 2 | 4e7bfe55 | `Hal_PQ_SetxvYCC` |
| 2 | 4e7bf7c9 | `Hal_PQ_VideoDecoder` |
| 2 | 4e7bf741 | `Hal_PQ_WCE` |
| 2 | 4e7c0c39 | `Hal_Pwm_Init` |
| 2 | 4e7c0d41 | `Hal_Pwm_Open` |
| 2 | 4e7c0fc1 | `Hal_Pwm_Set_ByChannel` |
| 2 | 4e7c8759 | `Hal_VideoDec_Init` |
| 2 | 4e7c8af1 | `Hal_VideoDec_Streamoff` |
| 2 | 4e7c89e1 | `Hal_VideoDec_Streamon` |
| 2 | 4e7c95f1 | `Hal_VideoDec_poll_event` |
| 2 | 4e7c939d | `Hal_VideoDec_subscribe_event` |
| 2 | 4e7c8f89 | `Hal_VideoDec_v4l2_set_fmt` |
| 2 | 4e7c5879 | `Hal_Video_Drm_CaptureFrame` |
| 2 | 4e7c5069 | `Hal_Video_Drm_CaptureInput` |
| 2 | 4e7c61c5 | `Hal_Video_Drm_CaptureScreen` |
| 2 | 4e7c4ead | `Hal_Video_Drm_ReleaseDumpBuffer` |
| 2 | 4e7c4a89 | `Hal_Video_Drm_Set_Mute` |
| 2 | 4e7c4b89 | `Hal_Video_Drm_Set_StillVideo` |
| 2 | 4e7c4989 | `Hal_Video_Drm_Set_Sync` |
| 2 | 4e7c75a9 | `Hal_Video_Libdrm_drmModeFreePlaneResources` |
| 2 | 4e7c8289 | `Hal_Video_Libdrm_drmModeGetCrtc` |
| 2 | 4e7c7c91 | `Hal_Video_Libdrm_drmModeGetPlaneResources` |
| 2 | 4e7c8315 | `Hal_Video_Libdrm_drmModeRmFB` |
| 2 | 4e7c81bd | `Hal_Video_Libdrm_drmModeSetPlane` |
| 2 | 4e7c3efd | `Hal_Video_Reset_V4l2Info` |
| 2 | 4e7c4139 | `Hal_Video_Set_V4l2Drm` |
| 2 | 4e7c3cc5 | `Hal_Video_V4l2InfoInit` |
| 2 | 4e7c9c2d | `Hal_Vin_GetHdmiStatus` |
| 2 | 4e7c9a85 | `Hal_Vin_GetLock` |
| 2 | 4e7c9afd | `Hal_Vin_GetRes` |
| 2 | 4e7c9a09 | `Hal_Vin_GetVDLock` |
| 2 | 4e7c9b95 | `Hal_Vin_SetInputRes` |
| 2 | 4e7c9cc1 | `Hal_Vin_SetPcMode` |

## Aurora imports of libNDAL_EX.so

| calls | address | symbol |
|---|---|---|
| 62 | 4db58409 | `NDAL_VenderVideoExt_SendSetCmd` |
| 7 | 4db69aa5 | `ND_TSPU_Desc_PID_Disable` |
| 6 | 4db581f9 | `NDAL_VenderVideoExt_SendGetCmd` |
| 6 | 4db59d11 | `NDAL_VideoExt_OpenDevice` |
| 5 | 4db58af9 | `NDAL_VideoExt_GetHDMIExtraInfo` |
| 5 | 4db5acb1 | `NDAL_VideoExt_STBC_OpTcnt` |
| 5 | 4db59f0d | `NDAL_VideoExt_VD_DeliverConfig` |
| 4 | 4db5a255 | `NDAL_VideoExt_STBC_StopWDT` |
| 4 | 4db58a6d | `NDAL_VideoExt_Set_HDMI_ClrRatioScramble` |
| 4 | 4db69eb1 | `NDOSD_EnableProtectionArea` |
| 4 | 4db67671 | `Nd_NIM_VD_ConnectATV` |
| 3 | 4db57019 | `NDAL_CommonExt_IsTighten` |
| 3 | 4db580d9 | `NDAL_VenderVideoExt_Send_DolbyVision_GetCmd` |
| 3 | 4db598ad | `NDAL_VideoExt_CEC_GeneralPollingMessage` |
| 3 | 4db59779 | `NDAL_VideoExt_CEC_Init` |
| 3 | 4db59971 | `NDAL_VideoExt_CEC_Process_Command` |
| 3 | 4db59a85 | `NDAL_VideoExt_CEC_Set_EDID_ATMOS_Bit` |
| 3 | 4db58d09 | `NDAL_VideoExt_FreeATVFB_and_StopAlloc` |
| 3 | 4db596e9 | `NDAL_VideoExt_FreeFRCFB` |
| 3 | 4db58f75 | `NDAL_VideoExt_GetIdentStatus` |
| 3 | 4db5871d | `NDAL_VideoExt_GetSyncInfo` |
| 3 | 4db5895d | `NDAL_VideoExt_MHL_KeyPressRelease` |
| 3 | 4db5a871 | `NDAL_VideoExt_STBC_SyncMcuDB` |
| 3 | 4db5918d | `NDAL_VideoExt_SetFrameBGColor` |
| 3 | 4db589e5 | `NDAL_VideoExt_SetHDMIRXCLK` |
| 3 | 4db58f15 | `NDAL_VideoExt_SetIdentMapping` |
| 3 | 4db58e9d | `NDAL_VideoExt_SetIdentSequence` |
| 3 | 4db587b9 | `NDAL_VideoExt_SetPlayFieldType` |
| 3 | 4db59639 | `NDAL_VideoExt_SetUp2DTo3D` |
| 3 | 4db59265 | `NDAL_VideoExt_SetVX1Partition` |
| 3 | 4db59dd5 | `NDAL_VideoExt_VD_GetStatus` |
| 3 | 4db69e25 | `NDOSD_SetAutomuteTrigger` |
| 3 | 4db69c11 | `ND_TSPU_Desc_Get_ScrambleBit` |
| 3 | 4db69941 | `ND_TSPU_Desc_PID_Enable` |
| 3 | 4db69891 | `ND_TSPU_FILT_GetPTS` |
| 3 | 4db697e5 | `ND_TSPU_Get_Audio_Filter_Handle` |
| 2 | 4db6511d | `HAL_CreateBuffer` |
| 2 | 4db64e49 | `HAL_ReleaseBuffer` |
| 2 | 4db64fa9 | `HAL_RenderBuffer` |
| 2 | 4db57f05 | `NDAL_VenderVideoExt_Send_DolbyVision_SetCmd` |
| 2 | 4db5aef9 | `NDAL_VideoExt_BLAST_RX` |
| 2 | 4db5ae71 | `NDAL_VideoExt_BLAST_SetRcProtocol` |
| 2 | 4db59a15 | `NDAL_VideoExt_CEC_CheckCmdCount` |
| 2 | 4db59809 | `NDAL_VideoExt_CEC_PollingMessage` |
| 2 | 4db588d5 | `NDAL_VideoExt_GetModeInfo` |
| 2 | 4db58d79 | `NDAL_VideoExt_Get_SRAM_EDID` |
| 2 | 4db5a1e5 | `NDAL_VideoExt_STBC_FreshWDT` |
| 2 | 4db5ade9 | `NDAL_VideoExt_STBC_GetMcuWakeUpTime` |
| 2 | 4db5a745 | `NDAL_VideoExt_STBC_GetMcuWatchDogTimeout` |
| 2 | 4db5b045 | `NDAL_VideoExt_STBC_GetPcbVersion` |
| 2 | 4db5a6b5 | `NDAL_VideoExt_STBC_GetPowerOnEvent` |
| 2 | 4db5afb5 | `NDAL_VideoExt_STBC_GetSTBCSvn` |
| 2 | 4db5a7d1 | `NDAL_VideoExt_STBC_GetSTBCVersion` |
| 2 | 4db5a459 | `NDAL_VideoExt_STBC_GetStbcGpio` |
| 2 | 4db5a5d5 | `NDAL_VideoExt_STBC_PowerOffByKeypad` |
| 2 | 4db5a645 | `NDAL_VideoExt_STBC_PowerSavingNow` |
| 2 | 4db5aa31 | `NDAL_VideoExt_STBC_ResetByHWWDT` |
| 2 | 4db5a9ad | `NDAL_VideoExt_STBC_SetMcuDCAutoOn` |
| 2 | 4db5ac2d | `NDAL_VideoExt_STBC_SetMcuFakeSTROnOff` |
| 2 | 4db5a8e1 | `NDAL_VideoExt_STBC_SetMcuLEDState` |
| 2 | 4db5ad5d | `NDAL_VideoExt_STBC_SetMcuWakeUpTime` |
| 2 | 4db5aba9 | `NDAL_VideoExt_STBC_SetMcuWifiPwr` |
| 2 | 4db5aaa1 | `NDAL_VideoExt_STBC_SetMcuWoLOnOff` |
| 2 | 4db5ab25 | `NDAL_VideoExt_STBC_SetMcuWoWOnOff` |
| 2 | 4db5a3b9 | `NDAL_VideoExt_STBC_SetStbcGpio` |
| 2 | 4db5a335 | `NDAL_VideoExt_STBC_SetWDT` |
| 2 | 4db59569 | `NDAL_VideoExt_Set3D` |
| 2 | 4db592f1 | `NDAL_VideoExt_SetDPFrameRate` |
| 2 | 4db58bf9 | `NDAL_VideoExt_SetHDMIExtraCtrl` |
| 2 | 4db5939d | `NDAL_VideoExt_SetNonlinearScalingParameter` |
| 2 | 4db58fd5 | `NDAL_VideoExt_SetPanel` |
| 2 | 4db58e0d | `NDAL_VideoExt_Set_SRAM_EDID` |
| 2 | 4db59c99 | `NDAL_VideoExt_VBI_IsDataReady` |
| 2 | 4db5b2a5 | `NDALex_GPU_Close` |
| 2 | 4db5c08d | `NDALex_GPU_Open` |
| 2 | 4db5bc3d | `NDALex_GPU_Scale` |
| 2 | 4db69d31 | `NDOSD_GetAlphaCount` |
| 2 | 4db6a9b9 | `NDOSD_GetAutomuteStatus` |
| 2 | 4db6a221 | `NDOSD_GetColorMode` |
| 2 | 4db6a8c5 | `NDOSD_GetCurrentBufferOrder` |
| 2 | 4db6a5fd | `NDOSD_GetDispWin` |
| 2 | 4db6a049 | `NDOSD_GetPhyBufferAddress` |
| 2 | 4db6ad5d | `NDOSD_GetPlaneOrder` |
| 2 | 4db6aaad | `NDOSD_GetStrLogoPhyAddrFromCMABuffer` |
| 2 | 4db6a3f9 | `NDOSD_GetStride` |
| 2 | 4db6aba1 | `NDOSD_ReleaseCMABuffer` |
| 2 | 4db6a13d | `NDOSD_SetColorMode` |
| 2 | 4db6a4ed | `NDOSD_SetDispWin` |
| 2 | 4db6a7e1 | `NDOSD_SetForceBufferOrder` |
| 2 | 4db69f55 | `NDOSD_SetPhyBufferAddress` |
| 2 | 4db6ac91 | `NDOSD_SetPlaneOrder` |
| 2 | 4db6a315 | `NDOSD_SetStride` |
| 2 | 4db6a715 | `NDOSD_SetUpdateEnable` |
| 2 | 4db571d9 | `ND_CommonExt_CheckSecuriry` |
| 2 | 4db570d1 | `ND_CommonExt_EnableSecuriry` |
| 2 | 4db57225 | `ND_CommonExt_ForceDisableSecurity` |
| 2 | 4db68861 | `ND_SYS_CheckAddress` |
| 2 | 4db69519 | `ND_SYS_Init` |
| 2 | 4db688d5 | `ND_SYS_Phys2Virt` |
| 2 | 4db696f1 | `ND_TSPU_CRC32_Check` |
| 2 | 4db69b61 | `ND_TSPU_Desc_Get_ScrambleStatus` |
| 2 | 4db69cb9 | `ND_TSPU_MuxStart` |
| 2 | 4db69cf5 | `ND_TSPU_MuxStop` |
| 2 | 4db577d5 | `_Z23NDAL_GfxExt_SetPreAlpha22_Nd_Gfxex_OSD_PLANE_IDb` |
| 2 | 4db5751d | `_Z24NDAL_GfxExt_SET_CROP_WIN22_Nd_Gfxex_OSD_PLANE_IDP8_ST_RECTS1_` |
| 2 | 4db57a89 | `_Z25NDAL_SoundExt_SetPCMParam24ND_SoundExt_CHANNEL_ID_kP24ND_SoundExt_PCM_Params_t` |
| 2 | 4db57b19 | `_Z26NDAL_SoundExt_DmixSoundOut24ND_SoundExt_CHANNEL_ID_kPvj` |
| 2 | 4db573bd | `_Z27NDAL_GfxExt_EnableOSDEngineb` |
| 2 | 4db57319 | `_Z27NDAL_GfxExt_SetTransparency22_Nd_Gfxex_OSD_PLANE_IDj` |
| 2 | 4db579f9 | `_Z29NDAL_SoundExt_OpenDmixChannel24ND_SoundExt_CHANNEL_ID_k` |
| 2 | 4db57bd9 | `_Z30NDAL_SoundExt_CloseDmixChannel24ND_SoundExt_CHANNEL_ID_k` |
| 2 | 4db57879 | `_Z31NDAL_SoundExt_SetDmixChannelVol24ND_SoundExt_CHANNEL_ID_kj` |

## Appendix: dynamic functions in libNDAL3.so

```
4e7ca46d AUD_DBG_GetStr
4e7cba31 calc_DBE_coefficient
4e7ac52d _CloseClientHeader
4e7ac16d _CloseServerHeader
4e7b0f55 compress_close
4e7b1fc1 compress_close_device
4e7b1619 compress_drain
4e7b1c9d compress_get_bufinfo
4e7b1b09 compress_get_codec_caps
4e7b1e7d compress_get_debug_conf
4e7b0c99 compress_get_error
4e7b0fb5 compress_get_hpointer
4e7b1f11 compress_get_pcm_metadata
4e7b1b89 compress_get_pts
4e7b1df1 compress_get_secure_buf
4e7b1095 compress_get_tstamp
4e7b171d compress_next_track
4e7b1a09 compress_nonblock
4e7b0d65 compress_open
4e7b168d compress_partial_drain
4e7b1549 compress_pause
4e7b1d41 compress_pcm_read
4e7b12bd compress_read
4e7b21bd compress_read_pcm
4e7b15bd compress_resume
4e7b17b5 compress_set_gapless_metadata
4e7b19cd compress_set_max_poll_wait
4e7b1c15 compress_set_pts
4e7b2045 compress_set_sync
4e7b1459 compress_start
4e7b14d5 compress_stop
4e7b1a4d compress_wait
4e7b1121 compress_write
4e7b20c9 compress_write_strm
4e7ac585 _CreateClientHeader
4e7ac1dd _CreateServerHeader
4e7cb989 DBE_CLIP_16bit
4e7cd271 _dbgGetPostParams
4e7ce141 _dbgPPDisplyConfig
4e7cbfa1 _dbgPPSetConfig
4e7cd57d _dbgSetPostParams
4e7caec1 DRV_AUD_MSC_FilterDesign
4e7cb6f1 DRV_AUD_MSC_FilterTblFloat2FXP
4e7cb3d1 DRV_AUD_MSC_FXP16
4e7cb4a9 DRV_AUD_MSC_FXP32
4e7d0400 _fini
4e7b0639 _FreeGemBuffer
4e7b06c5 _GetCardXDRMFd
4e7b0761 HAL_AllocBuffer
4e7ac9d9 Hal_Audio_Cmd_Exit
4e7ac929 Hal_Audio_Cmd_Init
4e7acb8d Hal_Audio_Cmd_Read_Key
4e7aca91 Hal_Audio_Cmd_Send_Msg
4e7aca35 Hal_Audio_Cmd_SockFd_Get
4e7af6cd Hal_Audio_Ctrl
4e7b03ed Hal_Audio_Debug
4e7afecd Hal_Audio_DualCmd
4e7aea7d Hal_Audio_EasyGet
4e7aeac5 Hal_Audio_EasySelect
4e7aea35 Hal_Audio_EasySet
4e7ad4f9 HalAudioEnc_Close
4e7acea1 HalAudioEnc_Init
4e7acde5 HalAudioEnc_InstallCallback
4e7acf09 HalAudioEnc_Open
4e7ad2f9 HalAudioEnc_Start
4e7ad3fd HalAudioEnc_Stop
4e7ae951 Hal_Audio_FreeCurrentControl
4e7ae179 Hal_Audio_GetControl
4e7ae511 Hal_Audio_GetCurrentControl
4e7aed71 Hal_Audio_GetInfoMutexId
4e7ad5f1 hal_audio_info_speaker_xch
4e7ae9c9 Hal_Audio_KItem
4e7aec11 Hal_Audio_Load_AudInfo
4e7ad941 Hal_Audio_Mixer_Ctrl_Init
4e7af579 Hal_Audio_NL_CB
4e7af12d Hal_Audio_Protect_FreeResource
4e7aeeb1 Hal_Audio_Protect_Init
4e7af409 Hal_Audio_Protect_InstallTrace
4e7af199 Hal_Audio_Protect_Lock
4e7aef8d Hal_Audio_Protect_RequestResource
4e7aef0d Hal_Audio_Protect_Terminate
4e7af2f5 Hal_Audio_Protect_Unlock
4e7af62d Hal_Audio_Proxy_Init
4e7aec59 Hal_Audio_Send
4e7ade61 Hal_Audio_SetControl
4e7aed29 Hal_Audio_SetNotifyAction
4e7b2835 Hal_Debug_DPConfig
4e7b26a9 Hal_Debug_Menu
4e7b2959 Hal_Debug_PrintMask
4e7b25d5 Hal_Debug_SetPrint
4e7b2501 Hal_Debug_SetTrace
4e7b299d Hal_Debug_TraceMask
4e7b32dd HalDemod_CheckPllLock_S
4e7b3319 HAL_Demod_Debug
4e7b5b49 HAL_DEMOD_EXT_BlinScan_Update
4e7b5dad HAL_DEMOD_EXT_DeliverConfig
4e7b569d HAL_DEMOD_EXT_DVIF_SetStandby
4e7b55dd HAL_DEMOD_EXT_DVIF_SetTunerType
4e7b5741 HAL_DEMOD_EXT_DVIF_SetupConfig
4e7b5ba9 HAL_DEMOD_EXT_GetBlindScanNextFreq
4e7b5901 HAL_DEMOD_EXT_GetFrequency
4e7b5f25 HAL_DEMOD_EXT_GetNIMDVBSInfo
4e7b5835 HAL_DEMOD_EXT_GetSymbolRate
4e7b5c21 HAL_DEMOD_EXT_Set_diseqc_control
4e7b5ad1 HAL_DEMOD_EXT_SetScanMode
4e7b5c85 HAL_DEMOD_EXT_SetSpectralINV
4e7b5d45 HAL_DEMOD_EXT_SetStandy
4e7b5ce5 HAL_DEMOD_EXT_SetTsOutput
4e7b59cd HAL_DEMOD_EXT_SetUnicableIF
4e7b3231 HAL_Demod_Get
4e7b5341 HalDemod_GetCurrentPLP
4e7b3531 HalDemod_GetPLP_List
4e7b44c1 HAL_Demod_GetStatus
4e7b3155 HAL_Demod_GetVif
4e7b2cd9 HAL_Demod_Init
4e7b36e1 HalDemod_send_diseqc_sequence
4e7b38dd HAL_Demod_Set
4e7b2e51 HAL_Demod_SetVif
4e7b6475 Hal_Demux_CI_Get
4e7b6b15 Hal_Demux_CI_GetEncryptMem
4e7b6349 Hal_Demux_CI_GetStatus
4e7b6119 Hal_Demux_CI_Init
4e7b6a0d Hal_Demux_CI_InstallCallback
4e7b6a71 Hal_Demux_CI_Mmap
4e7b6855 Hal_Demux_CI_Read_CIs_Memory
4e7b6681 Hal_Demux_CI_Read_Reg
4e7b6291 Hal_Demux_CI_Reset
4e7b64e5 Hal_Demux_CI_Set
4e7b6941 Hal_Demux_CI_Write_FCR_Value
4e7b6769 Hal_Demux_CI_Write_Reg
4e7b7581 Hal_Demux_Crypto_Context
4e7b744d Hal_Demux_Ctrl_CacheFlush
4e7b753d Hal_Demux_Ctrl_Context
4e7b7259 Hal_Demux_Ctrl_Crypto_Decrypt
4e7b70e9 Hal_Demux_Ctrl_Crypto_Encrypt
4e7b73c9 Hal_Demux_Ctrl_Crypto_GetPRNG
4e7b6f59 Hal_Demux_Ctrl_Crypto_PVR
4e7b7029 Hal_Demux_Ctrl_Crypto_SetPvrKey
4e7b6db1 Hal_Demux_Ctrl_Hwmem_Flush_Range
4e7b6b9d Hal_Demux_Ctrl_Init
4e7b6d35 Hal_Demux_Ctrl_Inv_Cache
4e7b6ecd Hal_Demux_Ctrl_Mmap
4e7b6e39 Hal_Demux_Ctrl_QueryBufferPVR
4e7b74d1 Hal_Demux_Ctrl_SetReadAddr
4e7b6cf9 Hal_Demux_Ctrl_SysClose
4e7b6cad Hal_Demux_Ctrl_SysOpen
4e7b85d9 Hal_Demux_Desc_Cfg
4e7b76a9 Hal_Demux_Desc_CheckDpHandle
4e7b75c5 Hal_Demux_Desc_CheckHandle
4e7b7e59 Hal_Demux_Desc_Close
4e7b8ae1 Hal_Demux_Desc_Config
4e7b8409 Hal_Demux_Desc_Disable
4e7b7a49 Hal_Demux_Desc_EnableMask
4e7b9b29 Hal_Demux_Desc_EnableMask4ECP
4e7b76ed Hal_Demux_Desc_Init
4e7b7b61 Hal_Demux_Desc_Open
4e7b7809 Hal_Demux_Desc_Set_Bypass
4e7b9169 Hal_Demux_Desc_Set_Descrambler_Keys
4e7b808d Hal_Demux_Desc_Set_Pid
4e7b9721 Hal_Demux_Desc_Start
4e7b999d Hal_Demux_Desc_Stop
4e7b9c49 Hal_Demux_Dvb_Close
4e7b9fc5 Hal_Demux_Dvb_Dvr_Close
4e7b9e49 Hal_Demux_Dvb_Dvr_Flush
4e7b9ec5 Hal_Demux_Dvb_Dvr_GetPlayBuffer
4e7b9f45 Hal_Demux_Dvb_Dvr_Open
4e7ba001 Hal_Demux_Dvb_Dvr_Write
4e7b9d1d Hal_Demux_Dvb_Getsource
4e7b9db9 Hal_Demux_Dvb_GetStc
4e7b9bcd Hal_Demux_Dvb_Open
4e7ba03d Hal_Demux_Dvb_Set_Pesfilter
4e7ba12d Hal_Demux_Dvb_Set_Sectionfilter
4e7b9c91 Hal_Demux_Dvb_Setsource
4e7b9d79 Hal_Demux_Dvb_Stop
4e7ba211 Hal_Demux_Dvb_TsData_Poll
4e7ba261 Hal_Demux_Dvb_TsData_Read
4e7b3351 HAL_dvif_open
4e7b34f5 HAL_fe_close
4e7b4301 HAL_fe_get_frontend
4e7b43b1 HAL_fe_get_parms
4e7b33f5 HAL_fe_open
4e7b37d5 HAL_fe_set_parms
4e7b0bc1 HAL_FreeBuffer
4e7bee15 Hal_Gpio_Conf_CITSIF
4e7be881 Hal_Gpio_Config
4e7be991 Hal_Gpio_GetDirection
4e7bec5d Hal_Gpio_GetDriving
4e7be3ed Hal_Gpio_Init
4e7bea81 Hal_Gpio_PinctrlRead
4e7beb91 Hal_Gpio_PinctrlWrite
4e7be4cd Hal_Gpio_Read
4e7bed41 Hal_Gpio_SetDriving
4e7be755 Hal_Gpio_Write
4e7b09b9 HAL_Handle2IPCName
4e7b0a55 HAL_Handle2PhyAdr
4e7b08c1 HAL_Handle2VirAdr
4e7ca21d Hal_IfCmd_Exit
4e7ca16d Hal_IfCmd_Init
4e7ca3b9 Hal_IfCmd_Read_Key
4e7ca2d5 Hal_IfCmd_Send_Msg
4e7ca279 Hal_IfCmd_SockFd_Get
4e7bee99 Hal_Iic_Init
4e7bf269 Hal_Iic_Read
4e7bf2b1 Hal_Iic_Write
4e7b0a9d HAL_IPCName2Handle
4e7c9dc1 Hal_Menu_Common
4e7c9fbd Hal_Menu_Driver
4e7d0229 Hal_Menu_PostProcess
4e7ac6fd HAL_NIPC_ClientConnectServer
4e7ac6b5 HAL_NIPC_ClientSetCallBackF
4e7ac805 HAL_NIPC_CloseClient
4e7ac4e5 HAL_NIPC_CloseServer
4e7ac609 HAL_NIPC_CreateClient
4e7ac2d1 HAL_NIPC_CreateServer
4e7ac8b9 HAL_NIPC_Read
4e7ac469 HAL_NIPC_ServerSetCallBackF
4e7ac841 HAL_NIPC_Write
4e7bc19d Hal_OS_CalcClockDiff
4e7bc335 Hal_OS_ConvertSecToDateTime
4e7bc459 Hal_OS_GetClockDate
4e7bc211 Hal_OS_GetClockDiff
4e7bc13d Hal_OS_GetClockInMS
4e7bc0e1 Hal_OS_GetClockInUS
4e7bc259 Hal_OS_GetClockRTC
4e7bc005 Hal_OS_Init
4e7bd52d Hal_OS_IPCSemaphoreCreate
4e7bd6ad Hal_OS_IPCSemaphoreDestroy
4e7bd81d Hal_OS_IPCSemaphoreSignal
4e7bd75d Hal_OS_IPCSemaphoreWait
4e7bd8d9 Hal_OS_IPCShmCreate
4e7bd989 Hal_OS_IPCShmDestroy
4e7be285 Hal_OS_MiscRandom
4e7bda2d Hal_OS_MutexCreate
4e7bda79 Hal_OS_MutexDestroy
4e7bdac5 Hal_OS_MutexLock
4e7bdb09 Hal_OS_MutexUnlock
4e7be1ad Hal_OS_QueueAddHookOp
4e7be0d1 Hal_OS_QueueCheckEmpty
4e7be081 Hal_OS_QueueCheckFull
4e7bdb4d Hal_OS_QueueCreate
4e7bdd6d Hal_OS_QueueDestroy
4e7be039 Hal_OS_QueueGetCount
4e7bdee9 Hal_OS_QueuePeek
4e7bdf65 Hal_OS_QueueRecv
4e7be219 Hal_OS_QueueRemoveHookOp
4e7bde11 Hal_OS_QueueSend
4e7be121 Hal_OS_QueueShow
4e7be2f1 Hal_OS_runShell
4e7bd3c5 Hal_OS_SemaphoreCreate
4e7bd671 Hal_OS_SemaphoreCreate_MP
4e7bd401 Hal_OS_SemaphoreDestroy
4e7bd721 Hal_OS_SemaphoreDestroy_MP
4e7bd4f1 Hal_OS_SemaphoreGetValue
4e7bd479 Hal_OS_SemaphoreSignal
4e7bd89d Hal_OS_SemaphoreSignal_MP
4e7bd43d Hal_OS_SemaphoreWait
4e7bd7e1 Hal_OS_SemaphoreWait_MP
4e7bc41d Hal_OS_SetClockRTC
4e7bc4d5 Hal_OS_TaskCreate
4e7bceed Hal_OS_TaskDelay
4e7bc9f9 Hal_OS_TaskGetSelf
4e7bca81 Hal_OS_TaskPriority
4e7bcc4d Hal_OS_TaskResume
4e7bcd25 Hal_OS_TaskSleep
4e7bcec1 Hal_OS_TaskSuicide
4e7bcc11 Hal_OS_TaskSuspend
4e7bcf41 Hal_OS_TaskUsDelay
4e7bc8f1 Hal_OS_TaskWaitForExit
4e7bcdcd Hal_OS_TaskWakeup
4e7bc0a5 Hal_OS_Term
4e7bc7f5 Hal_OS_TestTaskDestroy
4e7be3b5 Hal_Peripheral_Debug
4e7bfb7d Hal_PQ_BypassDither
4e7c0065 Hal_PQ_ClearWhite
4e7c00d1 Hal_PQ_ClearWhite2
4e7bfc4d Hal_PQ_ColorEnhanceBypass
4e7bfb0d Hal_PQ_ColorSpaceCustom
4e7bfcb5 Hal_PQ_Comp_Phase
4e7bf6d9 Hal_PQ_ContrastDimming
4e7bf975 Hal_PQ_DataDump
4e7c01a9 Hal_PQ_GammaEnable
4e7c0949 Hal_PQ_GetGammaData
4e7c0a29 Hal_PQ_GetGammaSinglePoint
4e7c073d Hal_PQ_GetHDRStatus
4e7c0699 Hal_PQ_GetHistogram
4e7c0aa1 Hal_PQ_GetPanelCursor
4e7bf8a1 Hal_PQ_PostPostSharpness
4e7bf835 Hal_PQ_PostSharpness
4e7bfffd Hal_PQ_PPQT_Version
4e7c0279 Hal_PQ_ReadAPL_CB
4e7c0631 Hal_PQ_ReadDP_RGB_APL
4e7c0701 Hal_PQ_ReadHistogram
4e7c0145 Hal_PQ_Set10BitDither
4e7c0425 Hal_PQ_SetBIPPMode
4e7c02e1 Hal_PQ_SetBlackAdjust
4e7bfa3d Hal_PQ_SetBLE
4e7bfaa5 Hal_PQ_SetBLEStartPointMin
4e7c03b9 Hal_PQ_SetCCEMode
4e7c0811 Hal_PQ_SetCMOnOff
4e7bfd1d Hal_PQ_SetColorCompensation
4e7c07a9 Hal_PQ_SetDVPanelConfig
4e7bff29 Hal_PQ_SetFilmMode
4e7c034d Hal_PQ_SetFRCDemoModeOnOff
4e7c08e1 Hal_PQ_SetGammaData
4e7c09b1 Hal_PQ_SetGammaSinglePoint
4e7bfec1 Hal_PQ_SetHdmiBlackLevel
4e7bf605 Hal_PQ_SetHdmiPC
4e7c0b59 Hal_PQ_SetHDR10P_BYPASS
4e7bf90d Hal_PQ_SetHLPF
4e7bf66d Hal_PQ_SetOffsetGain
4e7c0879 Hal_PQ_SetPictureMode
4e7c048d Hal_PQ_SetPictureQuality
4e7bfbe5 Hal_PQ_SetRGBOnlyMode
4e7bfd85 Hal_PQ_SetSaturation
4e7bfded Hal_PQ_SetSaturationSub
4e7bff91 Hal_PQ_SetScartUltraBlack
4e7bfe55 Hal_PQ_SetxvYCC
4e7bf7c9 Hal_PQ_VideoDecoder
4e7c0211 Hal_PQ_WB_Enable
4e7bf741 Hal_PQ_WCE
4e7c0f71 Hal_Pwm_Close
4e7c0bc1 _hal_Pwm_GetLock
4e7c0c39 Hal_Pwm_Init
4e7c0d41 Hal_Pwm_Open
4e7c0bfd _hal_Pwm_RelLock
4e7c0fc1 Hal_Pwm_Set_ByChannel
4e7c1109 Hal_Pwm_Set_HWFR_ByChannel
4e7c0cd5 Hal_Pwm_Terminate
4e7b27f9 Hal_ReadDecimalKey
4e7bf34d Hal_SPI_Close
4e7bf54d Hal_SPI_EraseFlash
4e7bf2f9 Hal_SPI_Open
4e7bf42d Hal_SPI_Read
4e7bf39d Hal_SPI_Seek
4e7bf4bd Hal_SPI_Write
4e7c1ff9 Hal_Userdata_DisableVbiLine
4e7c1db9 Hal_Userdata_EnableVbiLine
4e7c217d _HAL_UserData_GetAnalogData
4e7c26b1 Hal_UserData_GetStatus
4e7c1d49 Hal_Userdata_InstallUserDataCallback
4e7c8925 Hal_VideoDec_Close
4e7c906d Hal_VideoDec_get_status
4e7c8759 Hal_VideoDec_Init
4e7c8815 Hal_VideoDec_Open
4e7c8c09 Hal_VideoDec_Pause
4e7c95f1 Hal_VideoDec_poll_event
4e7c8d01 Hal_VideoDec_Resume
4e7c8af1 Hal_VideoDec_Streamoff
4e7c89e1 Hal_VideoDec_Streamon
4e7c939d Hal_VideoDec_subscribe_event
4e7c8dfd Hal_VideoDec_v4l2_get_ctrls
4e7c8ed1 Hal_VideoDec_v4l2_set_ctrls
4e7c8f89 Hal_VideoDec_v4l2_set_fmt
4e7c5879 Hal_Video_Drm_CaptureFrame
4e7c5069 Hal_Video_Drm_CaptureInput
4e7c6ad9 Hal_Video_Drm_CaptureOSD
4e7c61c5 Hal_Video_Drm_CaptureScreen
4e7c3a71 _hal_video_drm_create_fb
4e7c2c55 _hal_video_drm_info_init
4e7c3bed Hal_Video_DrmInit
4e7c364d _hal_video_drm_kms_init
4e7c2d05 _hal_video_drm_open
4e7c4ead Hal_Video_Drm_ReleaseDumpBuffer
4e7c4df5 Hal_Video_Drm_Set_CaptureScreenSize_Fmt
4e7c472d Hal_Video_Drm_Set_DpSource
4e7c4a89 Hal_Video_Drm_Set_Mute
4e7c4c89 Hal_Video_Drm_Set_Property
4e7c4b89 Hal_Video_Drm_Set_StillVideo
4e7c4989 Hal_Video_Drm_Set_Sync
4e7c481d Hal_Video_Drm_Set_VideoSize
4e7c2ff9 Hal_Video_DrmShareMemCreate
4e7c314d Hal_Video_DrmSocketCreate
4e7c2b21 _hal_video_drm_socket_thread_create
4e7c3a35 Hal_Video_FlushPlaneInfo
4e7c45dd Hal_Video_Get_DecoderId
4e7c3201 Hal_Video_Get_DrmDeviceId
4e7c3465 Hal_Video_Get_DrmPlaneInfo
4e7c4491 Hal_Video_Get_PlaySpeed
4e7c41f9 Hal_Video_Get_V4l2Drm
4e7c3fa9 Hal_Video_Get_V4l2FdCommon
4e7c3e4d Hal_Video_Get_V4l2Info
4e7c2f49 Hal_Video_Init
4e7c76d9 Hal_Video_Libdrm_drmAllocCpy
4e7c8231 Hal_Video_Libdrm_drmCommandWriteRead
4e7c7475 Hal_Video_Libdrm_drmFree
4e7c73c9 Hal_Video_Libdrm_drmIoctl
4e7c7435 Hal_Video_Libdrm_drmMalloc
4e7c7769 Hal_Video_Libdrm_drmModeAddFB2
4e7c7511 Hal_Video_Libdrm_drmModeFreeConnector
4e7c756d Hal_Video_Libdrm_drmModeFreeEncoder
4e7c83d5 Hal_Video_Libdrm_drmModeFreeFB
4e7c7639 Hal_Video_Libdrm_drmModeFreeObjectProperties
4e7c75f1 Hal_Video_Libdrm_drmModeFreePlane
4e7c75a9 Hal_Video_Libdrm_drmModeFreePlaneResources
4e7c7689 Hal_Video_Libdrm_drmModeFreeProperty
4e7c74b5 Hal_Video_Libdrm_drmModeFreeResources
4e7c7a1d Hal_Video_Libdrm_drmModeGetConnector
4e7c8289 Hal_Video_Libdrm_drmModeGetCrtc
4e7c7c1d Hal_Video_Libdrm_drmModeGetEncoder
4e7c835d Hal_Video_Libdrm_drmModeGetFB
4e7c7d79 Hal_Video_Libdrm_drmModeGetPlane
4e7c7c91 Hal_Video_Libdrm_drmModeGetPlaneResources
4e7c7fbd Hal_Video_Libdrm_drmModeGetProperty
4e7c7839 Hal_Video_Libdrm_drmModeGetResources
4e7c7e79 Hal_Video_Libdrm_drmModeObjectGetProperties
4e7c8165 Hal_Video_Libdrm_drmModeObjectSetProperty
4e7c8315 Hal_Video_Libdrm_drmModeRmFB
4e7c81bd Hal_Video_Libdrm_drmModeSetPlane
4e7c42a5 Hal_Video_Print_DrmPlaneInfo
4e7c3efd Hal_Video_Reset_V4l2Info
4e7c4539 Hal_Video_Set_DmxHndl
4e7c2cb1 _hal_video_set_drm_device_id
4e7c3559 Hal_Video_Set_DrmPlaneInfo
4e7c43ed Hal_Video_Set_PlaySpeed
4e7c4139 Hal_Video_Set_V4l2Drm
4e7c4071 Hal_Video_Set_V4l2FdCommon
4e7c3d8d Hal_Video_Set_V4l2Info
4e7c3cc5 Hal_Video_V4l2InfoInit
4e7c468d Hal_Video_VideoPath_to_PlaneType
4e7c8415 Hal_Video_Vpltio_Init
4e7c85fd Hal_Video_Vpltio_SendCmd
4e7c98dd Hal_Vin_Close
4e7c9c2d Hal_Vin_GetHdmiStatus
4e7c9a85 Hal_Vin_GetLock
4e7c9afd Hal_Vin_GetRes
4e7c9a09 Hal_Vin_GetVDLock
4e7c97e9 Hal_Vin_Open
4e7c993d Hal_Vin_SelectInput
4e7c99a1 Hal_Vin_SetColorSystem
4e7c9b95 Hal_Vin_SetInputRes
4e7c9cc1 Hal_Vin_SetPcMode
4e7c9d59 Hal_Vin_SetStreamStart
4e7aaf8c _init
4e7b18d5 is_codec_supported
4e7b0d21 is_compress_ready
4e7b0cd5 is_compress_running
4e7ac25d _makeAddr
4e7cab79 my_itoa
4e7ac07d _NullProtocoFun
4e7ca42d SD_PPU_GetMenu
4e7c19f1 sh_drv_tsmux_Close
4e7c19a5 sh_drv_tsmux_Flush
4e7c124d sh_drv_tsmux_GetContext
4e7c1ce5 sh_drv_tsmux_GetMemInfo
4e7c12ad sh_drv_tsmux_Init
4e7c154d sh_drv_tsmux_InstallTsCallback
4e7c142d sh_drv_tsmux_Open
4e7c16b1 sh_drv_tsmux_Packetize
4e7c18b5 sh_drv_tsmux_ReleaseTsBuffer
4e7c1761 sh_drv_tsmux_SetPmtMode
4e7c17ad sh_drv_tsmux_SetUserSection
4e7c15a5 sh_drv_tsmux_Start
4e7c1811 sh_drv_tsmux_StartUserSection
4e7c1901 sh_drv_tsmux_Stop
4e7c1861 sh_drv_tsmux_StopUserSection
4e7c1b71 sh_drv_tsmux_WaitStatus
4e7bac71 sh_drv_venc_Change_Param
4e7ba69d sh_drv_venc_Close
4e7badb5 sh_drv_venc_EnqueueOnePic
4e7bae15 sh_drv_venc_EnqueueSrcMutePic
4e7ba90d sh_drv_venc_Get_Addr
4e7bad59 sh_drv_venc_getInputBufInfo
4e7ba4d9 sh_drv_venc_Init
4e7bad1d sh_drv_venc_Mute
4e7ba535 sh_drv_venc_open
4e7ba9a9 sh_drv_venc_ReleaseEncBuffer
4e7babf5 sh_drv_venc_Resume
4e7bac31 sh_drv_venc_Set_Bitrate
4e7ba79d sh_drv_venc_Start
4e7baa69 sh_drv_venc_Stop
4e7bab51 sh_drv_venc_Suspend
4e7ba949 sh_drv_venc_Write_BS
4e7ac0b5 _TheardServerLoop
4e7cb8d5 _TOOL_FUNC_COEFTBL_FLOAT2FXP
4e7cac75 _TOOL_PP_find_string
4e7cac31 _TOOL_PP_NumToStr
4e7cffd5 _TOOL_PP_ParseCmd
4e7cad8d _TOOL_PP_ParseCmdItem
4e7cfed9 _TOOL_PP_ParseCmdModule
4e7bb0bd venc_v4l2_close
4e7bb9fd venc_v4l2_dequeue_buffer
4e7bb819 venc_v4l2_de_queue_event
4e7bb895 venc_v4l2_get_remain_offset
4e7bb031 venc_v4l2_open
4e7bb8d1 venc_v4l2_queue_buffer
4e7bb981 venc_v4l2_queue_phys_buffer
4e7bb521 venc_v4l2_set_bitrate
4e7bbabd venc_v4l2_set_event
4e7bb595 venc_v4l2_set_framerate
4e7bb201 venc_v4l2_set_mmap_normal
4e7bb109 venc_v4l2_set_mmap_secure
4e7bb611 venc_v4l2_set_mute
4e7bb689 venc_v4l2_set_src_mode
4e7bb6f9 venc_v4l2_set_vid_fmt
4e7bb3e1 venc_v4l2_start
4e7bb4b9 venc_v4l2_stop
4e7bb449 venc_v4l2_streamoff
4e7bb371 venc_v4l2_streamon
4e7bb799 venc_v4l2_subscribe_event
4e7acd85 _Z15AENC_GetContexth
4e7c1af5 _Z17sh_drv_tsmux_Termv
4e7ba2fd _Z18sh_drv_venc_ThreadP17_VENC_USERCONTEXT
4e7ba29d _Z19DRV_VECN_Callback_p21_DRV_VENC_EVENT_PARAM
4e7ace65 _Z20HalAudioEnc_Write_BSPci
4e7bd30d _Z20Hal_OS_MutexCondInitPvi
4e7bd03d _Z20Hal_OS_MutexCondWaitPvj
4e7bd4b5 _Z20Hal_OS_SemaphoreInitPvi
4e7bcf7d _Z21Hal_OS_MutexCondCreatjPKc
4e7bd2a1 _Z22Hal_OS_MutexCondSignalPv
4e7bcfe9 _Z23Hal_OS_MutexCondDestroyPv
4e7ba491 _Z23sh_drv_venc_Get_Contexti
4e7bd375 _Z24Hal_OS_MutexCondGetValuePv
4e7bafa9 _Z25sh_drv_venc_Tasecase_Stopv
4e7bc831 _Z26SD_SHELL_OS_IntTaskDestroyPvPFimE
4e7bae91 _Z26sh_drv_venc_Tasecase_Startv
4e7bc2a1 _Z32SD_SHELL_OS_ConvertDateTimeToSecP23_ST_SD_SHELL_OS_SYSTIMEPj
4e7bc3c1 _Z38SD_SHELL_OS_ConvertSecToDateTimeStringPaj
```

Other dynamic symbols:

```
4e7f3d04 D _astAVLConfig_usr
4e7f40d8 D _astDAPConfig_usr
4e7f4b1c D _astDAPV22Config_usr
4e7f5734 D _astDBEConfig_usr
4e7f5758 D _astDRCConfig_usr
4e7f57c4 D _astDTSNEOULTRAConfig_usr
4e7f592c D _astEchoCancellationConfig_usr
4e7f58c0 D _astEQConfig_usr
4e7f5a94 D _astIIR0Config_usr
4e7f5b24 D _astIIR1Config_usr
4e7f5bb4 D _astKeyShfConfig_usr
4e7f5bfc D _astNGTConfig_usr
4e7f5cd4 D _astPPConfig_usr
4e7f5cf8 D _astReverberationConfig_usr
4e7f5d64 D _astSAConfig_usr
4e7f5e3c D _astTSHDConfig_usr
4e7f60e8 D _astV3DSURROUNDConfig_usr
4e7f63dc D _astVIRTUALXConfig_usr
4e7f6eb0 D _astVSurConfig_usr
4e80e734 B __bss_end__
4e80e734 B _bss_end__
4e7f7010 B __bss_start
4e7f7010 B __bss_start__
4e80267c B demux_src0
4e802680 B demux_src1
4e7f7010 D _edata
4e80e734 B __end__
4e80e734 B _end
4e8027a0 B exit_status
4e7f714c B fake_time
4e80caa4 B gac8Cmd
4e80bec0 B geDisplay_dispageFmt
4e80bec4 B geDisplay_dispageFmt_New
4e7f3d00 D gfSqrt2
4e80bee8 B g_iExitDriverMenu
4e80b590 B g_pstHalVideoDrmInfo
4e80caf4 B gstPPCmd
4e80a848 B g_tsmux_module_name
4e80bec8 B gu32Display_dispageHSize
4e80becc B gu32Display_dispageHSize_New
4e80bed0 B gu32Display_dispageVSize
4e80bed4 B gu32Display_dispageVSize_New
4e80caa0 B g_u32PPUPostModuleSize
4e7f3c6c D gValFile
```

## Appendix: dynamic functions in libNDAL_EX.so

```
4db6ae38 _fini
4db5e035 _GPU_Blend_CMD_Setting
4db5feb5 _GPU_Blend_Setting
4db5fc9d _GPU_Check_Planes
4db5d585 GPU_CMD_Buf_alloc
4db5dee9 GPU_CMD_Cancel
4db5dd21 GPU_CMD_End
4db5ee5d GPU_CMD_Q_BlendScale
4db5e3b9 GPU_CMD_Q_Close
4db5e74d GPU_CMD_Q_Copy
4db5f7a1 GPU_CMD_Q_Emit
4db5e311 _GPU_CMD_Q_EndCheck
4db5e429 GPU_CMD_Q_Fill
4db5f475 GPU_CMD_Q_FlipRotate
4db5e34d GPU_CMD_Q_Open
4db5e2d5 _GPU_CMD_Q_PostCheck
4db5e299 _GPU_CMD_Q_PreCheck
4db5e971 GPU_CMD_Q_Scale
4db5f7dd GPU_CMD_Q_Sync
4db5d78d GPU_CMD_Seq_add
4db5da9d GPU_CMD_Seq_Check
4db5d6fd GPU_CMD_Seq_Close
4db5d889 GPU_CMD_Seq_del
4db5d901 GPU_CMD_Seq_GetAddr
4db5d99d GPU_CMD_Seq_Numbers
4db5d69d GPU_CMD_Seq_Open
4db5da05 GPU_CMD_Seq_Print
4db5d94d GPU_CMD_Seq_Reset
4db5dc15 GPU_CMD_Seq_Space
4db5dbd9 GPU_CMD_Seq_Writeback
4db5dfb1 _GPU_CMD_Set_Clip
4db5dd65 GPU_CMD_Set_Register
4db5dddd GPU_CMD_Set_Registers
4db5df2d _GPU_CMD_Set_Size
4db5dcd1 GPU_CMD_Trigger
4db60539 _GPU_ColorTransform_Coefficient
4db60489 _GPU_ColorTransform_Enable
4db6082d _GPU_ColorTransform_RGB2YUV
4db60c01 _GPU_ColorTransform_Setting
4db60a01 _GPU_ColorTransform_YUV2RGB
4db64641 GPU_Dump
4db60ca5 _GPU_Format_Domain
4db60055 _GPU_Gradient_Fix_Color
4db5e20d _GPU_GradientFixColor_CMD_Setting
4db6014d _GPU_Gradient_Setting
4db646ed GPU_Human_Readable_Size
4db647a9 GPU_Human_Readable_Time
4db626dd GPU_HW_BlendScale
4db63761 GPU_HW_CMD_Go
4db61bd1 GPU_HW_Copy
4db60e5d GPU_HW_CopyFlip_Setting
4db5f8e5 GPU_HW_Disable_All_DbgMask
4db6108d GPU_HW_DumpReg
4db61121 GPU_HW_End
4db613bd GPU_HW_Fill
4db631c5 GPU_HW_FlipRotate
4db61355 GPU_HW_FmtString
4db60d15 GPU_HW_Initial
4db612d9 GPU_HW_OpString
4db5f99d GPU_HW_PrintDbgGroup
4db5faad GPU_HW_Printf
4db61ff1 GPU_HW_Scale_LowPassFilter
4db60de5 GPU_HW_Start
4db5f929 GPU_HW_ToggleDbgMask
4db5fb41 _GPU_Plane_Valid_Check
4db64569 GPU_SYS_Find_SD_Path
4db63d2d GPU_SYS_GetVersion
4db639a9 GPU_SYS_Linux_Close
4db63a2d GPU_SYS_Linux_Fcntl_GetFlag
4db63a81 GPU_SYS_Linux_Fcntl_SetFlag
4db63875 GPU_SYS_Linux_Open
4db63e31 GPU_SYS_Lock
4db6438d GPU_SYS_MEM_Cache_Flush_All
4db640fd GPU_SYS_MEM_Cache_Invalidate
4db64205 GPU_SYS_MEM_Cache_Invalidate_All
4db6430d GPU_SYS_MEM_Cache_Sync_Ranges
4db64181 GPU_SYS_MEM_Cache_Writeback
4db64289 GPU_SYS_MEM_Cache_Writeback_All
4db63f75 GPU_SYS_MEM_CMA_Alloc
4db64039 GPU_SYS_MEM_Free
4db63fd5 GPU_SYS_MEM_PAGES_Alloc
4db6409d GPU_SYS_MEM_Show
4db64409 GPU_SYS_Mmap
4db644f1 GPU_SYS_Msync_Invalidate
4db644a1 GPU_SYS_Munmap
4db63b49 GPU_SYS_Named_Semaphore_Close
4db63ad5 GPU_SYS_Named_Semaphore_Open
4db63cb9 GPU_SYS_Named_Semaphore_post
4db63bbd GPU_SYS_Named_Semaphore_TimedWait
4db63dcd GPU_SYS_Trigger
4db63f01 GPU_SYS_Trigger_Unlock
4db63e99 GPU_SYS_Unlock
4db64d55 HAL_ClearSplashLogo
4db6511d HAL_CreateBuffer
4db64e49 HAL_ReleaseBuffer
4db64fa9 HAL_RenderBuffer
4db64c3d HAL_ShowSplashLogo
4db64b0d HAL_TestCreateBuffer
4db648d9 HAL_TestReleaseBuffer
4db649c5 HAL_TestRenderBuffer
4db56288 _init
4db5707d NDAL_CommonExt_GetCorePwrOP
4db56dc1 NDAL_CommonExt_GetUniqueID
4db57019 NDAL_CommonExt_IsTighten
4db56e59 NDAL_CommonExt_Version
4db5c82d NDALex_GPU_BatchCopy
4db5c565 NDALex_GPU_BatchFill
4db5cd3d NDALex_GPU_BatchRop
4db5cabd NDALex_GPU_BatchScale
4db5c51d NDALex_GPU_Cache_Flush_All
4db5c3ed NDALex_GPU_Cache_Invalidate
4db5c485 NDALex_GPU_Cache_Invalidate_All
4db5c439 NDALex_GPU_Cache_Writeback
4db5c4d1 NDALex_GPU_Cache_Writeback_All
4db5b2a5 NDALex_GPU_Close
4db5d0d1 NDALex_GPU_Cmd_Copy
4db5d4ed NDALex_GPU_Cmd_Emit
4db5cfd9 NDALex_GPU_Cmd_Fill
4db5d3f9 NDALex_GPU_Cmd_FlipRotate
4db5d291 NDALex_GPU_Cmd_Rop
4db5d191 NDALex_GPU_Cmd_Scale
4db5d539 NDALex_GPU_Cmd_Sync
4db5bb89 NDALex_GPU_Copy
4db5bf91 NDALex_GPU_Dlfun
4db5ba99 NDALex_GPU_Fill
4db5be91 NDALex_GPU_FlipRotate
4db5c2d5 NDALex_GPU_MEM_Alloc_CMA
4db5c28d NDALex_GPU_MEM_Alloc_Pages
4db5c31d NDALex_GPU_MEM_Free
4db5c365 NDALex_GPU_Mmap
4db5c3b1 NDALex_GPU_Munmap
4db5c08d NDALex_GPU_Open
4db5bd35 NDALex_GPU_Rop
4db5bc3d NDALex_GPU_Scale
4db5c045 NDALex_GPU_Version
4db57d29 NDAL_VenderVideoExt_GetDPCountTsk
4db58339 NDAL_VenderVideoExt_init
4db581b5 NDAL_VenderVideoExt_init_Status
4db57c7d NDAL_VenderVideoExt_OS_CalcClockDiff
4db57cb9 NDAL_VenderVideoExt_OS_GetClockDiff
4db57c35 NDAL_VenderVideoExt_OS_TaskDelay
4db580d9 NDAL_VenderVideoExt_Send_DolbyVision_GetCmd
4db57f05 NDAL_VenderVideoExt_Send_DolbyVision_SetCmd
4db581f9 NDAL_VenderVideoExt_SendGetCmd
4db58409 NDAL_VenderVideoExt_SendSetCmd
4db582b9 NDAL_VenderVideoExt_TaskCreate
4db5aef9 NDAL_VideoExt_BLAST_RX
4db5ae71 NDAL_VideoExt_BLAST_SetRcProtocol
4db59a15 NDAL_VideoExt_CEC_CheckCmdCount
4db598ad NDAL_VideoExt_CEC_GeneralPollingMessage
4db59ba1 NDAL_VideoExt_CEC_Get_LogicalAddr
4db59779 NDAL_VideoExt_CEC_Init
4db59809 NDAL_VideoExt_CEC_PollingMessage
4db59971 NDAL_VideoExt_CEC_Process_Command
4db59a85 NDAL_VideoExt_CEC_Set_EDID_ATMOS_Bit
4db59b01 NDAL_VideoExt_CEC_Set_LogicalAddr
4db58d09 NDAL_VideoExt_FreeATVFB_and_StopAlloc
4db596e9 NDAL_VideoExt_FreeFRCFB
4db58af9 NDAL_VideoExt_GetHDMIExtraInfo
4db58f75 NDAL_VideoExt_GetIdentStatus
4db588d5 NDAL_VideoExt_GetModeInfo
4db58d79 NDAL_VideoExt_Get_SRAM_EDID
4db5871d NDAL_VideoExt_GetSyncInfo
4db5895d NDAL_VideoExt_MHL_KeyPressRelease
4db59d11 NDAL_VideoExt_OpenDevice
4db59569 NDAL_VideoExt_Set3D
4db592f1 NDAL_VideoExt_SetDPFrameRate
4db5918d NDAL_VideoExt_SetFrameBGColor
4db58a6d NDAL_VideoExt_Set_HDMI_ClrRatioScramble
4db58bf9 NDAL_VideoExt_SetHDMIExtraCtrl
4db589e5 NDAL_VideoExt_SetHDMIRXCLK
4db58f15 NDAL_VideoExt_SetIdentMapping
4db58e9d NDAL_VideoExt_SetIdentSequence
4db594e1 NDAL_VideoExt_SetNetflexFlag
4db5939d NDAL_VideoExt_SetNonlinearScalingParameter
4db58fd5 NDAL_VideoExt_SetPanel
4db587b9 NDAL_VideoExt_SetPlayFieldType
4db58e0d NDAL_VideoExt_Set_SRAM_EDID
4db59639 NDAL_VideoExt_SetUp2DTo3D
4db58841 NDAL_VideoExt_Set_VDStatusBeforeDP
4db59265 NDAL_VideoExt_SetVX1Partition
4db5a1e5 NDAL_VideoExt_STBC_FreshWDT
4db5a525 NDAL_VideoExt_STBC_GetAdcChannelValue
4db5ade9 NDAL_VideoExt_STBC_GetMcuWakeUpTime
4db5a745 NDAL_VideoExt_STBC_GetMcuWatchDogTimeout
4db5b045 NDAL_VideoExt_STBC_GetPcbVersion
4db5a6b5 NDAL_VideoExt_STBC_GetPowerOnEvent
4db5a459 NDAL_VideoExt_STBC_GetStbcGpio
4db5afb5 NDAL_VideoExt_STBC_GetSTBCSvn
4db5a7d1 NDAL_VideoExt_STBC_GetSTBCVersion
4db5acb1 NDAL_VideoExt_STBC_OpTcnt
4db5a2c5 NDAL_VideoExt_STBC_Poweroff
4db5a5d5 NDAL_VideoExt_STBC_PowerOffByKeypad
4db5a645 NDAL_VideoExt_STBC_PowerSavingNow
4db5aa31 NDAL_VideoExt_STBC_ResetByHWWDT
4db5a9ad NDAL_VideoExt_STBC_SetMcuDCAutoOn
4db5ac2d NDAL_VideoExt_STBC_SetMcuFakeSTROnOff
4db5a8e1 NDAL_VideoExt_STBC_SetMcuLEDState
4db5ad5d NDAL_VideoExt_STBC_SetMcuWakeUpTime
4db5aba9 NDAL_VideoExt_STBC_SetMcuWifiPwr
4db5aaa1 NDAL_VideoExt_STBC_SetMcuWoLOnOff
4db5ab25 NDAL_VideoExt_STBC_SetMcuWoWOnOff
4db5a3b9 NDAL_VideoExt_STBC_SetStbcGpio
4db5a335 NDAL_VideoExt_STBC_SetWDT
4db5a255 NDAL_VideoExt_STBC_StopWDT
4db5a871 NDAL_VideoExt_STBC_SyncMcuDB
4db59c99 NDAL_VideoExt_VBI_IsDataReady
4db59c29 NDAL_VideoExt_VBI_IsTeletexReady
4db59f0d NDAL_VideoExt_VD_DeliverConfig
4db5a14d NDAL_VideoExt_VD_GetNoiseLevel
4db59dd5 NDAL_VideoExt_VD_GetStatus
4db571d9 ND_CommonExt_CheckSecuriry
4db570d1 ND_CommonExt_EnableSecuriry
4db57225 ND_CommonExt_ForceDisableSecurity
4db66c2d ndcommon_v4l2_config_ctrl
4db66be5 ndcommon_v4l2_config_ctrllist
4db66c71 ndcommon_v4l2_ioctl
4db66cdd ndcommon_v4l2_set_fd
4db66a51 NDFW_GetImageLength
4db669d9 NDFW_GetImageName
4db65e85 NDFW_GetImgSignatureStatus
4db66acd NDFW_GetUpdateStatus
4db664b9 NDFW_ParserHeader
4db66b11 NDFW_ResetUpdateStatus
4db661e1 NDFW_SaveAsFile
4db66589 NDFW_SaveTempImage
4db66b75 NDFW_SetUpdateOpt
4db6647d NDFW_ShowVersion
4db6688d NDFW_UpdateImage
4db66749 NDFW_UpdateTempImage
4db660c9 NDFW_VerifyImg
4db65ff1 NDFW_VerifyWholeImg
4db67ab5 Nd_NIM_BlinScan_Update
4db67ef1 Nd_NIM_DeliverConfig
4db676f9 Nd_NIM_DVIF_SetTunerType
4db67b15 Nd_NIM_GetBlindScanNextFreq
4db68069 Nd_NIM_GetDeliverySystem
4db67885 Nd_NIM_GetFrequency
4db67b8d Nd_NIM_GetPLPStatus
4db677b9 Nd_NIM_GetSymbolRate
4db67c8d Nd_NIM_Send_diseqc_sequence
4db67d65 Nd_NIM_Set_diseqc_control
4db67a55 Nd_NIM_SetScanMode
4db67dc9 Nd_NIM_SetSpectralINV
4db67e89 Nd_NIM_SetStandy
4db67e29 Nd_NIM_SetTsOutput
4db67951 Nd_NIM_SetUnicableIF
4db67671 Nd_NIM_VD_ConnectATV
4db6725d Nd_NIM_VD_GetNoiseForAP
4db672f5 Nd_NIM_VD_GetStatus
4db6742d Nd_NIM_VD_SetAMCForceMode
4db674bd Nd_NIM_VD_SetAutoScan
4db67545 Nd_NIM_VD_SetChannelUpDnInit
4db675d5 Nd_NIM_VD_SetSupportTortureRoom
4db69eb1 NDOSD_EnableProtectionArea
4db69d31 NDOSD_GetAlphaCount
4db6a9b9 NDOSD_GetAutomuteStatus
4db6a221 NDOSD_GetColorMode
4db6a8c5 NDOSD_GetCurrentBufferOrder
4db6a5fd NDOSD_GetDispWin
4db6a049 NDOSD_GetPhyBufferAddress
4db6ad5d NDOSD_GetPlaneOrder
4db6a3f9 NDOSD_GetStride
4db6aaad NDOSD_GetStrLogoPhyAddrFromCMABuffer
4db6aba1 NDOSD_ReleaseCMABuffer
4db69e25 NDOSD_SetAutomuteTrigger
4db6a13d NDOSD_SetColorMode
4db6a4ed NDOSD_SetDispWin
4db6a7e1 NDOSD_SetForceBufferOrder
4db69f55 NDOSD_SetPhyBufferAddress
4db6ac91 NDOSD_SetPlaneOrder
4db6a315 NDOSD_SetStride
4db6a715 NDOSD_SetUpdateEnable
4db6851d NDSCALER_BRIG_SetNetflexFlag
4db68315 NDSCALER_VIN_GetMuteState
4db68289 NDSCALER_VIN_SetForceScartMode
4db68395 NDSCALER_VIN_SetHDMI_HDCPKey
4db68495 NDSCALER_VIN_SetHDMIRXCLK
4db6842d NDSCALER_VIN_SetVGA_SyncType
4db68861 ND_SYS_CheckAddress
4db68c91 ND_SYS_FlushPhys
4db68d0d ND_SYS_FlushVirt
4db69519 ND_SYS_Init
4db688d5 ND_SYS_Phys2Virt
4db68b31 ND_SYS_Read16
4db68a3d ND_SYS_Read32
4db68c1d ND_SYS_Read8
4db69345 NDSYS_SYS_CloseReservedMemory
4db68d75 NDSYS_SYS_GetReservedMemory
4db692c1 NDSYS_SYS_VirAddr2Phys
4db687b1 ND_SYS_Term
4db68949 ND_SYS_Virt2Phys
4db68ab9 ND_SYS_Write16
4db689c5 ND_SYS_Write32
4db68bad ND_SYS_Write8
4db696f1 ND_TSPU_CRC32_Check
4db69c11 ND_TSPU_Desc_Get_ScrambleBit
4db69b61 ND_TSPU_Desc_Get_ScrambleStatus
4db69aa5 ND_TSPU_Desc_PID_Disable
4db69941 ND_TSPU_Desc_PID_Enable
4db69891 ND_TSPU_FILT_GetPTS
4db697e5 ND_TSPU_Get_Audio_Filter_Handle
4db69cb9 ND_TSPU_MuxStart
4db69cf5 ND_TSPU_MuxStop
4db65c79 nvtfw_find_img
4db65975 system_command_touch_file
4db56ed9 _Z10_ksys_openv
4db56f41 _Z11_ksys_closei
4db66dd5 _Z12NdCP_GetPRNGPh
4db66d21 _Z12NdExtCP_Initv
4db66d99 _Z15NdExtCP_Decrypt11NdExtCPID_kP21NdCPExt_ParamEncDec_t15NdExtMainChip_k
4db66d5d _Z15NdExtCP_Encrypt11NdExtCPID_kP21NdCPExt_ParamEncDec_t15NdExtMainChip_k
4db69381 _Z18_SYS_GetDevNodeFIDPKcii
4db56fa5 _Z20_ksys_get_hw_versioni
4db68199 _Z21NDOSD_EnableOSDEngineb
4db680f5 _Z21NDOSD_SetTransparencyhj
4db56e19 _Z22NDAL_CommonExt_GetCPOPPj
4db56d3d _Z23NDAL_CommonExt_GetCAUIDPj
4db577d5 _Z23NDAL_GfxExt_SetPreAlpha22_Nd_Gfxex_OSD_PLANE_IDb
4db5751d _Z24NDAL_GfxExt_SET_CROP_WIN22_Nd_Gfxex_OSD_PLANE_IDP8_ST_RECTS1_
4db57665 _Z25NDAL_GfxExt_GetAlphaCounthPj
4db57a89 _Z25NDAL_SoundExt_SetPCMParam24ND_SoundExt_CHANNEL_ID_kP24ND_SoundExt_PCM_Params_t
4db57b19 _Z26NDAL_SoundExt_DmixSoundOut24ND_SoundExt_CHANNEL_ID_kPvj
4db573bd _Z27NDAL_GfxExt_EnableOSDEngineb
4db57319 _Z27NDAL_GfxExt_SetTransparency22_Nd_Gfxex_OSD_PLANE_IDj
4db57449 _Z29NDAL_GfxExt_EnableProtectAreab22_Nd_Gfxex_OSD_PLANE_IDjj
4db57749 _Z29NDAL_GfxExt_SetOSD1_AUTHORITY22_Nd_Gfxex_OSD_PLANE_ID
4db579f9 _Z29NDAL_SoundExt_OpenDmixChannel24ND_SoundExt_CHANNEL_ID_k
4db57bd9 _Z30NDAL_SoundExt_CloseDmixChannel24ND_SoundExt_CHANNEL_ID_k
4db57879 _Z31NDAL_SoundExt_SetDmixChannelVol24ND_SoundExt_CHANNEL_ID_kj
```

Other dynamic symbols:

```
4db84558 B __bss_end__
4db84558 B _bss_end__
4db81c34 B __bss_start
4db81c34 B __bss_start__
4db6eb50 R crc32table_fw
4db81c34 D _edata
4db84558 B __end__
4db84558 B _end
4db82b5b B gb8NDAL_VenderVideo_InitDone
4db818e0 D g_bDPCallBackEnable
4db81c40 B g_cbLibAmbl
4db81c44 B g_cbLibDPCount_CE_New
4db81c48 B g_cbLibDPCount_Dimming_New
4db6c068 R g_Dal_HwGpuBlendParam
4db82bd4 B gfb_id
4db81bd0 D gModeName
4db82bd8 B gparam
4db818e4 D gs32Ndp_PQ_fd
4db81c4c B g_u32_cbDPCount_CE_NewFlag
4db81c50 B g_u32_cbDPCount_Dimming_NewFlag
4db81c54 B g_u8APL_Left
4db823d8 B g_u8APL_Right
4db82b5c B mutex_NDAL_VenderVideoExt_read
4db82b74 B mutex_NDAL_VenderVideoExt_write
4db8409c B stTrng
```
