# mod_cec async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real `mod_cec`
and its idle owner worker. No CEC device, `/dev/ntkcec`, vendor HAL or TV was
touched.

## State held by mod_cec

`mod_cec` publishes `cec.owner` and `cec.audio`, owns the CEC policy and an
`oa_cec_owner` worker. The owner may hold an injected-I/O session; a failed
hardware close retains the owner/fd for a later quiesce generation.

The old quiesce hook only signalled the owner; both services stayed published,
`stop` joined on the loop, and a failed close left the owner without a
structured retry path.

## Gate

`quiesce` now:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. unpublishes `cec.owner` and `cec.audio` synchronously, so new lookups get
   `ENODEV`;
3. sets `quiescing`, detaches the owner and starts a cleanup worker;
4. the worker sleeps for `cec.cleanup_delay_ms`, calls
   `oa_cec_owner_quiesce`, polls it for up to 200 ms, joins and destroys when
   the worker has exited.

If the owner is still busy (open session or failed hardware close), the worker
returns `-EAGAIN` and keeps `cleanup_owner`; the next `quiesce` call starts a
new retry generation, matching the owner's documented contract. `stop` and
`destroy` join/free leftovers; `start` refuses with `-EBUSY` if an owner or
cleanup worker is live; `settled` is suppressed while quiescing. A retained
owner is never freed/unloaded underneath its worker: the normal stop path waits
for quiesce 0, and a force unload with a retained owner is outside the safe
contract. `suspend`/`resume` keep the synchronous idle-owner join/create path.

## Control probe

```text
cec-state     query cec owner/audio policy
```

It holds both services, reads `hardware_open_supported()` and the policy
route's `local_volume_permitted`, then releases.

## Verification

`check-cec-quiesce` with `cec.cleanup_delay_ms=400`:

```text
load/start cec
cec-state -> cec owner 1 audio 1 supported 0 local 1
reload cec -> failed (fast; 1 ms in the full run)
status cec -> responsive (< 1 s)
cec-state -> failed errno 19             (both services unpublished)
wait 600 ms
reload cec -> ok
cec-state -> cec owner 1 audio 1 supported 0 local 1
stop cec -> failed; cec-state -> failed errno 19
wait 600 ms; stop cec -> ok; unload cec -> ok; quit -> bye
```

```text
ok cec quiesce: reload fails fast (1ms), owner/audio republished, retry completes
```

Existing CEC tests still pass, including the retained-close owner contract:

```text
ok worker serializes authority, open and send; failed close stays owned until a later quiesce
ok control reload/suspend/resume joins the idle worker; a held service stays loaded
ok System Audio mutes local volume; ARC follows 1.4 direction; route survives reload
```

The ARM module links `libpthread`:

```text
build-tv-v3/modules/mod_cec.so
  NEEDED libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok cec quiesce: reload fails fast (1ms), owner/audio republished, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The module currently exposes no acquire/send control path, so the
  control-socket gate uses the idle owner. The failed-close retained-owner
  path is covered by `check-cec-owner` at the owner API, not by this module
  test.
- No real CEC device, `/dev/ntkcec`, physical-memory authority or bus traffic
  was exercised. `hardware_open_supported()` remains 0.
- A retained owner can keep cleanup pending; a force unload while it is
  retained is deliberately not made safe.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio` and now `cec` have the async gate; the remaining modules
  still stop synchronously.
- No TV or stock-Aurora coexistence test was performed.
