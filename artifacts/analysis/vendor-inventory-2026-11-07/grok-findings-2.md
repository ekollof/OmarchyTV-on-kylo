# Nine root kernel modules without a Ghidra project

Read-only, offline, 2026-11-07. No TV access. No Ghidra project was opened or modified. Disassembly is ARM objdump from `artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-objdump` (`-d --no-show-raw-insn`). Relocations are `arm-none-linux-gnueabihf-readelf -W -r`. These files are ET_REL. Every address below is a section offset, not a load address. `bl 0 <symbol>` is an unapplied relocation; the symbol in the relocation record is the callee. A linear objdump listing is not a success path when a later block is only reached by a taken branch.

Binaries: `artifacts/workbench/rootfs/root/lib/modules/<name>.ko`, the extract of `root/lib/modules/<name>.ko`. MD5 and SHA256 were recomputed and match `exports/kernel-modules.tsv` and `elf/elf-census-extra.tsv`. Vermagic on all nine is `4.9.118+ SMP preempt mod_unload ARMv7`.

| module | size | MD5 | SHA256 |
|---|---:|---|---|
| ntkndpcodec.ko | 489788 | 76b9671c0064a4c674a99997796d405c | e5e15b41fa82b3fe861df326cae33ccd7840a25b5b9682003c48e7df92a1c37e |
| ntkndptuner.ko | 7132 | 84be071efa5d16c2b6e8d13db667937a | 8fd65ab9a3aa3203f3adfca25d9948c47e65f611b929b42c4b95020fda15be31 |
| ntkndpdemod.ko | 37260 | 607e40dc6f5252efeb5c89d30fb8e066 | 1eb62cd4a6bab43122f65a619a8b10fe24638e725cdbc9cc67aaf6d1d026b94a |
| ntkndptsd.ko | 273732 | 2470bb737131e4f803163ba9b1dce569 | 44e9d6888197df8ae794806115d8a6e3e9808e7dbf28dcd36eb27d73457f2a95 |
| ntkndp_pq.ko | 141576 | c208cdd0967cddf8cba7c826adf3f73a | 025a5ba97a85a2ddca71fa3dd05b5d52fd94c8a85e1e5ee1525a07d518c0a97f |
| PreTcon.ko | 155524 | bf1d64f7e40555c913d1c7425dde552c | 20311a77c7730f26c7e069f8b8c1a73a0c43a3846a32b0c0fe783ad68a532d87 |
| ntkndpci.ko | 7828 | 6a06a1bdf95dffa93371b9575a6d294f | 27d4d192509d3cea807ff41630da8e53bc08c735fe2bfc11245fba2b070fadfc |
| kteeclient.ko | 26228 | 235215638f06201efe9bbebbb6389c9b | 075c885338a15e179c4eaee4538dadd3deb1b67241dc3719558b9e1b8b9444d9 |
| ntkndphdmi.ko | 94816 | 930204bb8a2d977bd7f2501a575a0964 | 5d2bac7d507dde9c39ac348948a8bdddc59a37f2610bb7ba20019167265b7a4b |

`file_operations` tables that are 120 bytes put `unlocked_ioctl` at +0x24, `mmap` at +0x2c, `open` at +0x30 and `release` at +0x38. That is the Linux 4.9 `file_operations` layout once `iterate_shared` is present. The 36-byte tables are `v4l2_file_operations` without a compat slot: owner, read, write, poll, unlocked_ioctl, get_unmapped_area, mmap, open, release. The 460-byte tables are 115 pointers, `v4l2_ioctl_ops`. Slot 0 is querycap, slot 54 is streamon, slot 112 is subscribe_event. `video_ioctl2` owns the VIDIOC numbers. Those numbers are not immediates in the module.

## ntkndpcodec.ko

`.modinfo`: `license=GPL`, `depends=ntkdriver,PreInit`. No `parm`. No `__kstrtab` export.

`init_module` is `.text+0x370d0` and is also `ndp_codec_init`. It calls, then returns 0 regardless of the three results:

1. `.text+0x370e4` `ndp_mfc_driver_init` (`.text+0x89d4`)
2. `.text+0x370e8` `ndp_venc_driver_init` (`.text+0x20044`)
3. `.text+0x370ec` `ndp_img_driver_init` (`.text+0x2ddfc`)
4. `.text+0x370f0` `mov r0, #0`

`cleanup_module` / `ndp_codec_exit` at `.text+0x370f8` calls `ndp_mfc_driver_exit` (`.text+0x8a3c`), `ndp_venc_driver_exit` (`.text+0x200a8`), `ndp_img_driver_exit` (`.text+0x2de68`).

Sub-driver success paths:

