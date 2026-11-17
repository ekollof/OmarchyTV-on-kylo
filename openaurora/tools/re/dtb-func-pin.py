#!/usr/bin/env python3
"""Print the func_pin_tbl from the TV's embedded DTB.

Offline only: reads the pristine NAND dump, extracts the DTB at a known
offset and walks its structure block. Used to recover GPIO descriptors
(pin/mode/normal/active) without querying the TV. No device access.

Usage:
  python3 openaurora/tools/re/dtb-func-pin.py [--dump PATH] [--offset HEX]
  python3 openaurora/tools/re/dtb-func-pin.py --dump artifacts/dumps/tv_nand.bin \
      --offset 0xac0040 --name HP_MUTE

Defaults match artifacts/reports/AMP-BALANCE-2026-11-07.md: the DTB starts at
NAND offset 0xac0040 (totalsize 0x11bb3) and the interesting records include
HP_MUTE (id 0xc3, pin 207) and AMP_MUTE (id 0xc4, pin 208).
"""
import argparse
import struct

FDT_MAGIC = 0xD00DFEED


def parse_dtb(data):
    magic, totalsize, off_struct, off_strings, _off_rsv, _ver, _last, _cpu, \
        size_strings, size_struct = struct.unpack('>10I', data[:40])
    if magic != FDT_MAGIC:
        raise SystemExit('not an FDT at this offset')
    strings = data[off_strings:off_strings + size_strings]

    def string_at(off):
        end = strings.find(b'\0', off)
        return strings[off:end].decode('utf-8', 'replace')

    pos = off_struct
    path = []
    nodes = []
    while pos < off_struct + size_struct:
        token = struct.unpack('>I', data[pos:pos + 4])[0]
        pos += 4
        if token == 1:  # FDT_BEGIN_NODE
            end = data.find(b'\0', pos)
            name = data[pos:end].decode('utf-8', 'replace')
            pos = (end + 1 + 3) & ~3
            path.append(name or '/')
            nodes.append({'path': '/'.join(path), 'props': {}})
        elif token == 2:  # FDT_END_NODE
            path.pop()
        elif token == 3:  # FDT_PROP
            length, nameoff = struct.unpack('>II', data[pos:pos + 8])
            pos += 8
            value = data[pos:pos + length]
            pos = (pos + length + 3) & ~3
            nodes[-1]['props'][string_at(nameoff)] = value
        elif token == 4:  # FDT_NOP
            continue
        elif token == 9:  # FDT_END
            break
        else:
            raise SystemExit('bad FDT token %#x at %#x' % (token, pos))
    return nodes


def cstr(value):
    return value.split(b'\0')[0].decode('utf-8', 'replace') if value else ''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dump', default='artifacts/dumps/tv_nand.bin')
    ap.add_argument('--offset', default='0xac0040')
    ap.add_argument('--name', default=None,
                    help='only print this func_pin_tbl node')
    args = ap.parse_args()
    offset = int(args.offset, 0)

    with open(args.dump, 'rb') as f:
        f.seek(offset)
        head = f.read(40)
        if len(head) < 40:
            raise SystemExit('short read at %#x' % offset)
        totalsize = struct.unpack('>I', head[4:8])[0]
        f.seek(offset)
        data = f.read(totalsize)

    nodes = parse_dtb(data)
    print('node\tgroup\tpin\tmode\tnormal\tactive')
    for node in nodes:
        name = node['path'].split('/')[-1]
        props = node['props']
        if '/func_pin_tbl/' not in node['path'] or 'gpio_fun_pin' not in props:
            continue
        if args.name and name != args.name:
            continue
        group, pin, _extra = struct.unpack('>3I', props['gpio_fun_pin'])
        print('%s\t%d\t%d\t%s\t%s\t%s' % (
            name, group, pin, cstr(props.get('gpio_mode')),
            cstr(props.get('normal')), cstr(props.get('active'))))


if __name__ == '__main__':
    main()
