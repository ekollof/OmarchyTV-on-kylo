# Fake-fbdev display_fb socket evidence

Interpretation: [FAKE-FB-SOCKET](../../reports/FAKE-FB-SOCKET-2026-11-05.md).

- `check.txt`: real Unix-socket display_fb lifecycle through the fake fbdev.
- `full-host-check.txt`: full strict host `make check` (401 ok).
- `identities.txt`: host, mod_display_fb.so and fake preload hashes.

The fake `/dev/fb1` starts with known yoffset/MAU/eight region words and pixel
fill. The test proves the display self-test changed those values, then verifies
suspend, stop, reload and unload/load/start cycles restore the initial
yoffset/MAU/region/pixel hash. No real framebuffer, DRM/KMS, HAL or TV is used.
