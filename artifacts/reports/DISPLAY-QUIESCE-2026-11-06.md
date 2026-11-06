# display async quiesce — 2026-11-06

TV was off limits. Offline host test with the real `mod_display_fb` over an
`LD_PRELOAD` fake `/dev/fb1`, the real control socket and the existing
snapshot/restore checks. No real framebuffer, HAL or TV was touched.

## Existing gate and this step

`mod_display` already had a worker that restores the layer snapshot and closes
the mapping/fd. This step adds the diagnostic delay and an explicit control
proof:

- `display.cleanup_delay_ms` holds the restore/close worker open;
- a new `display-info` command queries the display service;
- the fake-fb socket test now asserts the first `reload display_fb` fails fast,
  `status` stays responsive, a retry completes and `display-info` reports the
  layer again before the existing suspend/resume/stop/reuse checks run.

## Behavior

`m_quiesce` sets `closing=1`; every draw/present/info entry point checks it and
returns -1. The worker restores the snapshot and closes the layer. The current
try-join result and cleanup error are retained for a throttled retry (250 ms);
`m_stop` unpublishes the display services only after the worker has joined. This
is the contract the existing `display_present`/`display_binding` unit fixtures
assert (drawing after quiesce returns -1 without unpublishing mid-worker).

## Verification

```text
node tests/fake_fb_socket.cjs openaurora/build
ok actual fake-fbdev display_fb IPC: async gate, start/suspend/resume/reload/stop/reuse, full snapshot restoration
```

The new gate section:

```text
reload display_fb -> failed (<1.5 s)
status display_fb -> responsive (<1 s)
retry reload -> ok
display-info -> display 1920x1080 32bpp 2 frames
```

Then the existing checks still prove suspend/resume and stop/reload restore the
fake layer's yoffset, MAU, region and pixels exactly.

Full suite and QEMU:

```text
full_check=0
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
```

## Limits

- The fake fbdev is a memfd with the measured geometry, not the TV's kernel
  driver; a real driver close can still stall.
- GPU/Mali/scanout teardown, cold startup and stock-Aurora coexistence are not
  proven by this test.
- The worker retention/throttle behavior is unit-tested with the fixture; the
  diagnostic delay is not a production timing guarantee.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine` and `display` now have the
  async gate; later gates add source and ui_menu.
