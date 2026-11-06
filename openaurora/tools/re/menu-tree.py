#!/usr/bin/env python3
"""Decode the Aurora menu tree from shipped firmware evidence.

Inputs are offline only:
  --elf        aurora.elf (for resolving string pointers in .rodata)
  --decompile  DumpFirmware.java output for the SubLauncherDialog initialisers
  --index/--bin  the telefunken18 BinSVG pack

It emits a JSON model and a readable tree:

  dialog SubLauncher (DialogSubLauncher.svg)
    list box 189,255 1774x222
    item 240x140, 300 ms scale 1->1.35, glyph 0->-42/-24
    branch 2 picture
      MODE (0)
      ENERGY_SAVING (1) -> OFF, MINIMUM_STR, ...
      PICTURE_ZOOM (2) -> ZOOM_MODE_16_9, ...

The item/value extraction follows the SettingItem/SettingValue constructor
calls in the decompiled initialisers; resource geometry comes from the BinSVG
records. Dynamic values are marked as such instead of guessed.
"""
import argparse
import json
import os
import re
import struct
import subprocess
import sys
import zlib

# ---------------------------------------------------------------- ELF strings


def load_segments(elf):
    out = subprocess.check_output(["readelf", "-lW", elf], text=True)
    segments = []
    for line in out.splitlines():
        m = re.match(
            r"\s*LOAD\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)"
            r"\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)",
            line,
        )
        if m:
            off, vaddr, _paddr, _filesz, memsz = (int(x, 16) for x in m.groups())
            segments.append((vaddr, vaddr + memsz, off))
    return segments


class Elf:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        self.segments = load_segments(path)

    def bytes(self, vaddr, n):
        for lo, hi, off in self.segments:
            if lo <= vaddr < hi:
                start = off + (vaddr - lo)
                return self.data[start:start + n]
        return b""

    def u32(self, vaddr):
        b = self.bytes(vaddr, 4)
        return struct.unpack("<I", b)[0] if len(b) == 4 else None

    def cstr(self, vaddr):
        b = self.bytes(vaddr, 256)
        end = b.find(b"\0")
        if end < 0:
            return None
        return b[:end].decode("latin1")

    def string_at_literal(self, vaddr):
        """Resolve a DAT_ address: the address holds a pointer to the string."""
        p = self.u32(vaddr)
        return self.cstr(p) if p else None

    def string_at_pointer_slot(self, vaddr):
        """Resolve _FUN_/pcRam slots that actually hold a string pointer."""
        p = self.u32(vaddr)
        return self.cstr(p) if p else None


# ---------------------------------------------------------------- BinSVG pack


def u16(b, p):
    return b[p] | b[p + 1] << 8


def s16(v):
    return v - 65536 if v & 0x8000 else v


