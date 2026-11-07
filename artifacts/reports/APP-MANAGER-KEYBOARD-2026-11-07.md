# App-manager keyboard layout and HID/IR key path — 2026-11-07

Status: static recovery from an **unstripped** vendor library. No TV execution
or hardware writes. Raw binary stays out of history; extracted tables are under
`artifacts/analysis/vendor-inventory-2026-11-07/`.

## Source

```text
vendor/com.nvt.appmgr/libappmgr.so
size      391 KB
ELF       32-bit LSB ARM, EABI5, shared object, not stripped
BuildID   fd36d8414541b839f57ab5ab732ba4054897f707
```

The library has 2,476 local/global symbols with sizes and 693 defined dynamic
exports. The full symbol table is in
`exports/libappmgr-all-symbols.txt`; the Ghidra inventory is in
`ghidra/appmgr/` (functions, callgraph, symbols, data). A new project
`artifacts/workbench/ghidra_appmgr` was created read-only after import; no
existing project was modified.

## Keyboard layout table

Symbols:

```text
00052f50  0x180c  d  _ZL15keyboard_layout      (US English layout)
000549ac  0x000c  b  _ZL18keycode_ext_layout   (extended-key map)
00033651  0x00d0  T  _Z17KBL_mapExtKeycodej
00033721  0x0130  T  _Z12KBL_findCodeimm
```

`keyboard_layout` starts at **0x52f50**. Its header is 0x3c bytes:

```text
+0x00  char*  "US English"
+0x08  u32    0x77 (119 populated keys)
```

Key entries start at **0x52f8c**, are **0x30 (48) bytes**, and there are
**127 slots** (`0x52f8c + 127*0x30 = 0x5475c`, exactly the symbol end). Slots
0..118 are populated; 119..126 are zero. Entry layout recovered from
`KBL_findCode`/`KBL_mapExtKeycode` and the data:

```text
+0x00  u32  Linux input keycode (1=Esc, 2..11=1..0, 16..25=q..p, ...)
+0x04  u32  modifier flags (observed 0, 2, 4)
+0x08  u32  highest subrecord index (0 or 1)
+0x0c  subrecord 0: u32 codepoint, u32 codepoint, char* label
+0x18  subrecord 1: u32 codepoint, u32 codepoint, char* label
+0x24  remaining words zero
```

`KBL_findCode(index, modifiers, pressed)` selects subrecord 0 or 1 from the
entry flags and returns `entry + 0x0c + 12*index`; `KBL_mapExtKeycode(code)`
scans entries for the matching Linux keycode and returns `entry + 0x0c`.
The extraction is in **`keyboard-layout-us.tsv`** (127 rows):

```text
index linux_key flags nsub sub0_code sub0_label sub1_code sub1_label
0     1         0     0    27        (esc)      0
1     2         0     1    49        1          33        !
2     3         0     1    50        2          64        @
...
15    16        4     1    113       q          81        Q
27    28        0     0    13        (enter)    0
41    42        0     0    61953     (lshift)   0
58    59        0     0    61697     (F1)       0
70    71        2     0    61445     [home]     55        [7]
71    72        2     0    61442     [up]       56        [8]
74    75        2     0    61440     [left]     52        [4]
95    96        0     0    13        [enter]    0
```

The high values are DirectFB `DIKI_*` keycodes (0xF000 + nav, 0xF100 + F-keys,
0xF200 + modifiers, etc.), not Unicode. This table is the on-screen/native
keyboard mapping, not the BLAST IR code list.

`keycode_ext_layout` is 12 bytes at 0x549ac:

```text
0x00000011 0x00052e4c 0x00000000
0x00170000 0x000000b1 0x00052e4c
0x0000001c 0x00170001 0x00000011
```

Its use is not decoded yet; do not treat these words as a byte array without
checking `HID_LoadKeycodeExtMap` at entry 0x32cdc (Ghidra did not define that
Thumb entry as a function; a disposable-project `DecompileRaw` pass is needed).

## App-manager HID/IR path

Recovered symbols and entry points:

```text
AM_Class::HID_CommandIrInput        0x30f74  (wraps _HID_HandleWindowEvent with type 2)
AM_Class::HID_SimIrInput            0x30ed4
AM_Class::COM_SendKey               0x3678c  (NAM send-key -> DFB event)
AM_Class::HID_NamKeyToDfbKey        0x32c14  (copies ST_LIBNAM_KEY fields -> DFBWindowEvent)
AM_Class::HID_DfbKeyToNamKey        0x32b98  (reverse copy)
AM_Class::HID_HandleWindowEvent     0x313c0
AM_Class::HID_SendWinEventToApp     0x31c1c
AM_Class::_HID_HandleInputEvent     0x4133c  (DFBInputEvent -> WindowEvent)
AM_Class::MGR_SendCommand           0x2a550  (LIBNAM_MGR_SendCommand bridge)
```

`HID_NamKeyToDfbKey` is a field copy between the two structs, not a code-table
lookup: it copies `ST_LIBNAM_KEY` words into the DirectFB window event. The
actual keycode translation is therefore in the NAM/LIBNAM layer and the
`keyboard_layout` table, not in this copy helper. `COM_SendKey` calls
`HID_NamKeyToDfbKey`, sets event type 2, and forwards through
`HID_CommandIrInput`.

`HID_CommandIrInput` traces and calls `_HID_HandleWindowEvent` with a synthetic
event; it is the same path used by the app-manager debug/simulated IR input, so
it is a useful native injection point for offline/CPU tests. It is not a
hardware IR driver and does not prove BLAST timing/repeat behavior.

## Limits

- The 127-slot boundary and entry fields are derived from instructions and data
  arithmetic; the table has not been executed through libappmgr.
- Only the US English layout is present in this 0x180c-byte symbol; other
  languages would be separate tables/files if the product ships them.
- `keycode_ext_layout` and `HID_LoadKeycodeExtMap` remain undecoded.
- No TV, framebuffer, DirectFB, IR or app-manager process was touched.

## Reproduction

```sh
nm -a --defined-only -S libappmgr.so          # 2,476 symbols with sizes
# keyboard-layout-us.tsv was decoded from vaddr 0x52f8c, stride 0x30, using
# the libappmgr ELF mapping (.data vaddr 0x52e48 -> file offset 0x42e48).
/opt/ghidra/support/analyzeHeadless artifacts/workbench/ghidra_appmgr Appmgr \
  -process libappmgr.so -readOnly -noanalysis -scriptPath openaurora/tools/re \
  -postScript DumpFirmware.java _Z12KBL_findCodeimm
```

Report: APP-MANAGER-KEYBOARD-2026-11-07. Vendor inventory:
VENDOR-INVENTORY-2026-11-07.
