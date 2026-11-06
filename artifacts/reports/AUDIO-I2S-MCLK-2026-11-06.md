# Amp-init I2S MCLK request — 2026-11-06

Offline implementation of the recovered `DEV_AMP_Init` clock request, with a
fake-HAL test. No vendor HAL, device, TV or audible path was exercised.

## Firmware evidence

`DEV_AMP_Init` @ `0x0084f4ac` (called from `DEV_INI_Init` after IIC/amp stage):

```text
84f4e4: movs r2,#56 ; memset a 56-byte parameter block
84f4e6: movs r1,#0
84f4e8: mov  r0,r7
84f4ea: blx  memset
84f4ee: mov.w r3,#256 ; 0x100
84f4f2: mov  r1,r7
84f4f4: movs r0,#58   ; 0x3a
84f4f6: str  r3,[r7,#0]
84f4f8: blx  Hal_Audio_Ctrl
```

It does not check the scalar return, then creates the amp monitor task
(`TD_OS_TaskCreate`, 4096-byte stack). `modules/audio/audio.c` already documented
opcode `0x3a` as “I2S MCLK, params[0]=0x100 (aurora: DEV_AMP_Init)” but never
sent it.

## Implementation

`mod_audio` start now optionally performs the same request before applying
volume/mute:

```c
if (audio.i2s_mclk == 1) {
    uint32_t params[OA_AUDIO_PARAM_WORDS] = {0};
    params[0] = 0x100;
    rc = Hal_Audio_Ctrl(0x3a, params);
    log "i2s mclk -> 0x100 (%d)"   /* failure logs, does not abort */
}
```

- `audio.i2s_mclk` defaults to `0`: this is transport/clock ownership work, not
  an accepted hardware or audible change while stock Aurora owns the audio path.
- The vendor ignores the return; the module logs a warning and continues.
- It is one request per start; no amp task, routing or mute/volume ordering
  change beyond running before the existing `0x33/0x35` calls.

## Verification

`tests/audio.c` adds a dedicated scenario with the fake HAL:

```text
ok   opt-in DEV_AMP_Init I2S MCLK request uses 0x3a/0x100 once, before volume
```

It asserts exactly one call, `last_op == 0x3a`, `params[0] == 0x100`, all other
parameter words zero, and that a following `set_volume(50)` still sends
`0x33/50`. Existing scenarios keep the default-off path and their request counts
unchanged.

Focused ASan+UBSan passes the audio test (`detect_leaks=1`,
`halt_on_error=1`). Full suite ran visibly and ended `all tests passed`; QEMU
sub-launcher value/live/launcher checks pass.

## Limits

- No vendor HAL, I2S clock, amplifier, ALSA or audible verification. The
  request is opt-in and not part of the default/cold config yet.
- The vendor amp monitor task, register initialization, mute/reset sequencing
  and standby lifecycle remain unimplemented; see AUDIO-AMP-PROFILE/AMP-INIT.
- Real audio hardware and broad parity remain unaccepted. Stable controlled
  display ownership still needs explicit owner authorization plus independent
  observation.
