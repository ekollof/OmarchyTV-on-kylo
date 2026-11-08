# DEV_INI device stages — 2026-11-08

Read-only recovery of the seven hardware-facing stages called by `DEV_INI_Init`.
Firmware: `artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf`,
MD5 `64c252340d3af6c5d66072611b7bad7a`. Instructions are from
`artifacts/workbench/aurora.dis` plus ELF bytes at the cited VAs. The Aurora
Ghidra project was left closed. Decompiler prototypes were not used.

Every stage is Thumb, returns in `r0`, and spills the incoming `r0` into the
stack gap. None of the seven bodies reload that slot. `DEV_INI_Init`'s
`movs r0, #0` before `DEV_SPI_Open` does not select a device index inside
`Open`. Stack canary word is `0x01791d68`.

`libNDAL3.so` MD5 `c842bad804302ec822076c3d9d3ce31d`. Symbol values are odd
Thumb addresses; the even entry is the value minus one.

| import | libNDAL3 symbol | even entry |
|---|---|---|
| `Hal_Audio_Ctrl` | `0x4e7af6cd` | `0x4e7af6cc` |
| `Hal_Iic_Read` | `0x4e7bf269` | `0x4e7bf268` |
| `Hal_Iic_Write` | `0x4e7bf2b1` | `0x4e7bf2b0` |

No call into `libNDAL_EX.so` (MD5 `e6a465861d1869b8b138b3f299c89f87`) appears in
these stage bodies. HAL bodies inside libNDAL3 were not decompiled.

## DEV_INI_Init order and ownership

`DEV_INI_Init` @ `0x00853a58`. The call order in
`artifacts/reports/COLD-START-STAGE-AUDIT-2026-11-06.md` matches the
instructions. The audit sentence "each return is checked; a zero logs and
continues" does not. Recovered checks:

| order | call | site | on zero |
|---:|---|---|---|
| 1 | `DEV_COMMON_SET_PCBLAYOUTTYPE(14)` @ `0x00850288` | `0x00853a78` | return ignored; `printf` always runs |
| 2 | `DEV_GPIO_Init` @ `0x00851fcc` | `0x00853a92` | `TD_DBG_Print(0x100, 2, "#DEV-INI#DEV_GPIO_Init failed")`, then continue |
| 3 | `dev_src_init` @ `0x008fc654` | `0x00853a9c` | same printer, `"#DEV-INI#dev_src_init failed"` |
| 4 | `DEV_PANEL_Init` | `0x00853aa4` | `"#DEV-INI#DEV_PANEL_Init failed"` |
| 5 | `DEV_NIM_Init` | `0x00853aac` | `"#DEV-INI#DEV_NIM_Init failed"` |
| 6 | `DEV_IIC_Init` @ `0x008531e8` | `0x00853ab4` | `printf("DEV_IIC_Init failed")`, not `TD_DBG_Print` |
| 7 | `DEV_AMP_Init` | `0x00853abc` | `"#DEV-INI#DEV_AMP_Init failed"` |
| 8 | `DEV_SPI_Init` | `0x00853ac4` | `"#DEV-INI#DEV_SPI_Init failed"` |
| 9 | `DEV_SPI_Open` | `0x00853ace` | `cbz`; `"#DEV-INI#DEV_SPIOpen failed"` |
| 10 | `DEV_NIM_DTV_Init` | `0x00853ad4` | `cbz`; `"#DEV-INI#DEV_NIM_DTV_Init failed"` |
| 11 | `DEV_MTD_Init` | `0x00853ada` | `uxtb` into `r5`; zero prints `"#DEV-INI#DEV_MTD_Init failed"` |

Strings live at `0x013588f8`, `0x01358918`, `0x01358938`, `0x01358958`,
`0x01358978`, `0x0135898c`, `0x013589ac`, `0x013589cc`, `0x013589e8`,
`0x01358a0c`. After MTD, success or failure, `0x00853b0a` always calls
`PLAT_SP_DebugRegisterMenu(3, 0x100000, "DEV_INI", 0x01760ae0)`. The function
returns `r5`, the low byte of `DEV_MTD_Init`. Earlier failures do not change
it. `DEV_MTD_Init` itself always returns 0, so the MTD log always runs and
`DEV_INI_Init` returns 0 when it reaches the epilogue.

