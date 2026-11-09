# Binaries, modules and hashes

Hashes identify the local extracts used by the reports. They are **not** a
security signature, and none of these files may be redistributed. The NAND
dump is the source of truth; the extracted trees are caches produced from it.

## Main application and libraries

| artifact | extracted path | MD5 | Ghidra project | role / key evidence |
|---|---|---|---|---|
| `aurora.elf` | `rootfs/vendor/com.nvt.idtv/aurora.elf` | `64c252340d3af6c5d66072611b7bad7a` | `ghidra_proj` (Aurora) | stock UI/app; `ICE_HardwareInit`, `PLAT_INI_Init`, `ICE_AudioStart/Volume`, `PLAT_VAUD_SwitchHP`, `PLAT_AIN_*`, `PLAT_AOUT_*`, `PLAT_DRV_INI_Init`; VENDOR-INVENTORY, AUDIO-*, OSD-*, COLD-START-STAGE-AUDIT |
| `libNDAL3.so` | `rootfs/vendor/lib/libNDAL3.so` | `c842bad804302ec822076c3d9d3ce31d` | `ghidra_ndal3` (ndal3) | vendor HAL: `Hal_Audio_*`, `Hal_Video_*`, `Hal_Gpio_*`, `Hal_Iic_*`, `Hal_Pwm_*`, VPLTIO; 132 distinct ioctl immediates |
| `libNDAL_EX.so` | `rootfs/vendor/lib/libNDAL_EX.so` | `e6a465861d1869b8b138b3f299c89f87` | `ghidra_ndal_ex` (ndal_ex) | extended NDAL: `Hal_Audio_DualCmd`, PQ/EX symbols; 36 distinct ioctl immediates |
| `libappmgr.so` | `rootfs/vendor/com.nvt.appmgr/libappmgr.so` | `413930d9366b4f8e76e5ebc2db246a72` | vendor-inventory | app manager; unstripped (2,476 symbols); keyboard layout `0x52f50`; APP-MANAGER-KEYBOARD/CONFIG |
| `libdirectfb_tdga.so` | `rootfs/vendor/lib/dfb/directfb-1.7-1/gfxdrivers/libdirectfb_tdga.so` | `a770a7e5e7d33c2857e773a91c6c2654` | `ghidra_tdga` (Tdga) | DirectFB TDGA gfxdriver; `NVTGA_OSD_SetRegion` `0x152b8`; region ioctls `0x8020c86c`/`0x4020c86b` |
| `libmali.so` | vendor GPU library | (partial analysis) | `ghidra_mali` (Mali) | EGL/GPU limits; EGL-SCENE |

## Kernel modules

| module | extracted path | MD5 | Ghidra project | role / evidence |
|---|---|---|---|---|
| `ntkdriver_nondovi.ko` | `rootfs/vendor2/modules/ntkdriver_nondovi.ko` | `fded7005651e37b8a1b5e8e89f73f4ff` | `ghidra_nondovi` (nondovi) | main vendor driver: OSD/DRM/TCON/VPL, PQ/MPFW, watchdog; `gstOSDfbOps` `0x75494`, `_ker_osd_fb_CheckVar` `0x754a8`, `_ker_osd_fb_SetPar` `0x754ac`, `PLAT_PQ_Mpfw_CtrlTask` `0x253d84` |
| `ntkndpdrm.ko` | `rootfs/root/lib/modules/ntkndpdrm.ko` | `a70aca9b814e51cb9680476ce27051de` | `ghidra_ntk` (ntk) | DRM/GEM |
| `ntkndpalsa.ko` | `rootfs/root/lib/modules/ntkndpalsa.ko` | `8758a13d12799217ce702849df7d54da` | `ghidra_alsa` (Alsa) | ALSA controls; AUDIO-GAIN-TRANSPORT |
| `ntkndphdmiswitch.ko` | `rootfs/root/lib/modules/ntkndphdmiswitch.ko` | `d4ff737dcbd9be2b628f2dd4eccf5d7a` | `ghidra_hdmiswitch` (HdmiSwitch) | HDMI switch |
| `PreInit.ko` | `rootfs/root/lib/modules/PreInit.ko` | `1e50bee8b7888315c7a070f8e69947f6` | `ghidra_preinit` (PreInit) | early CEC cdev registration; CEC-OWNERSHIP |
| `ntkndpextin.ko` | `rootfs/root/lib/modules/ntkndpextin.ko` | `cb0689e9e1c425a27e670168e51cc32f` | `ghidra_extin` (Extin) | external input; `comp_open`/`comp_release`, video24; AMP-VIDEO-OWNER |
| `ntkethmac.ko` | `rootfs/vendor/modules/ntkethmac.ko` | `2efe1ee138ee29c2d490f6e4a667a38d` | `ghidra_eth` (Eth) | Ethernet/PHY and PM policy; POWER-STANDBY-CONTRACT |
| `ntkndp_pq.ko` | `rootfs/root/lib/modules/ntkndp_pq.ko` | `c208cdd0967cddf8cba7c826adf3f73a` | (inventory) | PQ template driver; PQ-MPFW-TASK |
| `ntkndpci.ko` | `rootfs/root/lib/modules/ntkndpci.ko` | `6a06a1bdf95dffa93371b9575a6d294f` | (inventory) | CI |
| `ntkndpcodec.ko` | `rootfs/root/lib/modules/ntkndpcodec.ko` | `76b9671c0064a4c674a99997796d405c` | (inventory) | codec |
| `ntkndpdemod.ko` | `rootfs/root/lib/modules/ntkndpdemod.ko` | `607e40dc6f5252efeb5c89d30fb8e066` | (inventory) | demodulator |
| `ntkndptsd.ko` | `rootfs/root/lib/modules/ntkndptsd.ko` | `2470bb737131e4f803163ba9b1dce569` | (inventory) | TSD |
| `ntkndptuner.ko` | `rootfs/root/lib/modules/ntkndptuner.ko` | `84be071efa5d16c2b6e8d13db667937a` | (inventory) | tuner |
| `kteeclient.ko` | `rootfs/root/lib/modules/kteeclient.ko` | `235215638f06201efe9bbebbb6389c9b` | (inventory) | TEE client |