class Pack:
    def __init__(self, index_path, bin_path):
        self.data = open(bin_path, "rb").read()
        self.records = {}
        with open(index_path, "r") as f:
            count = int(f.readline().strip())
            for _ in range(count):
                line = f.readline().split()
                if len(line) != 2:
                    raise SystemExit("bad pack index line")
                self.records[int(line[0], 16)] = int(line[1])
        self.offsets = {}
        off = 0
        for h, n in self.records.items():
            self.offsets[h] = off
            off += n
        if off != len(self.data):
            raise SystemExit("pack index does not cover binary")

    def record(self, name):
        h = zlib.crc32(name.encode()) & 0xFFFFFFFF
        if h not in self.offsets:
            return None
        off = self.offsets[h]
        return self.data[off:off + self.records[h]]

    def decode(self, name):
        data = self.record(name)
        if data is None:
            return None
        pos = 0
        elements = []
        depth = 0
        while pos < len(data):
            start = pos
            tag = data[pos]
            pos += 1
            if tag == 9:  # SVGGroupEnd
                depth = max(0, depth - 1)
                continue
            end = data.find(b"\0", pos)
            if end < 0:
                raise SystemExit(f"{name}: unterminated id at {start}")
            rid = data[pos:end].decode("latin1")
            pos = end + 1
            element = {"tag": tag, "id": rid, "offset": start, "depth": depth}
            if tag in (10, 11):  # animation
                subtype = data[pos]
                pos += 1
                offset = s16(u16(data, pos))
                pos += 2
                strings = []
                for _ in range(3):  # link, attribute, transform
                    end = data.find(b"\0", pos)
                    if end < 0:
                        raise SystemExit(f"{name}: unterminated animation string")
                    strings.append(data[pos:end].decode("latin1"))
                    pos = end + 1
                begin = s16(u16(data, pos))
                pos += 2
                values = []
                for _ in range(6):
                    end = data.find(b"\0", pos)
                    if end < 0:
                        raise SystemExit(f"{name}: unterminated animation value")
                    values.append(data[pos:end].decode("latin1"))
                    pos = end + 1
                element.update(
                    subtype=subtype,
                    animation_offset=offset,
                    link=strings[0],
                    attribute=strings[1],
                    transform=strings[2],
                    begin=begin,
                    duration=values[0],
                    fill=values[1],
                    from_value=values[2],
                    to_value=values[3],
                    calc_mode=values[4],
                    key_splines=values[5],
                )
                elements.append(element)
                continue
            fixed = {0: 11, 1: 11, 2: 4, 3: 9, 4: 11, 5: 3, 6: 3, 7: 3, 8: 0}.get(tag)
            nstrings = {0: 1, 1: 4, 2: 2, 3: 2, 4: 2, 5: 3, 6: 3, 7: 3, 8: 0}.get(tag)
            if fixed is None or nstrings is None:
                raise SystemExit(f"{name}: unknown tag {tag} at {start}")
            raw = data[pos:pos + fixed]
            pos += fixed
            draws = []
            for _ in range(nstrings):
                end = data.find(b"\0", pos)
                if end < 0:
                    raise SystemExit(f"{name}: unterminated element string")
                draws.append(data[pos:end].decode("latin1"))
                pos = end + 1
            if tag == 1:
                pos += 2  # tail
                x, y, w, h, word4 = struct.unpack("<hhhhH", raw[:10])
                opacity = struct.unpack("<b", raw[10:11])[0]
                element.update(
                    rect=[x, y, w, h],
                    word4=word4,
                    opacity=opacity,
                    stroke=draws[0] if draws else "",
                    fill=draws[1] if len(draws) > 1 else "",
                    animation=draws[2] if len(draws) > 2 else "",
                    animate=draws[3] if len(draws) > 3 else "",
                )
            elif tag == 8:
                depth += 1
            elements.append(element)
        return elements


def rects_by_id(elements):
    return {
        e["id"]: e["rect"]
        for e in elements
        if e["tag"] == 1 and e.get("id")
    }


def animations(elements):
    out = []
    for e in elements:
        if e["tag"] not in (10, 11):
            continue
        out.append(
            {
                "target": e["link"],
                "attribute": e["attribute"],
                "transform": e["transform"],
                "begin": e["begin"],
                "duration": e["duration"],
                "fill": e["fill"],
                "from": e["from_value"],
                "to": e["to_value"],
                "calc_mode": e["calc_mode"],
                "key_splines": e["key_splines"],
            }
        )
    return out


# --------------------------------------------------------- decompiled menu tree


def split_args(text):
    args, depth, start, quote = [], 0, 0, None
    for i, ch in enumerate(text):
        if quote:
            if ch == quote:
                quote = None
            continue
        if ch in "\"'":
            quote = ch
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            args.append(text[start:i].strip())
            start = i + 1
    args.append(text[start:].strip())
    return args


def calls(text, name):
    """Yield (position, args) for name(...) calls, balanced parentheses.

    The Ghidra decompiler wraps long calls as ``Name\\n    (args)``; allow
    whitespace between the callee and the opening parenthesis.
    """
    pattern = re.compile(re.escape(name) + r"\s*\(")
    pos = 0
    while True:
        match = pattern.search(text, pos)
        if not match:
            return
        at = match.start()
        open_at = match.end() - 1
        depth = 0
        quote = None
        end = None
        for i in range(open_at, len(text)):
            ch = text[i]
            if quote:
                if ch == quote:
                    quote = None
                continue
            if ch in "\"'":
                quote = ch
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    end = i
                    break
        if end is None:
            return
        yield at, split_args(text[open_at + 1:end])
        pos = end + 1