`DEV_NIM_Init` and `DEV_NIM_DTV_Init` both end in `movs r0, #1` on every path
recovered below, so their failure logs are unreachable from those bodies.
`DEV_PANEL_Init` can return 0. `DEV_AMP_Init` returns 0 only when task creation
returns 0. `DEV_SPI_Init` / `DEV_SPI_Open` return 0 when the semaphore is
missing or the open fails.

Stages 1, 2, 3 and 6 are the other worker's bodies. This report only records
how `DEV_INI_Init` uses them. `DEV_INI_BY_PCB_Layout_Setting_Init` and
`DEV_AUD_INI_Init` are different functions and were not re-derived.

## DEV_PANEL_Init

Entry `0x008583fc`. No argument is consumed. No idempotency byte. Each call
re-registers a debug handler and rewrites the globals below.

Sequence:

1. `TD_DBG_Register(3, 0x100000, 0x0135acb8, 0x017616dc)`.
2. Store the timing block at `0x019354b4`: byte `+0 = 1`, halfwords `+2 = 10`,
   `+4 = 40`, `+6 = 800`, `+8 = 220`, `+10 = 55`, `+12 = 10`.
3. `DEV_GPIO_GetDescription(110)`. A non-null pointer is copied as 32 bytes to
   `0x0193545c`. A null pointer does not fail the function; a nonzero byte at
   `0x01935458` only enables `PLAT_SP_DebugWrite`.
4. `DEV_GPIO_GetDescription(111)`, same 32-byte copy to `0x01935494`. Null is
   the same non-fatal debug path.
5. `DEV_PWM_Get_ChIdx(108, &stack_word)`. The stack word is preset to 13 and
   is not read back. Return 0 skips the curve and becomes this function's
   return 0 (`mov r4, r0` then the epilogue returns `r4`).
6. Nonzero: `DEV_PANEL_SetBacklightPWMCurve(40, 60, 80, 100)`, then
   `strh 1000` at `0x019354c6`. Return is the nonzero channel-index result,
   which is 1 (`movle r0, #1` inside `Get_ChIdx`).

`DEV_GPIO_GetDescription` @ `0x00850c38` indexes the table at `0x01931270` by
`id * 32` and compares the byte at `record+8` with the id. On a mismatch it
waits on the semaphore at `0x0193127c` and calls
`ioctl(fd at 0x0175f0a4, 0xc088671f, ...)`. A null table pointer takes
`0x00850dce`, which was not walked. The device node behind that fd was not
recovered here.

`DEV_PWM_Get_ChIdx` @ `0x00851f0c` does not ioctl. It calls
`GetDescription`, then walks four 12-byte records at `0x0175f320`. A record
matches when `word+0 <= description+4` and `word+0 + byte+4 > description+4`.
The stored channel is `(description+4 - word+0) + word+8`. Return 1 only when
that sum is `<= 12`. Otherwise it prints and returns 0. A null description or
a null out-pointer returns 0.

`DEV_PANEL_SetBacklightPWMCurve` @ `0x00858320` writes halfwords at
`0x019354cc` and returns 1. It reads the minimum `M` from the halfword at
`0x019354c8`, which this Init does not store. The arithmetic is owned by
`artifacts/reports/PANEL-CURVE-NIM-DTV-2026-11-06.md` and
`openaurora/common/panel_curve.c`. Init does not call
`DEV_PANEL_SetPWMBacklightLv` @ `0x0085801c`, `DEV_PANEL_PanelPowerSet`, or
`Hal_Pwm_*`. `SetPWMBacklightLv` clamps with the immediate 1000 and reads `M`
from `0x019354c8`. `DEV_PANEL_SetPWMBacklightLimit` @ `0x008581dc` later loads
the halfword Init stored at `0x019354c6` and passes that value to
`DEV_PANEL_SetBacklightLevel`. Init does not call Limit.

