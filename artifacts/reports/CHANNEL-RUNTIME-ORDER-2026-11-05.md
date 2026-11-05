# Runtime channel-order provider and apps-grid capture gate — 2026-11-05

Offline/host work on the two remaining parity gaps. No TV state was changed;
the only TV access was a read-only fb1 capture that showed the no-signal
standby screen.

## Channel list: runtime-order ingestion

The stored channel screen previously had only one path: `service.list`
`channel_no`/name order with the "PROVISIONAL STORED ORDER - NOT RUNTIME EASI"
marker. The runtime EASI active list was unavailable because testtool TCP 1986
is disabled.

Added:

- `include/openaurora/channels.h` / `common/channels.c`: parser for the
  recovered `ACTIVECHANNELLIST` XML
  (`<active_list><service rsn='..' name='..' handle='..' onid='..'
  tsid='..' sid='..' type='..'/></active_list>`), preserving vendor row
  order, bounded to 512 entries and 96-byte names, with basic XML entity
  decoding and transactional replacement (a malformed list leaves the previous
  one intact).
- `mod_channels` publishes `channels`; start/reload clears the runtime list,
  stop/destroy clears it and unpublishes.
- Control commands `channel-ingest <hex>`, `channel-clear`,
  `channel-state`, `channel-list`.
- `ui_vendor` now prefers a non-empty runtime snapshot:
  - runtime marker: amber strip, `RUNTIME ACTIVE LIST - LCN/VISIBILITY
    UNAVAILABLE`, rows labeled by RSN in vendor order;
  - no runtime list: existing stored fallback and red provisional marker.

`ACTIVECHANNELLIST` still carries **no LCN and no visibility/HD-simulcast
flags**, so those remain unknown in both modes. This closes the structural
ingest path, not the live data gap.

### Verification

```
ok channels: runtime active-list XML order, fields and rejection
ok channels IPC: runtime XML order, replacement guards, clear and reload
ok channel list: stored fallback and runtime order markers/selection
```

The unit test covers entity decoding, attribute order, invalid/missing fields,
oversize names, 513-entry overflow, empty lists and failed replacement. The
socket test drives the real binary through stored mode, ingest, ordered rows,
malformed replacement, clear, reload and stop/unload. The UI recorder test now
checks both the stored red marker and the runtime amber marker plus selection.
Full host `make check`: **423 ok**; `verify.cjs`: **45 groups**. `mod_channels`,
`mod_ui_vendor` and the parser test build for ARM; the test passes under
`qemu-arm-static`.

## Apps-grid pixel parity: capture gate

The full-screen stock apps-grid oracle is still missing. Current facts:

- `artifacts/protocol/stock-apps-grid.png` does not exist;
- the TV is currently showing the stock no-signal standby frame, not the apps
  portal (read-only fb1 capture checked);
- this host has only UVC webcams, no HDMI capture device;
- the existing stock portal capture is a different screen (home portal card
  row), so it cannot certify the 5x3 full-screen grid.

The comparison harness and firmware-grounded region manifest are ready:
`openaurora/tools/capture_compare.py` and
`artifacts/analysis/stock-parity-2026-11-05/apps-grid-regions.json`, invoked by
`make -C openaurora compare-stock-apps-grid`.

### Owner-provided prerequisite

Pixel parity needs one of:

1. **Preferred:** leave the TV on the stock full-screen apps grid; then a
   read-only `tools/tvshot.py --out artifacts/protocol/stock-apps-grid.png`
   capture can be taken, followed by the comparison target; or
2. authorize sending the stock remote keys needed to navigate there (input,
   not view-only), then capture; or
3. provide a pixel-correct capture from an HDMI capture device.

No key was sent and no screen was changed. The no-signal frame is intact.

## Limits

- No live runtime EASI: the runtime list must be fed by `channel-ingest` or a
  future testtool client; testtool TCP 1986 remains disabled by owner decision.
- No runtime LCN/visibility; active-list order is the only recovered runtime
  ordering.
- No stock channel-list geometry/typography capture; the UI remains a
  reconstruction.
- No stock apps-grid capture, so pixel parity is still unmeasured. The
  manifest is firmware-grounded, not an observed layout.
- No vendor execution; host/recorder and ARM/qemu only except the read-only
  fb1 screenshot.
