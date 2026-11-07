# CEC reply queue — 2026-11-07

Scope: close the MEDIUM review finding in `modules/cec/cec.c`. Host regression
test and ARM module build; no TV access or bus traffic.

## Defect

`cec_poll()` processed the policy reply queue without checking `session_open`,
and on `oa_cec_owner_send_async` failure it did:

```c
c->reply_head = c->reply_tail; /* drop on submission failure */
```

One failed reply therefore discarded every later reply, and a queued reply
could still be submitted after the raw session had closed. A successful
release already cleared the queue (added with the suspend/release fix).

## Fix

- The reply path runs only when `session_open` is true:
  no send/receive is submitted after close.
- A failed submission drops only the failed record:
  `reply_head = (reply_head + 1) & 3`. Later replies keep their order and are
  attempted on subsequent polls.
- A successful `OA_CEC_ASYNC_RELEASE` clears `reply_head/reply_tail`; a failed
  release keeps the session and queue (the raw session is still open).
- Queue saturation still drops newly offered replies and logs, preserving the
  existing four-slot policy.

## Regression test

`check-cec-reply-queue` (`tests/cec_reply_queue.c`) includes the module source
and wraps owner send/take/receive, so no worker or device is needed:

1. Queue two replies, fail the first submission: only record 1 is dropped,
   record 2 is still attempted and completes (`tx_frames == 1`).
2. Queue a reply while `session_open == 0`: neither send nor receive is
   submitted and the queue is unchanged.
3. Successful release clears the queue; failed release keeps the session open
   and the queue intact.

Negative check: restoring `reply_head = reply_tail` fails at
`reply_head == 1 && reply_tail == 2`.

Existing CEC authority, suspend/resume, transport, owner, quiesce and device
suites pass.

## ARM

```text
5083aee9aa75f87b12f4b32a3139a7c60cb29c65591f3531ba93b2ac06ffa185  build-tv-v3/modules/mod_cec.so
```

## Limits

- No CEC frame TX/RX hardware run: raw open is still blocked by the stock
  kernel owner after stock starts (CEC-RAW-BACKEND).
- This verifies queue state and submission ordering only; it does not claim a
  bus transmission.
