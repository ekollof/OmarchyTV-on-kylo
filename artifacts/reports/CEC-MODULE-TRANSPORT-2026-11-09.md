# CEC module transport seam and fake end-to-end test — 2026-11-09

Status: **module-level injectable transport and host fake TX/RX run. No TV,
no device, no bus traffic. `cec.enable=0` in every profile.**

## Why

The raw CEC device, authority page, owner worker, policy and reply queue were
already injectable/tested individually, but `mod_cec` hardcoded
`native_device_io` and the native `/dev/mem` authority map. Its enable=1 host
test could only prove fail-closed at the authority mapping; it could not drive
the real module open → receive → send → close path.

## Seam

New `include/openaurora/cec_module_transport.h`:

```c
typedef struct {
    const oa_cec_authority_mem_io *authority_mem;
    const oa_cec_device_io *device;
} oa_cec_module_transport;

const oa_cec_module_transport *oa_cec_module_transport_get(void);
void oa_cec_module_transport_set(const oa_cec_module_transport *transport);
```

`common/cec_module_transport.c` holds the override (NULL in production).
`mod_cec` reads it at start:

- `oa_cec_authority_mem_open(&c->authority, transport->authority_mem)` when a
  transport is installed, otherwise NULL selects the native `/dev/mem` path;
- `begin_acquire` uses `transport->device` when set, otherwise
  `native_device_io`.

A production build has no override, so the native path is unchanged.

## Fake transport

`tests/cec_module_fake.c` installs a fake from a constructor:

- authority-memory page with the mirror word at `+0x200` = `0x02` (bit 1 set),
  so the authority gate passes without an STBC write;
- raw device fd 7: `CEC_OPEN` (`0x401c4301`), one queued RX frame
  `50 7a a8` (5→0, opcode 0x7a, operand 0xa8), TX frame logging,
  `CEC_CLOSE` and close.

`tests/cec_module_fake.cjs` loads the real `mod_cec` with `cec.enable=1` and
`cec.device=/fixture/ntkcec`, then drives the control socket:

```text
load cec / start cec
wait auto-open  -> cec-state open 1
wait RX         -> cec rx 5->0 op 7a len 1 [a8 ]
cec-send 0404   -> fake cec tx len=2 04 04, cec-state tx 1
cec-close       -> cec-state open 0
quit
```

## Evidence

```text
$ make check-cec-module-fake
ok cec module fake transport: open, RX, policy log, TX bytes and close

$ make check-cec-packet check-cec-device check-cec-authority-mem \
       check-cec-transport-ipc check-cec-authority-lifetime \
       check-cec-reply-queue check-cec-owner check-cec-quiesce \
       check-cec-module-fake check-cec-audio check-cec-audio-policy \
       check-cec-suspend-resume
... all pass (rc 0)
```

The ARM `mod_cec.so` builds with the new common object and the 34-module cold
bundle manifest verifies.

## Limits

- The fake proves module acquire/send/receive/release ordering, frame logging
  and the gate; it is not hardware or bus-conformance evidence.
- The native authority write path remains fail-closed (`write_register`
  returns 0); a clear mirror bit still prevents the device open.
- `cec.enable=0` remains in every shipped profile. Enabling it still requires
  an authorized TV session, stock restoration and independent bus/audio
  acceptance.
- The fake queues one RX frame and does not exercise multi-frame queue races,
  the VPL filter defect or reply ordering beyond the existing unit tests.
