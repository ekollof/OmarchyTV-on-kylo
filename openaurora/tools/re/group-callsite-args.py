#!/usr/bin/env python3
"""Group objdump call-site immediate rows into a command map.

Input is the TSV produced by objdump-callsite-args.py. Output columns:
value, calls, callers, call_sites. Writes to stdout unless an output path is
given as the second argument.
"""
import collections
import sys

if len(sys.argv) not in (2, 3):
    print(__doc__, file=sys.stderr)
    sys.exit(2)
rows = collections.defaultdict(lambda: {"calls": 0, "callers": set(), "sites": []})
with open(sys.argv[1], encoding="utf-8") as fh:
    fh.readline()
    for line in fh:
        if line.startswith("#"):
            continue
        p = line.rstrip("\n").split("\t")
        if len(p) < 5 or p[4] != "IMM":
            continue
        d = rows[p[3]]
        d["calls"] += 1
        d["callers"].add(p[2])
        d["sites"].append(p[1])
out = open(sys.argv[2], "w", encoding="utf-8") if len(sys.argv) == 3 else sys.stdout
print("value\tcalls\tcallers\tcall_sites", file=out)
for value, d in sorted(rows.items(), key=lambda kv: int(kv[0], 16)):
    print("%s\t%d\t%s\t%s" % (value, d["calls"], ",".join(sorted(d["callers"])),
                              ",".join(d["sites"])), file=out)
if out is not sys.stdout:
    out.close()
