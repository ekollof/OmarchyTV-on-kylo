# LauncherItem resource conversion — 2026-11-04

`common/launcher_item.c` implements the shipped
`LauncherItem::LauncherItem(NovoLauncherItem const&)` at `0xe4794c` (Thumb,
500 bytes, CRC `0xd1bcb8c5`) — the settingsDB resource conversion used by
`LauncherDialog::getLauncherItemsList` (`0xedb1a8`, calls at `0xedb2ac` and
`0xedb40a`). **8** native/C cases pass; the full `verify.cjs` now has **38** ok
lines. Host unit, focused ASan/UBSan, ARM/qemu, full `make check` and a focused
real-TV CPU run all pass.

## Where this sits in the resource path

The dumped profile has `/conf/sqlite_db/settingsDB.db` with a 26-column
`LauncherItem` table and 50 rows. The shipped path is:

1. `NovoLauncherDataProvider::getLauncherItems` (`0x107a0c5`) reads rows through
   the launcher database adapter into `std::vector<NovoLauncherItem>`.
2. `LauncherDialog::getLauncherItemsList` converts each `NovoLauncherItem` into
   the UI `LauncherItem` with the constructor implemented here.

This turn recovers step 2. The adapter/query (step 1) is still not implemented;
the conversion is now available for it.

## Recovered behavior

The body copies fields at fixed offsets, constructs 10 UStrings plus one
default UString, copies the shared_ptr pair, and initialises the remaining
fields:

| destination | source / value |
|---|---|
| `+0x00` | source `+0x00` |
| `+0x04` | source `+0x18` |
| `+0x08` | source `+0x0c` |
| `+0x0c` | source `+0x10` |
| `+0x10` | UString ← source `+0x1c` |
| `+0x18` | byte `1` |
| `+0x1c` | UString ← source `+0x24` |
| `+0x24` | UString ← source `+0x2c` |
| `+0x2c` | default UString (then the thumbnail path, below) |
| `+0x34` | UString ← source `+0x34` |
| `+0x3c` | UString ← source `+0x3c` |
| `+0x44`/`+0x48` | source `+0x44`/`+0x48` |
| `+0x4c` | UString ← source `+0x78` |
| `+0x54` | UString ← source `+0x80` |
| `+0x5c` | UString ← source `+0x88` |
| `+0x64` | source `+0x90` |
| `+0x68` | source `+0x08` |
| `+0x6c`/`+0x6d`/`+0x6e` | bytes source `+0x51`/`+0x50`/`+0x54` |
| `+0x70` | UString ← source `+0x58` |
| `+0x78` | UString ← source `+0x60` |
| `+0x80` | source `+0x14` |
| `+0x84`/`+0x88` | source `+0x68`/`+0x6c` |
| `+0x8c` | shared_ptr control block (source `+0x70`); refcount at control+4 incremented |
| `+0x90` | shared_ptr raw pointer (source `+0x74`) |
| `+0x94` | `-1` |
| `+0x98`/`+0x9c`/`+0xa0` | `0` |

Finally, when the thumbnail-name UString at source `+0x3c` is **non-empty**, the
body builds `"/buffer/downloaded_images"` + `"/"` + name at `+0x2c`. An empty
name leaves the default-constructed empty string. The branch direction matters:
the body does `cbz` on `isEmpty`, so it enters the path-building block when the
result is false (non-empty); an earlier reading of this slice had it inverted
and the differential caught it.

New API: `oa_launcher_item_from_novo(item,novo,ops)` with the UString
operations behind `struct oa_launcher_item_ops`; `OA_LAUNCHER_ITEM_BYTES` is
164. The only write outside the destination is the control-block refcount.

## Verification method

`tests/launcher_item_native.h` copies the body verbatim at `0xe4794c`, writes
the literal-pool entries just past the copied range (canary pointer, non-zero
gthread probe so the shipped atomic refcount branch runs, and the two path
strings redirected to owned copies), and stubs the UString
constructors/`isEmpty`/assign/append and the shared_count destructor. Both the
native trampolines and the owned ops call the same trace helpers.

Grid: source pattern {A,B} × thumbnail-name empty {0,1} × control block
{none,present} = 8 cases. Each compares:

- the ordered call trace (10 copies, default, isEmpty, optional
  assign/append-literal/append),
- the full 164-byte output plus untouched bytes to 256,
- the control-block refcount (7 → 8) and the null-control case (still 7).

The host unit adds a string-table UString emulation and asserts the exact
destination mapping, the built path `"/buffer/downloaded_images/thumb-file"`,
the empty-name case, the untouched byte past `+0xa3`, and the pointer fields.
The refcount assertion is 32-bit-only in the host unit because the object
stores the vendor's 32-bit control pointer; ARM/qemu and the TV run exercise it.

## Verification and limits

Isolated `build-tv-v3/settings-row-render`:

| executable | bytes | MD5 |
|---|---:|---|
| tests/launcher_item | 42596 | d5ec4b2a4bd6d58e7194787f2792191c |
| tests/tab_policy_firmware | 1331260 | c661b4c5d0167eec8658ce66bed80330 |

The focused real-TV CPU test ran the self-contained `launcher_item` ARM unit:
exit 0, staging hash-verified and removed, no helper left, no-reset flag
retained. The TV run executes the 32-bit refcount branch that the 64-bit host
unit skips. No device/screen/input/settings/network/service/boot change.

**Boundary:** the SQLite adapter/query and `NovoLauncherDataProvider` are not
implemented yet, so this is the conversion half of live resource loading, not a
live DB read. UString operations and the shared_count destructor remain stubs;
the exception/cleanup path is not exercised. The C implementation always takes
the atomic refcount branch; the shipped non-atomic fallback (only when the
runtime gthread probe is null) is not reproduced, but the observable refcount
result is the same. Still open: the DB adapter, scrollbar/skin,
display/scanout binding, independent startup and visible parity.
