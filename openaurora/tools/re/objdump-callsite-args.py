#!/usr/bin/env python3
"""Recover immediate register values at calls in an objdump listing.

Reads `objdump -d --no-show-raw-insn` output (for example
artifacts/workbench/aurora.dis) and prints every call to a named symbol with
the last immediate assignment to a register inside the same basic block.
Handles MOV/MOVS/MOVW/MOVT and stops at a branch or another call.

Usage: objdump-callsite-args.py <listing> <symbol-substring> [reg]
"""
import re
import sys

INSN = re.compile(r"^\s*([0-9a-f]+):\s+(.+?)\s*$")
FUNC = re.compile(r"^([0-9a-f]+)\s+<([^>]+)>:\s*$")
REG = re.compile(r"^(movs?|mov\.w|movw|movt)\s+%s\s*,\s*#(0x[0-9a-f]+|\d+)$" % "r0")
BRANCH = re.compile(r"^(b|bl|blx|bx|cbz|cbnz|pop|ldr\s+pc)\b")


def parse_value(s):
    return int(s, 16) if s.startswith("0x") else int(s, 10)


def main():
    if len(sys.argv) < 3:
        print(__doc__, file=sys.stderr)
        return 2
    listing, target = sys.argv[1], sys.argv[2]
    reg = sys.argv[3] if len(sys.argv) > 3 else "r0"
    global REG
    REG = re.compile(r"^(movs?|mov\.w|movw|movt)\s+%s\s*,\s*#(0x[0-9a-f]+|\d+)$" % re.escape(reg))
    caller = ""
    window = []
    calls = known = 0
    print("symbol\tcall_site\tcaller\tvalue\tstatus\tcontext")
    with open(listing, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            fm = FUNC.match(line)
            if fm:
                caller = fm.group(2)
                window = []
                continue
            im = INSN.match(line)
            if not im:
                continue
            addr, text = im.group(1), im.group(2)
            text = text.split(";")[0].strip()
            if target in text and re.search(r"\b(bl|blx)\b", text):
                calls += 1
                found = None
                movt = None
                ctx = []
                for paddr, ptext in reversed(window):
                    ctx.insert(0, "%s: %s" % (paddr, ptext))
                    m = REG.match(ptext)
                    if m:
                        mnem, val = m.group(1), parse_value(m.group(2))
                        if mnem == "movt":
                            movt = val
                        elif mnem in ("movw",):
                            found = ((movt or 0) << 16) | val
                            break
                        else:
                            found = val
                            break
                    if BRANCH.match(ptext):
                        break
                if found is not None:
                    known += 1
                print("%s\t%s\t%s\t%s\t%s\t%s" % (
                    target, addr, caller,
                    ("0x%x" % found) if found is not None else "",
                    "IMM" if found is not None else "UNKNOWN",
                    " ; ".join(ctx)))
                window = []
                continue
            window.append((addr, text))
            if len(window) > 24:
                window.pop(0)
    print("# calls=%d known=%d" % (calls, known), file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