- MFC `.text+0x89d4`: `__platform_driver_register` of the driver object at `.data+0xc18` (literal `.text+0x8a38`). Success prints `NDP-MFC driver is built at %s, %s` with `Dec 17 2021` and `14:52:25` (`.rodata.str1.4+0x2bc0`, `+0x2ba8`, `+0x2bb4`). Failure prints `[ERR_DRV_VDEC]Failed to register platform driver` (`.rodata.str1.4+0x2b68`) and returns the register result. Exit only unregisters that driver.
- VENC `.text+0x20044`: `__platform_driver_register` of `.data+0x2d00`. Both the zero and nonzero results fall through to `mov r0, r4` / `ldm` at `.text+0x20074`. The `__dynamic_nvt_dbg` block at `.text+0x2007c` is not reached. Exit unregisters `.data+0x2d00`.
- IMG `.text+0x2ddfc`: `platform_device_register` of `.data+0x3f60`. On failure it printks and still falls into `__platform_driver_register` of `.data+0x4148`. A failed driver register printks and returns that error. Success returns 0. Exit unregisters the driver (`add r0, anchor, #0xc58`) and the device (`add r0, anchor, #0xa70`).

Three `v4l2_file_operations` in `.rodata`, each 36 bytes. Filled slots:

| table | owner | poll | unlocked_ioctl | mmap | open | release |
|---|---|---|---|---|---|---|
| `ndp_mfc_fops` +0x150 | `__this_module` | `ndp_mfc_poll` | `video_ioctl2` | `ndp_mfc_mmap` | `ndp_mfc_open_file` | `ndp_mfc_release_file` |
| `ndp_venc_fops` +0xcc0 | `__this_module` | `ndp_venc_poll` | `video_ioctl2` | `ndp_venc_mmap` | `ndp_venc_open` | `ndp_venc_release` |
| `ndp_img_fops` +0x177c | `__this_module` | `ndp_img_poll` | `video_ioctl2` | `ndp_img_mmap` | `ndp_img_open_file` | `ndp_img_release_file` |

`ndp_mfc_compat_ioctl32` at `.text+0xf0` is not in `ndp_mfc_fops`. It stores 4 at `[[r0,#0xb4],#0x44]` and tail-calls the function pointer at `[[r0,#0xb4],#0xc0]`. A `R_ARM_MOVW_ABS_NC`/`R_ARM_MOVT_ABS` pair at `.text+0x8824` names it, so some probe stores it elsewhere.

`v4l2_ioctl_ops` filled slots (empty slots omitted). Names are the relocated function symbols:

`ndp_mfc_dec_ioctl_ops` `.rodata+0x174`: 0 `vidioc_querycap`, 1 `vidioc_enum_fmt_vid_cap`, 3 `vidioc_enum_fmt_vid_out`, 4/5 mplane enum, 8 `vidioc_g_fmt_cap`, 10 `vidioc_g_fmt_out`, 16 `vidioc_g_fmt_cap_mplane`, 20 `vidioc_s_fmt_cap`, 22 `vidioc_s_fmt_out`, 28 `vidioc_s_fmt_cap_mplane`, 32 `vidioc_try_fmt_cap`, 34 `vidioc_try_fmt_out`, 44 `vidioc_reqbufs`, 45 `vidioc_querybuf`, 46 `vidioc_qbuf`, 48 `vidioc_dqbuf`, 54 `vidioc_streamon`, 55 `vidioc_streamoff`, 67/68 `vidioc_g/s_ctrl`, 69/70 `vidioc_g/s_ext_ctrls`, 91 `vidioc_decoder_cmd`, 112/113 subscribe/unsubscribe.

`ndp_venc_ioctl_ops` `.rodata+0xce4`: the `vidioc_venc_*` twins of those slots, plus slot 17 `vidioc_venc_g_fmt_out_mplane` and slot 94 `vidioc_venc_s_parm`. No decoder_cmd slot. No slot 28.

`ndp_img_dec_ioctl_ops` `.rodata+0x17a0`: the same slot set as MFC, symbols `imgioc_*` (`imgioc_querycap` through `imgioc_unsubscribe_event`, including slot 91 `imgioc_decoder_cmd`).

`__video_register_device` call sites and the format strings relocated next to `video_device_alloc`:

- `.text+0x2ce0` (inside `ndp_mfc_probe`): `.LC159` = `imvq-mfc-dec` (`.rodata.str1.4+0x1078`) and `.LC160` = `%s%d` (`+0x1088`).
- `.text+0x1f1f0` (inside `ndp_venc_probe`): `.LC82` = `imvq-venc` (`+0x78ec`) and `%s%d`.
- `.text+0x27c24` (inside `ndp_img_probe`): `.LC116` = `imvq-img-dec` (`+0xa2bc`) and `%s%d`.

Separate literal names also exist and are not the strings at those three sites: `ndp-mfc-dec0`..`ndp-mfc-dec3` (`.rodata.str1.4+0x2e74` step 0x10), `ndp-venc-h264` (`+0x7668`), `ndp-venc-jpeg` (`+0x7678`). There is no `/dev/ndp*` string. `/dev/sd`, `/proc/mounts` and `/dev/fuse` are filesystem probes, not codec nodes.

`depends` does not name `kteeclient`, but `.rodata.str1.4` contains `kteec_kobj_invoke_cmd` at +0x55d0, +0x627c and +0xd23c. That is a runtime symbol lookup, not a modinfo dependency. Codec behavior strings also name MFC/VENC/IMG frontend open, close, connect, disconnect and flush.

## ntkndptuner.ko

`.modinfo`: `license=GPL`, `author=by SP/SE3`, `description=Tuner module API`, `depends=ntkdriver`. No parm. No `/dev`, `/proc` or sysfs name.