`mod_panel` (`panel.curve`, default off) owns the curve math and a percent
duty path. It does not copy GPIO 110/111, write the timing block, or run
panel power. Visible backlight acceptance is still the limit in
`PANEL-BACKLIGHT-2026-11-07` and the panel-backlight memory note: a PWM HAL
success is not panel-on.

## DEV_NIM_Init

Entry `0x00854fd0`. Byte `0x019353fc` nonzero: skip the body and return 1.
Otherwise `DEV_NIM_Dbg_Init`, store 1 at that byte, call `_initNimDevice`,
return 1. The factory result is ignored, so the parent failure log cannot
fire.

`DEV_NIM_Dbg_Init` @ `0x00856ef4` is itself guarded by byte `0x0193540c`. The
first call registers a debug handler and calls `dev_tuner_SI2151_dbg_menu` and
`dev_tuner_MxL661_dbg_menu`. Those menu bodies were not walked.

`_initNimDevice` @ `0x00854f14` stores the word 2 at `0x01935400`, then
`_DEV_NIM_FACT_Create` twice. The name strings are `"DEV NIM ATV"`
(`0x01359278`) and `"DEV NIM DTV"` (`0x01359284`). The factory @ `0x00854eac`
`strcmp`s those two strings and calls `DEV_NIM_ATV_Create` @ `0x008683d4` or
`DEV_NIM_DTV_Create` @ `0x00864494`. Any other name returns 0. Create's return
only selects a `puts` string (`"Device NIM ATV create complete"` at
`0x01359290` and the two failure strings next to it).

`DEV_NIM_DTV_Create` starts by rejecting a null object, `malloc(92)`, and
filling that object from the `"DEV NIM DTV"` bytes plus function pointers in
the `0x0086....` range. It does not open a device node in the prologue that
was read. The rest of the constructor, and all of `DEV_NIM_ATV_Create`, were
not walked. No tuner/demod ioctl is in `DEV_NIM_Init` itself.

`include/openaurora/nim.h` documents an offline stub. `mod_tuner` does not
call `DEV_NIM_Init` or the two factories. The ATV/DTV objects this stage
creates are missing from a cold start.

## DEV_AMP_Init

Entry `0x0084f4ac`. No guard.

1. `PLAT_SP_DebugRegisterMenu(3, 0x100000, "DEV_AMP", 0x0175ee00)`.
2. `memset` 56 stack bytes, store `0x100` in word 0, `Hal_Audio_Ctrl(0x3a,
   &block)` via PLT `0x006b425c`. The HAL return is ignored. The format at
   `0x01356474` is `" %s --- HAL_AUDIO_SET_IISMCLK\n"`. This is the request
   in `artifacts/reports/AUDIO-I2S-MCLK-2026-11-06.md`.
3. `TD_OS_TaskCreate(0x0084ebf1, 0, 0x1000, 3, "DEV_AMP_ID")`. The name is
   `0x01356494`. A zero task id is logged with `PLAT_SP_DebugWrite` and
   returned. Nonzero returns 1.

The task body at `0x0084ebf0` (objdump's enclosing symbol is the wrong
`C_SalTunerAV2018` span) is the hardware-facing callee. It calls
`Profile_GetMainSpeakerPower` and `Profile_GetDRC`, compares the power string
with the literals at `0x01356abc` and `0x01356ac4`, then later
`DEV_AMP_FACT_Create`, `DEV_AMP_FACT_GetDefaultResource`, and two
`DEV_GPIO_GetDescription` calls. Those GPIO ids are words from the resource
record (`+4` and `+8`), not immediates. The resource table base used by
`GetDefaultResource` @ `0x00874b10` is `0x01763ef0`, stride 36, indexed by
the id `Create` stored; the row for this set was not recovered. The task also
`TD_OS_TaskDelay`s, creates a nested task, and indirect-calls through saved
function pointers. It does not itself call `Hal_Audio_Ctrl` or an amp ioctl.