def resolve_key(elf, text):
    text = text.strip()
    m = re.fullmatch(r"DAT_([0-9a-fA-F]+)", text)
    if m:
        return elf.string_at_literal(int(m.group(1), 16))
    m = re.fullmatch(r"(?:_FUN_|pcRam)([0-9a-fA-F]+)", text)
    if m:
        return elf.string_at_pointer_slot(int(m.group(1), 16))
    return None


BRANCHES = [
    ("initialisePictureSettingsShortcuts", 2, "picture"),
    ("initialiseSoundSettingsShortcuts", 3, "sound"),
    ("initialiseSystemSettingsShortcuts", 7, "system"),
    ("initialiseChannelListShortcuts", 0, "channel_list"),
    ("initialiseFVPShortcuts", 52, "fvp"),
    ("initialiseAIMenuShortcuts", 61, "ai"),
    ("initialiseRecommendationShortcuts", None, "recommendations"),
    ("initialiseAdvertisementBanner", None, "advertisement"),
]

DYNAMIC_KEYS = {
    "initialiseSoundSettingsShortcuts": {
        2: ("DOLBY_AUDIO_OR_MODE",
            "uVar3: DAT_00ee1de0 'Dolby Audio' or DAT_00ee1de4 'MODE'"),
    },
    "initialiseSystemSettingsShortcuts": {
        0: ("SLEEP_TIMER",
            "uVar8: DAT_00ee239c 'SLEEP_TIMER_STR' or DAT_00ee2398 'OFF_TIMER'"),
        1: ("AUTO_TV_OFF",
            "UString from DAT_00ee26cc 'AUTO_TV_OFF' or DAT_00ee26d0 'AUTO_TV_OFF_NO_TUNER'"),
    },
}

# PictureModeNameProvider's runtime table, filled by the initializer at
# 0x6eee50: {0 DYNAMIC, 1 NATURAL, 2 CINEMA, 3 GAME, 4 SPORT}. The firmware
# names are resolved from the literals at 0x6eee98..0x6eeea8.
PICTURE_MODES = ["DYNAMIC", "NATURAL", "CINEMA", "GAME", "SPORT"]

DYNAMIC_VALUES = {
    ("initialiseSystemSettingsShortcuts", 0): "sleep_timer_values",
    ("initialiseSystemSettingsShortcuts", 1): "auto_tv_off_values",
    ("initialiseSystemSettingsShortcuts", 3): "languages",
    ("initialiseChannelListShortcuts", 0): "favourite_lists",
    ("initialiseFVPShortcuts", None): "fvp_apps",
    ("initialiseRecommendationShortcuts", None): "recommendations",
    ("initialiseAdvertisementBanner", None): "advertising",
}


def decompile_sections(path):
    text = open(path).read()
    sections = []
    for part in re.split(r"===== ", text)[1:]:
        header = part.split("\n", 1)[0]
        m = re.search(r"->\s+(\S+)\s+@\s+([0-9a-fA-F]+)", header)
        if not m:
            continue
        sections.append((m.group(1), part))
    return sections


def parse_branch(elf, name, body):
    events = []
    for at, args in calls(body, "SettingValue::SettingValue"):
        key = resolve_key(elf, args[3]) if len(args) > 3 else None
        events.append((at, "value", args[2], key))
    for at, args in calls(body, "SettingItem::SettingItem"):
        key = resolve_key(elf, args[3]) if len(args) > 3 else None
        events.append((at, "item", args[2], key))
    events.sort()
    items, pending = [], []
    for _at, kind, ident, key in events:
        if kind == "value":
            pending.append({"id": ident, "key": key})
            continue
        try:
            item_id = int(ident, 0)
        except ValueError:
            item_id = None
        items.append({"id": item_id, "key": key or "DYNAMIC", "values": pending})
        pending = []
    if pending and items:
        items[-1]["values"].extend(pending)
    return items