Export: `__kstrtab_ndp_tuner_attach` only. `ndp_tuner_attach` is `.text+0x1c`. It copies 0xdc bytes of `ndp_tuner_ops` onto the frontend at `r0+0x120` and stores the anchor at `[r0,#0x278]`.

`ndp_tuner_ops` is `.rodata+0`, size 220. First bytes are the C string `tuner`. Filled function slots, after the 152-byte `dvb_tuner_info`:

| slot | offset | symbol | ops field this offset matches |
|---:|---:|---|---|
| 39 | 0x9c | `ndp_tuner_hw_init` | init (release at slot 38 is empty) |
| 40 | 0xa0 | `ndp_tuner_sleep` | sleep |
| 43 | 0xac | `ndp_tuner_set_params` | set_params |
| 44 | 0xb0 | `ndp_tuner_set_analog_params` | set_analog_params |
| 49 | 0xc4 | `ndp_tuner_get_ststus` | get_status (the symbol is spelled that way) |
| 50 | 0xc8 | `ndp_tuner_get_rf_strength` | get_rf_strength |
| 51 | 0xcc | `ndp_tuner_get_afc` | get_afc |

`init_module` and `ndp_tuner_init` are both `.init.text+0`. Success path, read from the branches:

1. `i2c_get_adapter(0)`. Null: `mvn r5, #11` (-12), `kfree`, return.
2. `i2c_new_device`. Null: `mvn r5, #5` (-6), `kfree`, return.
3. `i2c_put_adapter`.
4. `kmem_cache_alloc_trace` size 0x30, flags `movw r1,#0x80c0` / `movt r1,#0x240` = `0x024080c0`. Null: `dev_err`, `mvn r5,#11`, `kfree`, return.
5. Store the i2c client at `[obj,#0x24]`, `client->dev` at `[obj,#0x28]`, `__mutex_init`, `memcpy` 0xdc bytes of `ndp_tuner_ops` to `[obj+0x28]+0x120`.
6. Store 5 at `[anchor,#8]`, clear `[anchor,#12]` and `[anchor,#16]`, call `KER_TUNER_Init(anchor+8)`.
7. Return 0 (`r5` was cleared at `.init.text+0xa4` before the copy).

`cleanup_module` and `ndp_tuner_exit` are both `.exit.text+0`. The body is the prologue and `ldm sp, {fp, sp, pc}` at +0xc. Nothing is unregistered.

## ntkndpdemod.ko

`.modinfo`: `license=GPL`, `description=Novatek Internal DVBT/DVBC/ISDBT/ATSC/J83B demodulator driver`, `author=Novatek`, `depends=ntkdriver`. No parm. No device-path string.

Exports: `ndp_fe_attach`, `stAdapter`.

`init_module` / `ndp_demod_init` is `.init.text+0x1dc`, size 440. `ndp_tuner_init` is a different function, `.init.text+0`, size 476, and it ends where `init_module` starts. No relocation in this module has the symbol `ndp_tuner_init` as its target. Its own body does call `KER_TUNER_Init` (`.rel.init.text` at offset 0x174).

Success path of `init_module`:

1. `nvt_platform_bundle_simple`. `cmn r0,#0x1000` / `bhi` stores the error and returns it.
2. `nvt_printk_register`, `nvt_info_probe`.
3. `i2c_get_adapter(0)`. Null: `mvn r4,#11`, `nvt_warn`, `kfree`, another `nvt_warn`, return.
4. `i2c_new_device`. Null: `mvn r4,#5`, same failure tail.
5. `i2c_put_adapter`.
6. `kmem_cache_alloc_trace` size `0x4f8`, same `0x024080c0` flags. Null: `mvn r4,#11`.
7. `memcpy` 0x270 bytes from `.rodata+0x4c0`, `dvb_register_adapter` (adapter-number pointer is the halfword at `fp-38`, which was stored as 0).
8. `.init.text+0x328` is `R_ARM_CALL ndp_fe_attach`. Objdump prints `bl a4 <ndp_tuner_init+0xa4>` because 0xa4 sits inside the `.init.text` span of `ndp_tuner_init`. The relocation is `ndp_fe_attach` (`.text+0xa4`, size 188).
9. `dvb_register_frontend`, `DRV_DBG_Register(5, 0x100000)`, `tztv_int_demod_init`, return `r4` (still 0).

`ndp_fe_attach` copies 0x4c0 bytes, then overwrites the start with 0x270 bytes from `.rodata+0x4c0`. That object begins with the string `internal_fe`. Its function relocations, relative to +0x4c0:

| + | symbol |
|---:|---|
| 0xb0 | `ndp_fe_release` |
| 0xb8 | `ndp_fe_init` |
| 0xbc | `ndp_fe_sleep` |
| 0xc4 | `ndp_fe_tune` |
| 0xcc | `ndp_fe_set_frontend` |
| 0xd0 | `ndp_fe_get_tune_settings` |
| 0xd4 | `ndp_fe_get_frontend` |
| 0xd8 | `ndp_fe_read_status` |
| 0xdc | `ndp_fe_read_ber` |
| 0xe4 | `ndp_fe_read_snr` |
| 0xe8 | `ndp_fe_read_unc_blocks` |
| 0xf0 | `ndp_fe_send_diseqc_msg` |
| 0xf8 | `ndp_fe_diseqc_send_burst` |
| 0xfc | `ndp_fe_set_tone` |
| 0x100 | `ndp_fe_set_voltage` |
| 0x110 | `ndp_fe_set_ts_out` |
| 0x220 | `ndp_fe_set_property` |
| 0x224 | `ndp_fe_get_property` |
| 0x228 | `ndp_fe_send_dbg_cmd` |
| 0x22c | `ndp_fe_get_lock_time` |
| 0x25c | `ndp_fe_get_cellid` |
| 0x264 | `ndp_fe_get_signal_quality` |
| 0x268 | `ndp_fe_read_signal_strength` |
| 0x26c | `ndp_fe_get_stream_id` |

The demod's own `ndp_tuner_ops` is `.rodata+0x794`, size 220, name bytes `tuner`. Only slot 39 `ndp_tuner_hw_init` and slot 43 `ndp_tuner_set_params` are filled. It does not import `ntkndptuner.ko`'s `ndp_tuner_attach`.

`set_frontend` strings (`.rodata.str1.4`): `DVBT, BW:%dMHz`, `ISDBT BW to %dMHz`, `8VSB, BW:6`, `J83B, BW:6`, `DVBT2, BW:%dMHz,PLP:%d`, `DTMB, BW:6`, `Not Support ModType`, and `ker_demod not initial`. A type legend says `1:DVBC,2:J83B,3:DVBT,5:DVBS,6:DVBS2,8:ISDBT,11:8VSB,13:DTMB,16:DVBT2`. I2C name `nt316tb_i2c_dev`. Secure/TEE-side entry points named in strings: `tztv_int_demod_init`, `close`, `get_demod_status`, `tune`, `get_next_freq`, `set_ts_output`, `extension_cmd`, `BlindScanUpdate`, `send_diseqc_sequence`.

`cleanup_module` / `ndp_demod_exit` at `.exit.text+0` is only `ldm` at +0xc.

## ntkndptsd.ko

`.modinfo`: `license=GPL`, `author=Novatek`, `description=Novatek Demux device driver`, `depends=ntkdriver,ntkndpci,PreInit,ntkndpdemod`. One parameter: `parm=adapter_nr:DVB adapter numbers`, `parmtype=adapter_nr:array of short`. No ksymtab export.

`init_module` at `.text+0x1420` calls `KER_Demux_DrvInit` and returns its result. `cleanup_module` at `.text+0x5cc` calls `KER_Demux_DrvExit`.

`KER_Demux_DrvInit` is `.init.text+0`, and the success-side calls (debug `tst` blocks that `nop`/`b` over `__dynamic_nvt_dbg` are skipped) are:

1. `kmem_cache_alloc_trace` size `0xb40`, flags `0x024080c0`. Null returns -12.
2. `nvt_platform_bundle_simple`. Error returns that pointer.
3. `nvt_printk_register`, `printk`, `DRV_TSPU_Load_MemInfo`.
4. `__get_free_pages(0x024000c0, 6)`. Null returns -3 (`mvn r7,#2`).
5. `DRV_TSPU_Init`, `DRV_TSPU_CMA_GetCmaStatus`.
6. Two demux bring-ups. Each calls `dvb_dmx_init`, `dvb_dmxdev_init`, `DRV_TSPU_PCR_Open`. Between them the init stores `nvt_demux_connect_frontend`, `nvt_demux_disconnect_frontend`, `nvt_demux_set_source`, `nvt_demux_write`, `nvt_demux_get_stc`, `nvt_demux_hw_match_filter`, `nvt_demux_ioctl`, `nvt_demux_get_status`. `nvt_demux_ioctl` is stored at `.init.text+0x5dc` and again at `.init.text+0x764` (`R_ARM_MOVW_ABS_NC`/`MOVT` to `.text+0x15c`).
7. Also on the way: `DRV_TSPU_TSI_Config` twice, `DRV_TSPU_DP_Open` twice, `NTCI_SetDpHandle` twice, `DRV_TSPU_DESC_Config` twice, `kthread_create_on_node` / `wake_up_process`, `KER_DEMUX_DBG_Register`, `debugfs_create_dir` of the string `tsd` (`.rodata.str1.4+0x263c`, reloc `.init.text+0x7dc`), `debugfs_create_u8`, `Plat_Audio_InstallCB`, `KER_DEMOD_Regist_TSPUPath`.

Strings that describe the topology: `Request from demux_0`, `demux_1`, `Internal demod0`, `Internal demod1`, `External demod`, `Owner: demux1` (`.rodata.str1.4+0x38bc`), `add_frontend` / `DMX_FRONTEND_0` / `DMX_MEMORY_FE` class messages, section filter value/mask/mode, PES/PCR, video ES buffer. Source path baked in: `.../ndp_tsd/ker_demux_dbg.c`.

`nvt_demux_ioctl` (`.text+0x15c`) copies `r1` to `r5` and compares that command. It is not the V4L2/DMX core file ioctl. Accepted commands:

| command | decode | effect |
|---|---|---|
| `0x40046f61` | `_IOW('o', 0x61, 4 bytes)` | `(cmd + 0xbffb909f) <= 1` at `.text+0x308`..`0x318`, then return 0 |
| `0x40046f62` | `_IOW('o', 0x62, 4 bytes)` | same compare, return 0 |
| `0xc0086f63` | `_IOWR('o', 0x63, 8 bytes)` | `.text+0x1ac` equal, return 0 |
| `0xc0086f64` | `_IOWR('o', 0x64, 8 bytes)` | `.text+0x1c0`. If the file matches one of two demux objects (`[anchor+8]+0x48` or `+0x358`), call `DRV_TSPU_FILT_QueryBuffer` twice and write two counts through `arg` |
| `0xc0086f68` | `_IOWR('o', 0x68, 8 bytes)` | `.text+0x1d0`. Calls `TSPU_ESNotifyFn` |

Anything else is `mvn r5, #21` at `.text+0x31c`, return -22. The `0xbffb909f` addend is `movw r3,#0x909f` / `movt r3,#0xbffb` at `.text+0x308`.

`fops.32478` at `.rodata+0x9a4`, size 120, has one filled slot: slot 3 `scs_write` (the `write` member). `SCS_Init` at `.text+0x10358` calls `proc_create_data` at `.text+0x103a0` with name `.LC50` = `idx` (`.rodata.str1.4+0x43b8`), mode `0x1a4` (0644), parent 0, and the `.rodata` addend word at `.text+0x103c0` equal to `0x9a4` (`fops.32478`). It then calls `scs_debug_init`. This proc node is not the DVB demux device node. DVB device nodes come from `dvb_dmxdev_init` / `dvb_register_adapter`.

## ntkndp_pq.ko

`.modinfo`: `description=Third Party PQ Template Driver`, `author=Third Party Developer`, `license=GPL`, `depends=ntkdriver,PreInit`, OF aliases `of:N*T*Csamsung,sdp-pq` and `of:N*T*Csamsung,sdp-pqC*`. No parm. Export: `NVT_ex_proc`.

`init_module` / `ndp_pq_initial` at `.init.text+0`. Success path:

1. `ndp_pq_NvtPrintk_Register` (`.text+0xb63c`).
2. Optional `printk` pair if bit 0 of the debug word is set.
3. `__platform_driver_register` of the driver at `.data+0x124` (literal `.init.text+0x10c`). A negative return is returned as-is (`.init.text+0x58` `blt`).
4. `memset` 48 bytes, `platform_device_register_full`. The result is stored at `[anchor,#0xf84]`.
5. If `cmn r0,#0x1000` is HI: `platform_driver_unregister` and return that error pointer.
6. If OK: `DRV_DBG_Register(5, 0x100000)` and return 0.

`cleanup_module` / `ndp_pq_exit` unregisters the platform device at `[anchor,#0xf84]` and the driver at `.data+0x124`, with optional printks around them.

`ndp_pq_probe` at `.text+0x298` registers the char device:

- `device_create_file` of `dev_attr_debug`. That attribute is `.data+0x4`: name string `debug` (addend `0x5ee0` into `.rodata.str1.4`), mode 0644, show `show_pq_debug` (`.text+0xa8f8`), store `store_pq_debug` (`.text+0xa958`).
- `vk_alloc_chrdev_region(dev, baseminor 0, count 1, "ndppq")`. The name is `.LC12` at `.rodata.str1.4+0x128`, reloc `.text+0x2f8`.
- On success, `vk_cdev_init` / `vk_cdev_add` of one minor.
- `__class_create(__this_module, "ndppq")` and `device_create` with the same `"ndppq"` format (`r5` loaded at `.text+0x388`). The node name is `ndppq`.
- Then `_ker_osd_drm_get_fbinfo` and three `_ker_osd_drm_fb_Ioctl` calls: `0x4001c83e` with arg 1, `0x4004c840` with arg 0, `0x4008c80a` with a 14 and two flag bytes. The failure strings next to those calls are `OSD ALPHA ENABLE`, `OSD ALPHA THRD` and `OSD UPDATE ENABLE`.
- `PLAT_PQ_SHELL_Initialize`. Return 1 is the success compare (`cmp r0,#1`). Failure still returns into the caller after `nvt_warn` of `platform PQ init failed.` and prints `NDP-PQ-0.0.043-HEADER`.

`g_pqfops` is `.data+0x14`, size 120. Slots: 0 `__this_module`, 9 `ndp_PQ_UnlockIOC` (`.text+0xa7c4`), 12 `ndp_PQ_Open`, 14 `ndp_PQ_Close`.

`ndp_PQ_UnlockIOC` copies 0x54 bytes from userspace. A failed access check returns -14 (`mvn r0,#13` at `.text+0xa8ec`). It then calls `ndp_PQ_CmdHandle(cmd, first three words of that buffer)` at `.text+0xa8a0`.

`ndp_PQ_CmdHandle` (`.text+0x698`) compares `r0`:

- `0xba86` (`movw ip,#0xba86` at `.text+0x6b8`): vsync path, `PLAT_PQ_SHELL_GetVSyncCallback_New_Dimming` and `GetVSyncCallback_New_CE`.
- `0xba87` (`movw r3,#0xba87` at `.text+0x6e0`): loads a subcommand and `cmp r3,#0xd8` / `ldrls pc,[pc,r3,lsl #2]` at `.text+0x788`. Subcommands 0..216. The table targets call `PLAT_PQ_SHELL_SetPictureQuality`, `SetContrast`, `SetBrightness`, `SetSharpness`, `SetSaturation`, `SetHue`, `SetGamma`, `SetWBParam`, and the other `PLAT_PQ_SHELL_*` setters. This pass did not name all 217 slots.
- Any other `r0`: two `nvt_warn`s and return `0xfffffdf4` (`movw r5,#0xfdf4` / `movt r5,#0xffff` at `.text+0x700`).

## PreTcon.ko

`.modinfo`: `description=PreTCON Driver`, `author=Third Party Chip Manufacturer`, `license=GPL`, `depends=` empty. No parm, no ksymtab, no `file_operations`, no `/dev` or `/proc` string.

`init_module` at `.init.text+0` has no branch. Order:

1. `PRE_TCON_Init` (`.text+0x7b00`).
2. `printk` of `.LC117` = `-> TCON: tcon_perf: dump end (cost: %u msec)` (`.rodata.str1.4+0x138c`).
3. `free_pages` of the pointer at `[anchor,#0x9a0]`, order from `[.LANCHOR2,#0x9e8]`, then stores zero over those three words.
4. `pre_tcon_reg_cleanup` (`.text+0xa784`).
5. `mov r0, #0` and return.

`cleanup_module` at `.exit.text+0` only printks `.LC30` (`%s: Bye`, `.rodata.str1.4+0x584`, with a leading 0x13 log byte) and returns.

`PRE_TCON_Init` begins at `.text+0x7b18` with `movw r0,#0x13b4` / `movt r0,#0xfc0b` and `bl pre_tcon_reg_virt`. That is register `0xFC0B13B4`. The rest of the function is large (next global is `PRE_TCON_CLK_Update` at `.text+0xa8c8`). Strings, not a full walk of every branch, establish the TCON contract:

- `TCON.BIN`, id `0xF0000003` (`vcm-sel`), `TCON.BIN main partition fail, try backup partition`, `TCON data already load ... by xboot`, full-64k checksum.
- `romgen.bin`: `force romgen 0xFC0B0300[22]=1`, `ignore 0xFC0B031C temporary`, `more than one 0xFC0B031C`.
- Demura: `_AUO_demura_proc`, `PRE_TCON_set_demura_on_off`, `plane_sel > 3`, cell-flash demura header and checksum, `demura` DT memory, `cell_hw_working_buf_base` / `size`.
- Overdrive: DT node name `tcon_od`, `of_find_node_by_name` failure string, `od_hw_working_buf_base`.
- Clock helpers, all global: `PRE_TCON_SetMPLLByteReg` `.text+0xa9b8`, `PRE_TCON_SetClockSource` `.text+0xaa5c`, `PRE_TCON_SetClockReset` `.text+0xab3c`, `PRE_TCON_CLK_Update` `.text+0xa8c8`.
- Interface names in the string table: USIT, EPI, CEDS, ISP.

## ntkndpci.ko

`.modinfo`: `license=GPL`, `author=Novatek`, `description=Novatek CI device driver`, `depends=ntkdriver,ntkndpdemod`. No parm. No char-device path.

Exports: `NTCI_GetRouteMode` (`.text+0x150`, tail-calls `DRV_NTCI_GetRouteMode`), `NTCI_SetDpHandle` (`.text+0x16c`, tail-calls `DRV_NTCI_SetDpHandle`).

`init_module` at `.text+0x198` calls `ndp_CI_DrvInit` and returns its result. `cleanup_module` at `.text+0x74` calls `ndp_CI_DrvExit`.

`ndp_CI_DrvInit` (`.init.text+0`) success path:

1. `kmem_cache_alloc_trace` size `0x98`, flags `0x024080c0`. Null: printk `out of memory`, return -12.
2. `kthread_create_on_node` of `ndp_CI_TaskTimer` named `KER_CI_timer` (`.rodata.str1.4+0x40`). `bhi` after `cmn` printks `Create CI Timer thread fail` and continues. Otherwise `wake_up_process`.
3. Fills the 0x98-byte object from relocations of `ndp_ci_read_attribute_mem`, `ndp_ci_write_attribute_mem`, `ndp_ci_read_cam_control`, `ndp_ci_write_cam_control`, `ndp_ci_slot_reset`, `ndp_ci_slot_shutdown`, `ndp_ci_slot_ts_enable`, `ndp_ci_poll_slot_status`, `ndp_ci_slot_set_frontend_source`, `ndp_ci_slot_set_ts_route_mode`. ORs 3 into a flags word. Calls `dvb_ca_en50221_init` at `.init.text+0x14c`.
4. Nonzero: printk `dvb_ca_en50221_init is Failed!!` and `ndp_ci: failed %d`, return -1.
5. Zero: store 1 at `[obj,#0x90]`, printk `ndp ci DVB CA en50221 init successfully.`, return 0.

`ndp_CI_DrvExit`: if the object pointer is null, return. Else `dvb_ca_en50221_release`, `kthread_stop` on two task pointers, `kfree`.