The AD82587 register sequencer, GPIO114/208 and the post-init mute are owned
by `AMP-INIT`, `AMP-MODULE-2026-11-07`, `AUDIO-ROUTING-2026-11-01` and
`openaurora/modules/audio_amp`. Those reports are the board-amp contract.
`DEV_AMP_Init` is the earlier stage: one ignored I2S-MCLK HAL call plus this
profile/factory task. `mod_audio`'s `audio.i2s_mclk` defaults to 0 and, when
set, sends only the `0x3a`/`0x100` request. It does not create `DEV_AMP_ID`.

Re-entry creates another debug menu, sends `0x3a` again, and creates another
task. There is no initialized byte.

## DEV_SPI_Init

Entry `0x0085946c`. No device node.

`UTILI_DBG_ChkPrtLevel(2, 1, 8)` may print, then `_create_semaphore`
@ `0x008593b0`. That helper always calls `_destory_semaphore` and then
`TD_OS_SemaphoreCreate`, and stores the result at `0x019354f0`. It is not
idempotent: a second `DEV_SPI_Init` destroys and replaces the semaphore.
Init returns 0 when that word is 0. Otherwise it waits, calls
`TD_DBG_Register(3, 0x100000, "DEV_SPI", 0x01761998)`, signals, and returns 1
(`movne r0, #1` when the second level check is not 1; the debug path returns
the level-check value, which is 1).

## DEV_SPI_Open

Entry `0x00858d0c`. The caller's `r0` is unused.

If word `0x019354f0` is 0, return 0 without touching a file. Otherwise wait
on that semaphore.

`__xstat64(3, "/conf/spi.bin", &stack)`. The path is `0x0135b9ec`.

- Stat not `-1`: if `FILE*` at `0x019354f4` is already set, `fclose` it.
  `fopen64("/conf/spi.bin", "rb+")`. The mode is `0x0135ba48` and is the only
  mode in this function. A null `FILE*` sets the return to 0, signals, and
  returns. There is no second open with `"rb"`. A successful open
  `fseek(SEEK_END)`, `ftell`, `fseek(0)`. A failed seek still returns 1
  after an optional printf. The `FILE*` stays at `0x019354f4`.
- Stat fails with `errno == 2`: if byte `0x019354ec` is already nonzero,
  return 1 without opening. If it is 0, `open64("/dev/ntkspi", 2)` because
  the flag register is the errno value. The path is `0x0135bf70`. The fd is
  stored at `0x017618b4` even when it is negative. `fd >= 0` stores 1 at
  `0x019354ec` and returns 1. `fd < 0` returns 0.
- Stat fails with any other errno: fall through into the `fopen64("rb+")`
  path.

`DEV_SPI_Open` does not ioctl. The later read helper uses
`ioctl(fd at 0x017618b4, 0x8018531e, ...)` and is not on the init path.
`SPI-MTD-NIM-STAGE-2026-11-06` owns the read request layout. Its "falls back
to `rb`" clause is not this function.

`mod_spi` (`spi.enable` default `"0"`, `spi.file` `/conf/spi.bin`,
`spi.device` `/dev/ntkspi`) is the Open/read owner and is not autoloaded.
Cold start without stock Aurora still has to perform this open. Doing it in
a second process while stock holds `/dev/ntkspi` is a second fd; the
opened-byte is Aurora BSS, not a kernel lease.

## DEV_NIM_DTV_Init

Entry `0x00861678`. Byte `0x01936fb0 == 1`: optional debug write, return 1,
no second task. Otherwise the byte is still clear until the task stores it,
so a second call before the task runs inits the mutex again and creates
another task.

`DEV_COMMON_GetModelType` then a TBB at `0x008616c4`. Table bytes at
`0x008616c8` are `03 03 03 03 46`. Models 0..3 branch to `0x008616ce` and
store 3 at `0x0176201c`. Model 4 branches to `0x00861754` and stores 1 there.
`model > 4` stores 3. Every model value then continues. The header comment
that Init "accepts 0..3" is a project filter; the vendor function does not
reject model 4 or the default.

