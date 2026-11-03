# Owned setting-list model and draw providers — 2026-11-03

`common/settings_model.c` supplies real owned item/text/value/image providers
for the SETTINGS-DRAW dispatcher. The public opaque model copies decoded caller
inputs, runs the existing dispatcher and synchronously delivers row events with
real text and image handles. It has no singleton or hardware dependency. It is
not yet a settings database/resource service consumer, reloadable module or
lower widget renderer.

## Firmware evidence and provider design

Aurora MD5 `64c252340d3af6c5d66072611b7bad7a`. Read-only decompile and ARM
instructions in `analysis/settings-model-2026-11-03/` establish:

- c8865c, operator<<(UString&,int): sprintf to a16-byte stack buffer with
  the literal pointer at c886a0, then UString::append. format-source verifies
  **%d** at14c4439. The provider formats the raw signed32 operand using
  PRId32, including -2147483648 and2147483647, with bounded snprintf.
- c86c60, UString::empty: the first length word is tested for zero with
  clz/shift; the decompiler's void result is incorrect. c88274 operator==
  calls compare at c87994, then tests its zero result with clz/shift.
- c867f4/c86b00 copy construction/assignment preserve string value through
  owned construction; our representation is independent of their layout.

The scoped input domain is valid UTF-8 C strings. Bytes are preserved, including
multibyte labels; empty comparison is a length-zero test. This is not a complete
UString implementation: embedded NUL, malformed UTF-8, character indexing,
substring and other string operations remain outside this provider's contract.
No saved Ghidra return signature was treated as ABI evidence.

Every model string, value array/text and opaque image payload is copied before
create returns; no caller buffer is retained. Image bytes are not interpreted,
and no vendor pointer is followed. The localized lookupString("OK") result is
a required caller input, copied too; the provider does not invent an English
fallback or query live resources. Backing type writeback changes the owned model
only; no DB write occurs.

Each temporary text allocates space for the largest model/default string plus
11 decimal bytes and a terminator. This makes assignment and the dispatcher's
single numeric append infallible after construction, matching the existing
callback contract without hidden realloc failures. A value snapshot has its own
array and deep-copied text objects, including an owned empty snapshot. Failures
unwind partial snapshots/model construction; errors leave the output model
unpublished. Event handles remain borrowed for the sink's call only.

Operations are serialized and synchronous. During drawing, nested draw and
destroy return EBUSY, so a sink cannot invalidate its active model. This is not
a thread-safety guarantee. The allocator pair is copied, but its context must
outlive the model. Overflow checks protect item counts, array sizes and text
capacity; constructor and draw allocation failures are observable errno returns.

## Verification and TV acceptance

**2048** real-provider dispatch calls cover32 types,16 styles and0..3 values.
Tests inspect actual UTF-8/text bytes, selection IDs, localized default text,
signed numeric endpoint formatting, deep-copied images/value text, and retained
progress type writeback. Source buffers are overwritten after construction.
Every construction and selected draw allocation is failed in turn; poisoned
memory/live-allocation accounting verifies cleanup, retry and model survival.
Sink reentry/destroy is rejected while drawing, and borrowed snapshots are not
leaked. The row sink copies event data during the call; it is a CPU recorder,
not a screen or widget implementation.

Strict full host suite and actual Unix IPC pass, as do focused ASan/UBSan,
fresh ARM/qemu and the same standalone CPU test on the real TV. Binary:
`build-tv-v3/settings-model/tests/settings_model`,77080 bytes,
MD5 `f72781ca27b1d9e525b2e76d676c7684`. readelf reports interpreter
`/lib/ld-linux-armhf.so.3` and only required GLIBC_2.4. This is per-artifact
linkage evidence, not a general compiler-libc compatibility claim.

The TV was already awake. tools/tvssh.sh verified /conf/coredumpnoreset and
free /tmp space, then staged only this binary under
`/tmp/oa-settings-model-20261103`. A separate successful connection verified
size/hash before execution. Test exit0 reported2048 passes. The exact executable
and directory were removed; a /proc/*/exe readlink scan found no helper, and
the no-reset flag remained present. No device APIs, screen/input/audio controls,
settings/network/services, boot hooks or persistent installation were changed.

This closes native CPU/provider acceptance for this model. It does not close
live item/resource acquisition, ui.list or skin integration, real row widget
construction/rendering, scrollbar internals, independent display/scanout/startup
or visible parity. Next connect those consumers to these owned providers instead
of reintroducing borrowed buffers or placeholder string/vector stubs.