def build_tree(elf, decompile_path):
    sections = dict(decompile_sections(decompile_path))
    branches = []
    for suffix, launcher_type, label in BRANCHES:
        match = next((k for k in sections if k.endswith(suffix)), None)
        if not match:
            continue
        items = parse_branch(elf, match, sections[match])
        for item in items:
            dynamic = DYNAMIC_KEYS.get(suffix, {}).get(item["id"])
            if dynamic:
                item["key"] = dynamic[0]
                item["dynamic"] = dynamic[1]
            if suffix == "initialisePictureSettingsShortcuts" and item["id"] == 0:
                item["values"] = [
                    {"id": i, "key": key}
                    for i, key in enumerate(PICTURE_MODES)
                ]
            vdyn = DYNAMIC_VALUES.get((suffix, item["id"]))
            if vdyn:
                item["values"] = [{"id": 0, "key": vdyn}]
            if not item["values"]:
                item["values"] = [{"id": 0, "key": "dynamic"}]
            # The decompiler shows every conditional branch that builds the
            # same value vector. Keep the first occurrence of each key.
            seen, unique = set(), []
            for value in item["values"]:
                if value["key"] in seen:
                    continue
                seen.add(value["key"])
                unique.append(value)
            item["values"] = unique
        items.sort(key=lambda x: (x["id"] is None, x["id"]))
        branches.append(
            {
                "launcher_type": launcher_type,
                "name": label,
                "function": suffix,
                "items": items,
            }
        )
    return branches


# ----------------------------------------------------------------------- output


def c_quote(text):
    if text is None:
        return "NULL"
    return '"' + text.replace("\\", "\\\\").replace('"', '\\"') + '"'


def emit_c(model, path):
    """Emit the checked-in menu model used by common/menu_model.c."""
    lines = [
        "/* Generated by tools/re/menu-tree.py from aurora.elf and the shipped",
        " * telefunken18 BinSVG pack. Do not edit by hand; rerun the decoder. */",
        "#ifndef OA_MENU_MODEL_GENERATED_H",
        "#define OA_MENU_MODEL_GENERATED_H",
        "",
        '#include "openaurora/menu_model.h"',
        "",
    ]
    branches = [
        b for b in model["branches"]
        if b["launcher_type"] is not None and b["name"] not in model["excluded"]
    ]
    for branch in branches:
        btype = branch["launcher_type"]
        for item in branch["items"]:
            iid = -1 if item["id"] is None else item["id"]
            vname = f"oa_menu_b{btype}_i{iid}_values"
            lines.append(f"static const struct oa_menu_value {vname}[] = {{")
            if item["values"]:
                for value in item["values"]:
                    lines.append(f"    {{ {c_quote(value['key'])} }},")
            else:
                lines.append('    { "dynamic" },')
            lines.append("};")
        iname = f"oa_menu_b{btype}_items"
        lines.append(f"static const struct oa_menu_item {iname}[] = {{")
        for item in branch["items"]:
            iid = -1 if item["id"] is None else item["id"]
            vname = f"oa_menu_b{btype}_i{iid}_values"
            count = len(item["values"]) or 1
            key = c_quote(item["key"])
            action = c_quote(f"setting:{item['key']}")
            dynamic = c_quote(item.get("dynamic"))
            lines.append(
                f"    {{ {iid}, {key}, {action}, {dynamic}, {vname}, {count} }},"
            )
        lines.append("};")
        lines.append("")
    lines.append("static const struct oa_menu_branch oa_menu_generated_branches[] = {")
    for branch in branches:
        btype = branch["launcher_type"]
        lines.append(
            f"    {{ {btype}, {c_quote(branch['name'])}, oa_menu_b{btype}_items, "
            f"{len(branch['items'])} }},"
        )
    lines.append("};")
    lines.append("")
    rects = model["dialog"]["rects"]
    item_rects = model["item"]["rects"]
    list_box = rects.get("listBox", [0, 0, 0, 0])
    scalerect = item_rects.get("scalerect", [0, 0, 0, 0])
    glyph = item_rects.get("glyph", [0, 0, 0, 0])
    text = item_rects.get("textPos", [0, 0, 0, 0])
    scale_from, scale_to = 1.0, 1.35
    glyph_dx, glyph_dy, anim_ms = -42, -24, 300
    for anim in model["item"]["animations"]:
        if anim["target"] == "#scalerect" and anim["transform"] == "scale":
            parts = anim["from"].split()
            if parts:
                scale_from = float(parts[0])
            parts = anim["to"].split()
            if parts:
                scale_to = float(parts[0])
            anim_ms = int(round(float(anim["duration"]) * 1000))
        elif anim["target"] == "#glyph" and anim["attribute"] == "x":
            glyph_dx = int(float(anim["to"]))
        elif anim["target"] == "#glyph" and anim["attribute"] == "y":
            glyph_dy = int(float(anim["to"]))
    lines.append("static const struct oa_menu_dialog oa_menu_generated_sub_launcher = {")
    lines.append('    "SubLauncherDialog", "SubLauncher", "DialogSubLauncher.svg",')
    lines.append(
        f"    {list_box[0]}, {list_box[1]}, {list_box[2]}, {list_box[3]},"
    )
    lines.append(f"    {scalerect[2]}, {scalerect[3]},")
    lines.append(f"    {scalerect[0]}, {scalerect[1]},")
    lines.append(f"    {glyph[0]}, {glyph[1]}, {glyph[2]}, {glyph[3]},")
    lines.append(f"    {text[0]}, {text[1]}, {text[2]}, {text[3]},")
    lines.append(
        f"    {anim_ms}, {scale_from:.3f}f, {scale_to:.3f}f, {glyph_dx}, {glyph_dy}"
    )
    lines.append("};")
    lines.append("")
    lines.append("#endif")
    with open(path, "w") as f:
        f.write("\n".join(lines) + "\n")