If byte `0x01937000` is 0, `pthread_mutex_init` on `0x01936fe8`. A nonzero
debug byte only adds `PLAT_SP_DebugWrite` and still inits the mutex.
`PLAT_OS_CreateTask(0x008601ad, 0, 0x1000, 14, &stack, 1)`. The stack image
is copied from `0x0135f29c` (`"DTV_INIT_TASK"`). The create return is
ignored. Init then returns 1.

The task at `0x008601ac` is one-shot. It locks `0x01936fe8`, calls
`PLAT_DRV_GetModuleBufInfo(9, 0x019370ac, 0x019370b0)`, then
`_dev_nim_dtv_InitNimFunc(0x01937004)`. `GetModuleBufInfo` @ `0x0089b79c` is
`movs r0, #0` and return, so the `cmp #1` success-log branch at `0x00860324`
is dead in this binary. Both the live path and that dead path still reach
`InitNimFunc`. Its return is ignored.

Word `0x0176201c` shifted left 30: the N flag is bit 1. Value 3 (models 0..3
and the default) takes `HAL_Demod_User_Initialize` @ `0x0086ade4` and
`_dev_nim_dtv_Connect2HAL` @ `0x0085fea8`. Value 1 (model 4) skips both.
`tuner.c` passes `hal_requested = 1` into `oa_nim_dtv_task_run`, so a
configured model 4 would take HAL where this task does not. The default
`tuner.nim_model` 0 matches value 3.

The IR helpers are unnamed in the symbol table (objdump places them inside
`DEV_UART_IR_SetSystemCodeAndActive`). `0x0085fd04` calls `DEV_IIC_Read`;
`0x0085fc80` calls `DEV_IIC_Write`. Both are called as
`(0, 0, 0x80, 1, code, 1, &byte)`. Three stack bytes are zeroed once.
The chain, each step aborted to the common finish when the helper returns 0:

| step | helper | code | byte update before the write |
|---|---|---|---|
| read | `0x0085fd04` | 21 | |
| write | `0x0085fc80` | 21 | `byte &= ~8` |
| read | `0x0085fd04` | 17 | |
| write | `0x0085fc80` | 17 | `byte \|= 8` |
| read | `0x0085fd04` | 64 | |
| write | `0x0085fc80` | 64 | `byte \|= 2`; this write's return is not tested |

`DEV_IIC_Read` @ `0x0085358c` returns 1 when `Hal_Iic_Read` returns 0, and
returns 0 after a printf when `Hal_Iic_Read` is nonzero. `DEV_IIC_Write`
@ `0x008537ac` is the same shape around `Hal_Iic_Write`. Each helper returns
1 when that `DEV_IIC_*` return is nonzero, and 0 when it is 0. The task
continues only on helper nonzero. Net for one step: continue when the HAL
returns 0. `oa_nim_dtv_iic_read` / `iic_write` return 1 when their callback
returns 0, which matches this HAL polarity. The comment in
`include/openaurora/nim_dtv.h` that the helpers return 1 only when the IIC
call returns 0 does not match the helper-versus-`DEV_IIC_*` instructions.

The common finish at `0x00860252` runs on helper 0 and after the last write:
store `0xff` at `0x01762020`, store 1 at `0x01936fb0`,
`pthread_cond_signal(0x01936fb8)`, unlock `0x01936fe8`, return. That is the
byte `DEV_NIM_DTV_WaitInitComplete` @ `0x008615fc` waits on. Init does not
wait. `mod_tuner`'s hardware bring-up runs `oa_nim_dtv_task_run` inline and
then ignores IR failure, which matches the finish path, and it does not
create the ATV/DTV factories from `DEV_NIM_Init`.