`ndp_NTCI_INIT_REG` at `.text+0x248` only calls `DRV_NTCI_INIT_REG`. It is not called from `init_module`.

## kteeclient.ko

`.modinfo`: `description=NOVATEK MICROELECTRONICS CORP - TEE CLIENT MODULE`, author string includes `www.novatek.com.tw`, `license=GPL`, `depends=` empty. Parameters: `resume_cnt` uint, `resume_timestamp` uint. Source path string: `/home/nvt01486/TEE/tee/TeeClientDriver/src/kteec.c`.

Exports: `kteec_kobj_invoke_cmd` (`.text+0x8c`), `tzdev_scm_watch`.

`init_module` at `.init.text+0` calls `kteec_init` (`.text+0x2460`). That function registers the platform device at `.data+0x230`, then `__platform_driver_register` of `.data+0x418`. A failed device register printks and returns the error. A failed driver register unregisters the device, printks, and returns the error. Success returns 0.

`cleanup_module` at `.exit.text+0` always calls `platform_driver_unregister` on `anchor+0x418` and `platform_device_unregister` on `anchor+0x230`.

`kteec_probe` (`.text+0x474`) does the SMC/secure-call handshake (`smp_call_function_single` into `kteec_do_secure_call`, magic `0x12345678` / `0x87654321`, and `0x73726576` which is the bytes of `vers`). Then:

- `alloc_chrdev_region` count 1, baseminor 0, name `.LC2` = `kteeclient` (`.rodata.str1.4+0x34`), reloc `.text+0x634`.
- `__class_create` of the same `kteeclient` string.
- `device_create` format `.LC3` = `teec` (`.rodata.str1.4+0x40`), reloc `.text+0x684`. The device node name is `teec`.
- `cdev_init` / `cdev_add` of one minor.
- `ioremap_cache`, `kteec_get_revision`, printk `kteeclient module revision [%s]. secos revision: [%s]`, `kteec_wsm_init`, four `kteec_wsm_alloc`, then `kthread_create_on_node` of `kteec_kobj_service_task` with name `.LC5` = `teec_kservice` (`.rodata.str1.4+0x80`). Another string is `teec_service`.
- Failure tails call `device_destroy`, `class_destroy`, `cdev_del`, `unregister_chrdev_region`, `kteec_wsm_free`.

`g_kteec_fops` at `.data+0x124`, size 120. Slots: 0 `__this_module`, 9 `kteec_ioctl` (`.text+0x1afc`), 11 `kteec_mmap`, 12 `kteec_open`, 14 `kteec_release`.

`kteec_ioctl` switch at `.text+0x1b18`: `r3 = 0x3ffb1200`, `add r3, r1, r3`, `cmp r3,#4`, `ldrls pc,[pc,r3,lsl #2]`, else `b .text+0x240c`. Index `cmd + 0x3ffb1200` in 0..4, so the five commands are `0xc004ee00`..`0xc004ee04` (`_IOWR(0xee, 0..4, 4 bytes)`).

| cmd | handler | first lock | copy size |
|---|---|---|---|
| `0xc004ee00` | `.text+0x1b94` | `ioctl_shmem_mgmt_lock` at `.data+0x1c0` | 40 |
| `0xc004ee01` | `.text+0x1d1c` | `.data+0x1c0` again | 40 |
| `0xc004ee02` | `.text+0x1dbc` | `ioctl_open_session_lock` at `.data+0x1e4` | (open-session path) |
| `0xc004ee03` | `.text+0x1f8c` | `ioctl_close_session_lock` at `.data+0x208` | 24 |
| `0xc004ee04` | `.text+0x1b58` | `ioctl_invoke_cmd_lock` at `.data+0x100` | calls `kteec_invoke_command` |

The default at `.text+0x240c` is `movw r0,#515` and the common epilogue. Unknown commands return 515, not a negative errno. The invoke case is the one codec looks up indirectly through the exported `kteec_kobj_invoke_cmd`.

## ntkndphdmi.ko

`.modinfo`: `description=Template HDMI Driver`, `author=Third Party Chip Manufacturer`, `license=GPL`, `depends=ntkdriver`. No parm. Those four strings are byte-identical to `ntkndphdmiswitch.ko` (see the contradiction note). This object is the 94816-byte driver.

`init_module` / `ndp_hdmi_core_init` at `.init.text+0`. The debug blocks are skipped by unconditional `b` instructions at `.init.text+0x2c` and `.init.text+0x138` (`nop` then `b`). Success path:

1. `DRV_DBG_Register(5, 0x100000)` at `.init.text+0xb0`.
2. `__platform_driver_register` of the driver at `.data+0x27c` (literal `.init.text+0x23c`). Negative: return that value at `.init.text+0x228`. No unregister.
3. `memset` 48, `platform_device_register_full`, store the result at `[anchor,#0x10]`.
4. If `cmn` says error: `platform_driver_unregister` at `.init.text+0x1ac`, then `mov r0,#0` and return 0. Device-register failure does not fail `init_module`.
5. If OK: return 0 (the `ands` with the debug bit produces 0 when the bit is clear).