DYNAMIC_MARKERS = {
    "sleep_timer_values", "auto_tv_off_values", "languages",
    "favourite_lists", "fvp_apps", "recommendations", "advertising", "dynamic",
}


def runtime_values(item):
    keys = [value["key"] for value in item["values"]]
    if len(keys) == 1 and keys[0] in DYNAMIC_MARKERS:
        return keys[0]
    return [
        {"key": key, "action": f"set:{item['key']}={key}"}
        for key in keys
    ]


def emit_json(model, path):
    rects = model["dialog"]["rects"]
    item_rects = model["item"]["rects"]
    list_box = rects.get("listBox", [0, 0, 0, 0])
    scalerect = item_rects.get("scalerect", [0, 0, 0, 0])
    glyph = item_rects.get("glyph", [0, 0, 0, 0])
    text = item_rects.get("textPos", [0, 0, 0, 0])
    scale_from, scale_to, anim_ms = 1.0, 1.35, 300
    glyph_dx, glyph_dy = -42, -24
    for anim in model["item"]["animations"]:
        if anim["target"] == "#scalerect" and anim["transform"] == "scale":
            scale_from = float(anim["from"].split()[0])
            scale_to = float(anim["to"].split()[0])
            anim_ms = int(round(float(anim["duration"]) * 1000))
        elif anim["target"] == "#glyph" and anim["attribute"] == "x":
            glyph_dx = int(float(anim["to"]))
        elif anim["target"] == "#glyph" and anim["attribute"] == "y":
            glyph_dy = int(float(anim["to"]))
    branches = []
    for branch in model["branches"]:
        out = {
            "launcher_type": -1 if branch["launcher_type"] is None else branch["launcher_type"],
            "name": branch["name"],
            "items": [],
        }
        if branch["name"] in model["excluded"] or branch["launcher_type"] is None:
            out["excluded"] = True
        for item in branch["items"]:
            entry = {
                "id": -1 if item["id"] is None else item["id"],
                "key": item["key"],
                "action": f"setting:{item['key']}",
                "values": runtime_values(item),
            }
            if item.get("dynamic"):
                entry["dynamic"] = item["dynamic"]
            out["items"].append(entry)
        branches.append(out)
    document = {
        "version": 1,
        "source": "aurora.elf + telefunken18 BinSVG; generated by tools/re/menu-tree.py",
        "dialogs": [
            {
                "id": "sub_launcher",
                "class": model["dialog"]["class"],
                "name": model["dialog"]["name"],
                "svg": model["dialog"]["svg"],
                "list": list_box,
                "item": {
                    "w": scalerect[2],
                    "h": scalerect[3],
                    "scalerect": scalerect,
                    "glyph": glyph,
                    "text": text,
                    "anim_ms": anim_ms,
                    "scale": [scale_from, scale_to],
                    "glyph_delta": [glyph_dx, glyph_dy],
                },
                "layout": {
                    "source": "p0/03 settings_top capture (0,240 1920x180): "
                              "cards 201/451/701,36 235x130, pitch 250; "
                              "item cell 199,274 240x140",
                    "card": [201, 276, 235, 130],
                    "pitch": 250,
                    "item": [199, 274, 240, 140],
                },
                "branches": branches,
            }
        ],
    }
    with open(path, "w") as f:
        json.dump(document, f, indent=2)
        f.write("\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--elf", required=True)
    ap.add_argument("--decompile", required=True)
    ap.add_argument("--index", required=True)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--emit-c", default="")
    ap.add_argument("--emit-json", default="")
    args = ap.parse_args()
    elf = Elf(args.elf)
    pack = Pack(args.index, args.bin)
    branches = build_tree(elf, args.decompile)

    dialog = pack.decode("DialogSubLauncher.svg")
    listbox = pack.decode("SettingSubLauncherListBoxFA.svg")
    row = pack.decode("SettingSubLauncherListBoxRowFA.svg")
    rowitems = pack.decode("SettingItemsListBoxRowItems_sublauncher.svg")
    item = pack.decode("sublauncheritemFA.svg")
    if not all([dialog, listbox, row, rowitems, item]):
        raise SystemExit("missing one of the sub-launcher BinSVG resources")

    model = {
        "dialog": {
            "class": "SubLauncherDialog",
            "name": "SubLauncher",
            "svg": "DialogSubLauncher.svg",
            "rects": rects_by_id(dialog),
            "animations": animations(dialog),
        },
        "listbox": {
            "svg": "SettingSubLauncherListBoxFA.svg",
            "rects": rects_by_id(listbox),
            "row_svg": "SettingSubLauncherListBoxRowFA.svg",
            "row_rects": rects_by_id(row),
            "row_items_svg": "SettingItemsListBoxRowItems_sublauncher.svg",
            "row_items_rects": rects_by_id(rowitems),
            "animations": animations(listbox),
        },
        "item": {
            "svg": "sublauncheritemFA.svg",
            "rects": rects_by_id(item),
            "animations": animations(item),
        },
        "branches": branches,
        "excluded": ["fvp", "recommendations", "advertisement"],
    }

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "menu-tree.json"), "w") as f:
        json.dump(model, f, indent=2, sort_keys=True)
        f.write("\n")

    lines = []
    lines.append(f"dialog {model['dialog']['class']} ({model['dialog']['svg']})")
    for name, rect in model["dialog"]["rects"].items():
        lines.append(f"  rect {name} {rect}")
    for anim in model["item"]["animations"]:
        lines.append(
            "  item anim %s %s %s begin=%s duration=%s from=%s to=%s"
            % (
                anim["target"],
                anim["attribute"],
                anim["transform"],
                anim["begin"],
                anim["duration"],
                anim["from"],
                anim["to"],
            )
        )
    for branch in branches:
        typ = "default" if branch["launcher_type"] is None else str(branch["launcher_type"])
        lines.append(f"branch {typ} {branch['name']} ({branch['function']})")
        for item_node in branch["items"]:
            vals = ", ".join(v["key"] or "?" for v in item_node["values"])
            note = (f" [dynamic: {item_node['dynamic']}]"
                    if item_node.get("dynamic") else "")
            lines.append(
                f"  item {item_node['id']} {item_node['key']}{note} -> {vals}"
            )
    tree = "\n".join(lines) + "\n"
    with open(os.path.join(args.out, "menu-tree.txt"), "w") as f:
        f.write(tree)
    if args.emit_c:
        emit_c(model, args.emit_c)
    if args.emit_json:
        emit_json(model, args.emit_json)
    sys.stdout.write(tree)


if __name__ == "__main__":
    main()