`HAL_Demod_User_Initialize` and `_dev_nim_dtv_InitNimFunc` /
`Connect2HAL` were not walked past the call sites. Demod and tuner device
nodes inside those callees are not recovered here. Loaded kernel modules
`ntkndptuner` and `ntkndpdemod` are live observations only.

## DEV_MTD_Init

Entry `0x0085419c`. The whole body is
`PLAT_SP_DebugRegisterMenu(3, 0x100000, "DEV_MTD", 0x01760b9c)` and
`movs r0, #0`. No open, no ioctl, no device node. Re-entry only registers
the menu again and returns 0 again.

`DEV_MTD_EraseFlash` @ `0x008541e8` and `DEV_MTD_ShowInfo` @ `0x008542cc`
are separate functions. Erase uses request `0x40084d02`. ShowInfo uses
`0x80204d01`, `0x80044d07` and `0xc0104d08`. They are not called from Init.
No cold-start owner is required for `DEV_MTD_Init`. Opening `/dev/mtd*`
during cold start is a different, mutating contract and was not part of
this stage.

## TV read-only observations

2026-11-08 22:18 CET, host already awake, `tools/tvssh.sh` to
`192.168.178.87`. Commands were `tr` of `/proc/PID/cmdline`, `ls -l` of
`/proc/1770/fd` and `/proc/1567/fd` (symlink text only), `ls`/`cat` of
`/proc/fb`, `/sys/class/graphics/fb*/name`, `/proc/modules`, `/proc/devices`,
and `ls` of `/dev/fb*`, `/dev/dri`, `/dev/snd`, `/dev/ntk*`, `/dev/mtd*`,
`/tmp/ntkdriver_completed`, `/conf/spi.bin`. No process was stopped, no fd
target was read, no module was loaded, nothing was written. A successful
listing is presence, not a hardware effect.

Stock processes match the stated pair: pid 1567
`/vendor/com.nvt.appmgr/com.nvt.appmgr`, pid 1770
`/vendor/com.nvt.idtv/aurora.elf`, plus pid 1693
`/bin/sh /mtd_apexe/board/run_aurora.sh`. `/tmp/ntkdriver_completed` exists,
mode `0666`, size 2. Its bytes were not read.

`/proc/fb` and sysfs names: fb0 `OSD_NORMAL1`, fb1 `OSD_NORMAL2`,
fb2 `OSD_NORMAL3`, fb3 `OSD_CURSOR1`, fb4 `OSD_CURSOR2`. Nodes `/dev/fb0`
through `/dev/fb4` (major 29) and `/dev/dri/card0`, `card1`, `controlD64`,
`controlD65` exist. Aurora 1770 holds fd symlinks to fb0, fb1, fb2, fb3,
fb4, `/dev/dri/card0` and `/dev/ntkgpu`. Appmgr 1567 holds fb0, fb1, fb2,
`card0` and `/dev/ntkgpu`.

`/dev/ntkspi` exists (245,0). `/conf/spi.bin` does not. Aurora fd 27 is
`/dev/ntkspi`. That is the Open path this function takes when the file is
absent; the listing does not prove the flags or that this fd was the one
stored at `0x017618b4`.

`/dev/ntkled` (227,0) exists and is Aurora fd 25. Which panel function opened
it was not recovered. Aurora also holds `/dev/ntgpio` (fds 16 and 24),
`/dev/i2c-0` through `/dev/i2c-5`, `/dev/ntkiic0` (fds 23 and 26),
`/dev/snd/controlC0`, `controlC2`, `controlC3`, `/dev/ntkvplt`,
`/dev/ntkdvif`, `/dev/ntksys`. `/dev/mtd0` and later mtd nodes exist. Aurora's
fd listing, filtered to the names above, did not show an `/dev/mtd*` symlink.

Loaded modules include `ntkdriver`, `ntkndpalsa`, `ntkndptuner`,
`ntkndpdemod`, `ntkndpextin`, `ntkndpdrm`, `ntkndphdmi`, `mali`, `PreInit`.
`/proc/devices` lists `fb`, `mtd`, `alsa`, `i2c`, `ntkiic0`, `ntkled`,
`drm`, `ntgpio`.

