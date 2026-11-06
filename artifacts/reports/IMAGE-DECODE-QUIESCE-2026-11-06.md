# mod_image_decode async quiesce — 2026-11-06

TV was off limits. Offline host control-socket test with the real
`mod_image_decode` and the local PNG corpus. No TV, display or vendor codec
was touched.

## State held by mod_image_decode

`image_decode` owns three `dlopen` handles: libpng (plus optionally a
separate libz opened with `RTLD_GLOBAL`) and TurboJPEG. It publishes
`image.decode` and uses the handles through opaque function pointers stored in
its context. The old stop unpublished and `dlclose`d all handles
synchronously.

## Gate

`m_quiesce`:

1. tryjoins a running cleanup worker (`-EAGAIN` while busy, 0 when done);
2. clears `published` and unpublishes `image.decode` synchronously, so the
   CLI gets `ENODEV` and any stale decode callback rejects before touching a
   library;
3. detaches the lib/zlib/TurboJPEG handles and clears the TurboJPEG function
   pointers;
4. `dlclose`s them synchronously at `image_decode.cleanup_delay_ms=0`,
   otherwise on a worker after the diagnostic delay and returns `-EAGAIN`.

The worker makes no host API calls. `m_stop` joins a leftover worker and
closes both current and detached handles; `m_destroy` does the same.
`m_start` refuses with `-EBUSY` if published or a worker is live, then reopens
the codec libraries.

## Verification

`check-image-decode-quiesce` starts with
`image_decode.cleanup_delay_ms=400`:

```text
image-decode PNG -> image 400x225 fnv 12815d59be310b91
reload image_decode -> failed (fast; 1 ms in the full run)
status image_decode -> responsive (< 1 s)
image-decode -> failed errno 19       (service unpublished)
wait 600 ms
reload image_decode -> ok
image-decode PNG -> image 400x225 fnv 12815d59be310b91  (codecs reopened)
stop image_decode -> failed; image-decode -> failed errno 19
wait 600 ms; stop -> ok; unload -> ok; quit -> bye
```

```text
ok image_decode quiesce: reload fails fast (1ms), codecs republished, retry completes
```

Existing decoder behavior still passes:

```text
  ok   image decode: PNG and JPEG RGBA match references, errors reject and service reloads
ok actual image.decode Unix IPC: load/start/CLI use/stop/start/reload/unload/reload lifecycle
```

The ARM module links `libpthread` and `libdl`:

```text
build-tv-v3/modules/mod_image_decode.so
  NEEDED libdl.so.2, libpthread.so.0, libc.so.6
```

Full suite and QEMU:

```text
full_check=0
ok image_decode quiesce: reload fails fast (1ms), codecs republished, retry completes
ok qemu sub-launcher value: ... source-screen HDMI1 writes 5 and resyncs (151/36)
ok qemu sub-launcher live: Dutch text within 4 px of p0/03
ok qemu sub-launcher: static AE=0
```

## Limits

- The host's libpng/TurboJPEG are loaded, not the TV's shipped codec
  libraries. `dlclose` timing and destructor behavior on the target remain
  unproven.
- The test proves the gate through unpublish/fail-fast/retry and a successful
  decode after reload; it cannot observe when the worker's `dlclose` returns.
- `decode_png`/`decode_jpeg` are synchronous loop-thread calls. Quiesce cannot
  run during an in-flight decode, but a very slow decode still blocks the loop;
  the gate does not move decoding off it.
- The PNG/JPEG decode contract itself is unchanged, including rejection of
  animated PNG and CMYK/YCCK JPEG.
- `ir`, `i18n`, `settings`, `ui_vendor`, `ui_engine`, `display`, `source`,
  `ui_menu`, `audio`, `cec`, `audio_mixer`, `tuner`, `epg`, `pvr`, `channels`,
  `service_db` and now `image_decode` have the async gate; the remaining
  modules still stop synchronously.
- No TV or stock-Aurora coexistence test was performed.
