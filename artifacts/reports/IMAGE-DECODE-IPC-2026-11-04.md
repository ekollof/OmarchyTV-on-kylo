# Real socket/CLI image.decode lifecycle — 2026-11-04

Offline host test only; no display, HAL or TV module is loaded.

## What was added

- `core/control.c` now exposes `image-decode <path>` through the real control
  plane. It looks up `image.decode` v2, tries PNG then JPEG, prints
  `image <w>x<h> fnv <64-bit>` on success, and returns `ENODEV`, `EAGAIN` or
  `EINVAL` as appropriate.
- `tests/image_decode_ipc.cjs` starts `openaurora` with an empty autoload
  config and a real Unix socket, then drives load/start/use/stop/start/reload/
  unload/load through that socket and through stdin.

## Verified sequence

```
image-decode (before load)            failed errno 19
load image_decode                     ok image_decode
start image_decode                    ok
image-decode thumb61                  image 400x225 fnv 12815d59be310b91
image-decode DMR-small.jpg            image 48x48 fnv 954e7afda49f66c6
image-decode non-image                failed errno 22
image-decode (missing/extra arg)      failed errno 22
status via stdin                      name image_decode, state started
image-decode via stdin                image 400x225 fnv 12815d59be310b91
suspend                               ok
image-decode                          failed errno 11 (EAGAIN)
resume                                ok
image-decode                          expected PNG hash
stop image_decode                     ok
image-decode                          failed errno 19 (ENODEV)
start image_decode                    ok
image-decode                          expected PNG hash
reload image_decode                   ok
image-decode DMR-small.jpg            expected JPEG hash
stop + unload image_decode            ok
image-decode                          failed errno 19
load + start image_decode             ok
image-decode                          expected PNG hash
```

The host writes no persistent files, exits cleanly on SIGTERM, and removes the
socket. The FNV values are the same decoded bytes used by
`check-image-decode`; the socket path is the real `--sock` control plane, and
the status/hash paths are also exercised through stdin.

## Commands

```sh
make -C openaurora check-image-decode-ipc
make -C openaurora check
```

Evidence: `artifacts/analysis/image-decode-ipc-2026-11-04/check.txt`,
`full-host-check.txt`, `identities.txt`.

Full host suite: **395 ok**; `verify.cjs`: **45 groups** pass. This is host
module lifecycle evidence, not a display/TV or independent-startup claim.
