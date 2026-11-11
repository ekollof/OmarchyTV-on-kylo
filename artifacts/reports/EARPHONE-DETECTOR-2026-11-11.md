# Earphone detector recovery — 2026-11-11

Status: current
Last verified: 2026-11-11
Evidence class: firmware instructions, live read-only descriptor query, host
tests. No audio effect and no GPIO read/write on hardware.
Supersedes: the "GPIO earphone handler still open" wording for the detector
half only; the event-17 audio routing remains open.

## Firmware contract

`DEV_GPIO_Init` creates the earphone task at `0x00851544` (stack `0x1000`,
priority 10). The task:

1. Resolves GPIO **117** with `DEV_GPIO_GetDescription(0x75)`
   (`gpio-function-names.tsv` id 117 = `EARPHONE`); a missing descriptor
   follows the "Init Ear Phone Failed" path.
2. Waits on the HP IPC semaphore/state; a set stop/disable byte parks the loop.
3. Calls `Profile_BoardType()`. With type 17 the **normal** descriptor level
   means plugin and **active** means plugout; every other board uses the
   **active** level for plugin and **normal** for plugout.
4. Waits 100 ms, then `DEV_GPIO_GetLevel(descriptor->pin, &level)`.
5. A read failure increments a counter and logs only the first failure; the
   loop continues.
6. On a level change, sends `PLAT_OS_SendEvent(0, 17, plugin, 0, 0)`:
   `1` = `EN_PLAT_OS_EV_TYPE_EARPHONE_PLUGIN`, `0` =
   `EN_PLAT_OS_EV_TYPE_EARPHONE_PLUGOUT`, and records the last level.

The event names come from the shipping strings. The event-17 consumer is the
`ICE` headphone/lineout path (`ICE_SoundControlIsHeadphoneConnected`,
`Set/GetLineoutFromHeadphone`, headphone volume/balance); its complete
handler/state machine is **not** reproduced here.

## Live descriptor (read-only)

`tools/tv-gpio-describe.c` (static ARM, sha256 `49a9a3c1…`) issued only
`DEV_GPIO_GetDescription` and was removed after execution:

```text
id=117 name=EARPHONE rc=0 pin=117 mode='input'
       normal='high' active='low' idle='default' int_type='no_int'
```

So the set's jack input is pin 117, input, normal high, active low. With the
stock PCB layout 14 (not board type 17), plugin = active = low and plugout =
normal = high.

## Implementation

- `openaurora/include/openaurora/earphone.h` — injected descriptor/get/event/
  delay/log ops and the detector state.
- `openaurora/common/earphone.c` — GPIO 117 init, board-17 mapping, 100 ms
  poll, first-failure logging, level-change event.
- `openaurora/tests/earphone.c` and `check-earphone` — plugin/plugout,
  repeated-level suppression, board-17 swap, read-failure log/recovery, an
  "other" level, initial-level seeding and missing/invalid inputs.
- `common/dev_gpio.c` now resolves id 117 `EARPHONE`; `check-dev-gpio`
  asserts the name.

`oa_earphone_init(..., initial_level)` lets the caller seed the last level
because the vendor's shared-memory initial value is not fully recovered;
`-1` emits the first observed state.

## Verification

```text
ok earphone: GPIO117 descriptor, 100 ms debounce, board17 swap,
   plugin/plugout event, read-failure log/recovery, other-level and
   initial-level handling
```

The full host suite and docs checks are run with the same change. No TV GPIO
read, export, claim, write or audio operation was performed; the TV stayed in
stock operation.

## Limits and next step

- **Detector only.** Event 17 is delivered to a callback; no speaker mute,
  headphone routing, lineout switch, volume/balance or UI indication is
  implemented.
- The production detector is not wired to a module or timer yet. GPIO 117 is
  below 200, so `Hal_Gpio_Read` would export/claim the pin; a future owner
  must serialize it under the GPIO/audio owner, not the host loop.
- The vendor HP IPC shared-memory semantics and the exact event-17 handler
  remain open. Do not infer the audio effect from `SetLineoutFromHeadphone`
  alone; recover the handler first.
- A live jack insertion test would require the audio/GPIO owner and was not
  performed.