VENDOR-INVENTORY-2026-11-07 hashes/dumps **389 ELF files in `/vendor` plus 72
outside it (461 total), 41 kernel modules, 15 platform modules**. Nine of the
16 early platform modules lacked a Ghidra project but have objdump/relocation
inventories (codec/tuner/demod/TSD/PQ/PreTcon/PCI/kteec/HDMI).

## Dumps, captures and data

| artifact | path | identity | notes |
|---|---|---|---|
| NAND image | `artifacts/dumps/tv_nand.bin` | MD5 `36de1801478ce38c8042845374add41d`, 536870912 B | pristine source capture; identity from its stored manifest |
| raw kernel slots | `artifacts/workbench/ker*_img.bin` | ker0 `9b91aa0ad7339e7cdfdb3b67c5b50f81`; ker1 `95615c3e9e80230e0be658d82d2a2b23` | reconstructed kernels; OSD-BUFFER-CONTRACT |
| hardware profile | `artifacts/protocol/mb230_hwprofile.bin` | MD5 `1b063b2cc7faa8b92c0f843921e561c7` | amp/tuner/pin profile |
| AOUT cache snapshot | `artifacts/analysis/cec-ownership-2026-11-01/live-audio-routing.bin` | 612 B | read-only live snapshot; decoded by `openaurora/tools/audio/audio-state.c` |
| volume table | `artifacts/analysis/cec-ownership-2026-11-01/live-volume-table.bin` | 202 B | all 101 entries match the AQ decoder |
| keymap | `artifacts/protocol/ir-keymap-2026-10-30.csv` | — | ICE/Dial/NEC/DFB code spaces differ |
| theme/layout | `artifacts/ui-assets/` | — | extracted PNGs/layout manifests/string inventory |

## Recovery identities

| artifact | path | MD5 |
|---|---|---|
| tested `/conf` image | `artifacts/recovery-stability/apdat0_full.ubifs` | `994da7ff76a0b2fbafb8f6fa0c682627` |
| tested USB script | `artifacts/recovery-stability/kylo_usb_update.scr` | `de1591298d633cf6422a1ade4e8835a9` |
| pristine `/conf` | `artifacts/payload/apdat0_orig.ubifs` | `fde74f196b11740de528735e49bcc976` |
| hook script | `artifacts/payload/aurora.elf` | `0b1cd32a9de2b0251f6252e6603db8b7` |

The recovery pair is the tested fallback. Do not substitute the older images
or regenerate the stick silently; see `artifacts/README.md`.

## Verifying an extract

```sh
md5sum artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf
# 64c252340d3af6c5d66072611b7bad7a

# Extract another file from the dump:
artifacts/scripts/extract_from_dump.sh vendor:/path/in/ap0 -o /tmp/out
artifacts/scripts/extract_from_dump.sh vendor2:/path/in/ap1 -o /tmp/out
artifacts/scripts/extract_from_dump.sh root:/path/in/rootfs -o /tmp/out
```

`/conf` (apdat0) is not handled by the extractor: carve 0xA00000 bytes at NAND
offset 0xE340000, then extract as root with ownership preserved:
`sudo .venv/bin/ubireader_extract_files -k -o conf-ubi apdat0.img` (full recipe
in [../reproduce/root-payload.md](../reproduce/root-payload.md)).

The extracted trees are caches; if a hash does not match the report, stop and
re-extract rather than analyse an unknown build.
