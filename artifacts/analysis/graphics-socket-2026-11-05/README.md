# Graphics/scene_display socket evidence

Interpretation: [GRAPHICS-SOCKET](../../reports/GRAPHICS-SOCKET-2026-11-05.md).

- `check.txt`: real Unix-socket graphics/scene_display lifecycle.
- `full-host-check.txt`: full strict host `make check` (402 ok).
- `identities.txt`: host, mod_graphics.so, mod_scene_display.so,
  oa-scene-broker and fixture renderer hashes.

The test starts the real graphics module with the scene broker plus fixture
renderer, publishes and consumes display.gpu/display/display.alpha/
display.async, refuses a graphics reload while scene_display holds the GPU
service, performs a dependency-ordered reload, verifies the broker child is
replaced then reaped, and repeats load/start/stop with a fresh instance. No
real GPU, EGL/Mali, framebuffer or TV is used.