`cleanup_module` / `ndp_hdmi_core_exit` at `.exit.text+0`: same skipped debug blocks, then `platform_device_unregister` of `[anchor,#0x10]` and `platform_driver_unregister` of `.data+0x27c`.

`ndp_hdmi_fops` `.rodata+0x48c`, 36 bytes: slot 0 `__this_module`, slot 3 `_ndp_hdmi_core_poll`, slot 4 `video_ioctl2`, slot 7 `_ndp_hdmi_core_open`, slot 8 `_ndp_hdmi_core_release`. No read, write, or mmap slot.

`ndp_hdmi_ioctl_ops` `.data+0x24`, 460 bytes. Filled slots:

| slot | symbol |
|---:|---|
| 0 | `_ndp_hdmi_core_vidioc_querycap` |
| 8 | `_ndp_hdmi_core_vidioc_g_fmt_vid_cap` |
| 20 | `_ndp_hdmi_core_vidioc_s_fmt_vid_cap` |
| 32 | `_ndp_hdmi_core_vidioc_try_fmt_vid_cap` |
| 54 | `_ndp_hdmi_core_vidioc_streamon` |
| 55 | `_ndp_hdmi_core_vidioc_streamoff` |
| 59 | `_ndp_hdmi_core_vidioc_enum_input` |
| 61 | `_ndp_hdmi_core_vidioc_s_input` |
| 69 | `_ndp_hdmi_core_vidioc_g_ext_ctrls` |
| 70 | `_ndp_hdmi_core_vidioc_s_ext_ctrls` |
| 93 | `_ndp_hdmi_core_vidioc_g_parm` |
| 112 | `_ndp_hdmi_core_vidioc_subscribe_event` |
| 113 | `_ndp_hdmi_core_vidioc_unsubscribe_event` |

`__video_register_device` is at `.text+0x3fa8`, inside `_ndp_hdmi_core_probe`. The name relocated at `.text+0x3f3c` is `.LC45` = `ndp-hdmi` (`.rodata.str1.4+0x68c`). `ndp-hdmi0` is a separate string at `.rodata.str1.4+0x3c8`. Failure string: `Register HDMI video device not success`.

Exported (`__kstrtab_*`), all `.text`:

| symbol | offset |
|---|---:|
| `tztv_hdmi_int_switch_open` | 0x3268 |
| `tztv_hdmi_int_switch_close` | 0x33b0 |
| `tztv_hdmi_int_switch_connect_port` | 0x1070 |
| `tztv_hdmi_int_switch_disconnect_port` | 0x34f8 |
| `tztv_hdmi_int_switch_do_open_short_test` | 0x2908 |
| `tztv_hdmi_int_switch_enable_test_pattern` | 0x3788 |
| `tztv_hdmi_int_switch_get_edid` | 0x1c74 |
| `tztv_hdmi_int_switch_get_jackident` | 0x21c8 |
| `tztv_hdmi_int_switch_read_edid` | 0x243c |
| `tztv_hdmi_int_switch_write_edid` | 0x259c |
| `tztv_hdmi_int_switch_set_arc` | 0x22e0 |
| `tztv_hdmi_int_switch_set_hpd_high` | 0x3640 |
| `tztv_hdmi_int_switch_set_rxsense_high` | 0x2734 |

Global but not in `__kstrtab`: `tztv_hdmi_int_switch_get_arc_port` (`.text+0x4dfc`), `tztv_hdmi_int_switch_set_edid` (`.text+0x4f44`). The `ndp_hdmi_hw_rx_*` symbols (init, start, stop, suspend, resume, EDID/infoframe/color, lock, mute) are the RX state machine behind the v4l2 capture device. They are not a second file_operations table.

## Contradictions with the current inventory

The nine MD5/SHA256/size rows match `kernel-modules.tsv` and `elf-census-extra.tsv`. Nothing here reopens the GPIO-207 or VPLTIO corrections.

`ntkndphdmi.ko` and `ntkndphdmiswitch.ko` have the same `description`, `author`, `license` and `depends` bytes (`Template HDMI Driver`, `Third Party Chip Manufacturer`, `depends=ntkdriver`). They are different files: 94816 bytes / `930204bb8a2d977bd7f2501a575a0964` versus 27624 bytes / `d4ff737dcbd9be2b628f2dd4eccf5d7a`. `ghidra-summary.tsv` has a project only for `hdmiswitch`. A description lookup cannot tell them apart. The module in this note is the v4l2 capture driver that exports `tztv_hdmi_int_switch_*` and registers a video device named `ndp-hdmi`.

`ntkndpdemod` `init_module` does not call `ndp_tuner_init`. The relocation at `.init.text+0x328` is `ndp_fe_attach`. Objdump's `<ndp_tuner_init+0xa4>` label is the `.init.text` address overlap, not the callee. The demod also does not reference the exported `ndp_tuner_attach` from `ntkndptuner.ko`. Its `depends` line is only `ntkdriver`, which matches that.

`ntkndpcodec.ko` `depends=ntkdriver,PreInit` does not name `kteeclient.ko`, while its strings name `kteec_kobj_invoke_cmd`. That is a runtime lookup of the symbol `kteeclient.ko` exports. The modinfo row is not missing a hard dependency that the ELF records.

No census contradiction. These nine are already in the 15-module rootfs census.
