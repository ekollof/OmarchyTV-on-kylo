# Fake-fbdev display_fb socket lifecycle — 2026-11-05

Host-only test. No TV, HAL, DRM/KMS device or real framebuffer is opened.

## Purpose

`tests/fake_fb_socket.cjs` loads the real `mod_display_fb.so` into a real
`openaurora --sock` host while `tests/fake_fb_preload.so` replaces `/dev/fb1`
and the recovered fbdev/region/MAU ioctls. This exercises the display module's
actual open/snapshot/present/restore/close code without physical hardware.

The fake layer starts with yoffset 0, MAU 1, eight region words `0x100..0x107`
and a 1920x1080/2160-line ARGB buffer filled with `0xaa`. It records:

- `OA_FAKE_FB_CHANGED` on region or pan writes;
- `OA_FAKE_FB_STATE` on `close()` and process exit.

Both records include initial/current yoffset, MAU and all eight region words;
the state record includes an FNV-1a hash of the whole mapped pixel buffer.

## Verified

1. `load display_fb` then `start display_fb` publishes `display:v2` and
   `display.alpha:v1`.
2. `display.test=1` paints a magenta band and calls `present()`. The changed
   record proves region words changed, yoffset changed and the pixel hash
   differs from the initial layer.
3. `suspend` performs the synchronous restore+close path. The recorded layer
   equals the initial yoffset, MAU, region words and pixel hash.
4. `resume` reopens the layer and republishes the services.
5. `stop`, `start`/`reload`/`stop`, and `unload`/`load`/`start`/`stop` all
   restore the initial yoffset, MAU, eight region words and pixel hash.
6. SIGTERM exits 0 and removes the control socket.

The test retries `stop`/`reload`/`unload` while the module's asynchronous
`cleanup_worker` owns the restore/close (`m_quiesce` returns `-EAGAIN` first),
matching the documented ABI 3 quiesce protocol.

```
ok actual fake-fbdev display_fb IPC: start/suspend/resume/reload/stop/reuse and full snapshot restoration
```

## Evidence

- `artifacts/analysis/fake-fb-socket-2026-11-05/check.txt`
- `full-host-check.txt` — full strict host `make check`: **401 ok**
- `identities.txt` — host, `mod_display_fb.so`, fake preload hashes
- `verify.cjs`: **45 groups**

## Limits

This is a user-space fake. It proves the module's snapshot/restore ordering and
the ioctl sequence it issues, but not real kernel fbdev/driver behavior, TDGA
region/MAU effect, physical scanout, vendor Aurora coexistence, live fb1
ownership, cold startup or stock parity. It also does not prove that every
possible ioctl failure path restores successfully.
