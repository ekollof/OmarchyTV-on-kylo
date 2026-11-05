# Graphics/scene_display socket lifecycle — 2026-11-05

Host-only test. No TV, HAL, DRM/KMS, EGL/Mali or real framebuffer is used.

## Coverage

`tests/graphics_socket.cjs` starts a real `openaurora --sock` host with
`graphics.broker=build/oa-scene-broker` and
`graphics.renderer=build/tests/scene-renderer-fixture.so`, then drives the real
`mod_graphics` and `mod_scene_display` over the Unix socket:

1. load/start `graphics`, verify `display.gpu:v1` is published;
2. load/start `scene_display`, verify `display.gpu` is required and
   `display:v2`, `display.alpha:v1` and `display.async:v1` are published;
3. reload `scene_display` and verify its services return;
4. attempt `reload graphics` while `scene_display` holds `display.gpu`; it must
   return `failed` and graphics must stay started;
5. stop `scene_display`, capture the live broker child PID, then reload
   `graphics`; the old broker must be reaped and a replacement child spawned;
6. start `scene_display` again, then stop it and `graphics`; the broker child
   must be gone;
7. unload both, then repeat load/start/stop with a fresh instance;
8. SIGTERM exits 0 and removes the socket.

The fixture renderer validates the broker handshake and fake render path; it
does not create a GPU or framebuffer. The test checks the host's direct
children through `/proc/<pid>/task/<pid>/children`, so child reaping is
observed, not inferred from a return code.

```
ok actual graphics/scene_display IPC: gpu/display services, hold refusal, dependency-ordered reload, child reap and reuse
```

## Evidence

- `artifacts/analysis/graphics-socket-2026-11-05/check.txt`
- `full-host-check.txt` — full strict host `make check`: **402 ok**
- `identities.txt` — host, graphics, scene_display, broker and fixture renderer
- `verify.cjs`: **45 groups**

## Limits

This is a fake-renderer host lifecycle. It does not prove real EGL/Mali
behavior, physical fb1 scanout, display ownership, region/MAU effects, vendor
Aurora coexistence, hardware-module reload or cold startup. It also does not
cover a real GPU fault or device loss.