## Not recovered

- The device node for the GPIO-description fd at `0x0175f0a4`, and the body
  of `DEV_GPIO_GetDescription` when the table pointer is null.
- Which row of the amp resource table at `0x01763ef0` this set uses, and
  therefore the two GPIO ids the amp task passes to `GetDescription`.
- The bodies of `DEV_AMP_FACT_Create` after the call, `HAL_Demod_User_Initialize`,
  `_dev_nim_dtv_InitNimFunc`, `_dev_nim_dtv_Connect2HAL`, `DEV_NIM_ATV_Create`,
  and `DEV_NIM_DTV_Create` past the 92-byte fill.
- `Hal_Audio_Ctrl` / `Hal_Iic_Read` / `Hal_Iic_Write` inside libNDAL3. The
  return polarity above is the Aurora wrapper, not the library's effect on
  the wire.
- Whether Aurora fd 27 was opened by `DEV_SPI_Open` or by another caller,
  and the open flags of that live fd.
- The two bytes in `/tmp/ntkdriver_completed`.
- The caller of `DEV_INI_Init` (the other half). This file does not say
  whether `ICE_HardwareInit` aborts on the inevitable 0 return.
- A full walk of `DEV_INI_BY_PCB_Layout_Setting_Init` and `DEV_AUD_INI_Init`.

## Recommended cold-start ownership

Do not hang a cold start on `DEV_INI_Init`'s return. It is the low byte of
`DEV_MTD_Init`, and that function returns 0 after a debug-menu registration.
Treat each stage's own return as the gate, and treat MTD init as done when
the menu call has been issued.

One owner per stage, in this order, after the GPIO and IIC stages the other
half owns:

| stage | owner now | still missing for a stock-free cold start |
|---|---|---|
| panel | `mod_panel` curve math, opt-in | GPIO 110/111 copies, timing block at `0x019354b4`, PWM channel 108, then a separate power/backlight owner. Init does not light the panel |
| NIM factories | none | `DEV_NIM_Init`: SI2151/MxL661 debug menus and `DEV NIM ATV` / `DEV NIM DTV` objects. `mod_tuner` does not do this |
| amp | `mod_audio` opt-in `0x3a`/`0x100`; `audio_amp` for the later AD82587 sequencer | the `0x0084ebf0` task: speaker-power/DRC profile, factory, resource GPIO descriptions. Default `audio.i2s_mclk` is off |
| SPI | `mod_spi`, `spi.enable` default 0 | must be enabled. On this set the file is absent, so the contract is `open("/dev/ntkspi", 2)`. Do not open it while pid 1770 still holds fd 27 |
| NIM DTV | `oa_nim_dtv_task_run` inside `mod_tuner` | run it only after IIC and the NIM factories. Model 0..3 and the default take the demod HAL path; model 4 does not. Wait for byte `0x01936fb0` if the task stays asynchronous. IR failure still marks init done |
| MTD | none required | leave flash ioctls off this path |

Framebuffer and DRM nodes already exist under stock (`fb0`..`fb4`,
`/dev/dri/card0`). They are held by Aurora and appmgr. Creating a second
scanout owner is outside these seven stages.

## Cross-check by the orchestration worker (2026-11-08)

- `ICE_HardwareInit` `0x007551a0` calls `DEV_INI_Init` at `0x00755650` and
  does **not** check r0: `0x00755654` immediately starts the next mutex
  creation. The always-0 return is ignored by the caller too.
- `DEV_GPIO_Init`/`DEV_IIC_Init` are confirmed as separate device-layer
  inits; `Hal_Gpio_Init` (`0x4e7be3ec`) and `Hal_Iic_Init` (`0x4e7bee98`) in
  libNDAL3 are different functions. See PLATFORM-PARITY-GAP.
- The NIM-DTV task at `0x008601ac` was re-read: read/write codes 21, 17, 64
  and the bit-8/bit-2 updates match this report.

