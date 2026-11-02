/* Offline validation of the saved, non-atomic view-only title observations.
 * No TV connection, function calls or memory writes to the target process. */
'use strict';
const fs = require('node:fs');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const root = process.argv[2] || 'artifacts/workbench/title-horizontal-20261103';
const elf = fs.readFileSync('artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf');
assert.equal(crypto.createHash('md5').update(elf).digest('hex'),
             '64c252340d3af6c5d66072611b7bad7a');
const heaps = fs.readFileSync(root + '/maps.txt', 'utf8').split('\n').flatMap(line => {
    const m = /^([0-9a-f]+)-([0-9a-f]+) rw-p 00000000 00:00 0\s+\[heap\]$/.exec(line);
    return m ? [[parseInt(m[1], 16), parseInt(m[2], 16)]] : [];
});
assert(heaps.length);
function read(name, address, length) {
    assert(heaps.some(([lo, hi]) => address >= lo && address + length <= hi));
    const b = fs.readFileSync(root + '/' + name + '.bin');
    assert.equal(b.length, length, name);
    return b;
}
const word = (b, p) => b.readUInt32LE(p);
const rect = (b, p) => Array.from({ length: 4 }, (_, i) => b.readInt32LE(p + i * 4));
const scan = fs.readFileSync('artifacts/analysis/ui-title-horizontal-2026-11-03/live-scan.txt', 'utf8');
const titles = [...scan.matchAll(/^candidate=([0-9a-f]+) class=SVGTextBox created=1 .* font=18 /gm)];
assert.equal(titles.length, 1);
assert.match(scan, /complete=1/);
const address = parseInt(titles[0][1], 16);
const box = read('title-object', address, 704);
assert.equal(word(box, 0), 0x153b904);
assert.deepEqual(rect(box, 0x28), [57, 5, 691, 95]);
assert.deepEqual(rect(box, 0x1f4), [51, 0, 634, 90]);
assert.equal(word(box, 0x21c), 18);
assert.equal(word(box, 0xa4), 12);
assert.equal(read('title-string-storage', word(box, 0xa8), 32).subarray(0, 13).toString(),
             'Instellingen\0');
const wrapper = read('title-surface-wrapper', word(box, 0x1d8), 8);
const impl = read('title-surface-impl', word(wrapper, 4), 16);
const context = read('title-surface-context', word(impl, 8), 16);
const surface = word(context, 0);
assert.equal(word(read('title-hal-vptr', surface, 4), 0), 0x13f86e0);
const bounds = read('title-hal-bounds', surface + 8, 16);
assert.deepEqual(rect(bounds, 0), [108, 5, 691, 95]);
const rootPointer = read('title-root-pointer', surface + 0x20, 4);
const drawing = read('title-root', word(rootPointer, 0), 28);
assert.equal(word(drawing, 0), 0x13f86b4);
const commandAddress = word(drawing, 0x14);
const first = read('title-first-command', commandAddress, 28);
assert.equal(word(first, 0), 0x154c44c);
const command = read('title-text-command', commandAddress, 172);
assert.deepEqual(command.subarray(0, 28), first);
assert.deepEqual(rect(command, 0x24), [0, 0, 0, 0]);
assert.equal(word(command, 0x48), 1); // TextControls horizontal justification.
assert.equal(word(command, 0x4c), 3); // Vertical center.
assert.equal(word(command, 0x54), 1); // CharClipping, not a bold-font flag.
assert.equal(word(read('title-font-vptr', word(command, 0x1c), 4), 0), 0x13f9340);
// CORRECTION: b42468 returns -signed(font+5); sharing this function does NOT
// prove a zero return. The later guarded live header read supplies that byte.
const fontHeader = read('title-font-header', word(command, 0x1c), 9);
assert.equal(word(fontHeader, 0), 0x13f9340);
const xOffset = -fontHeader.readInt8(5);
assert.equal(xOffset, 23);
for (const offset of [0x1c, 0x20])
    assert.equal(word(elf, 0x13f9340 + offset - 0x10000), 0xb42469);
const line = read('title-line-object', word(command, 0x20), 64);
assert.equal(word(line, 0), 0x154c52c);
assert.deepEqual(rect(line, 8).slice(0, 2), [xOffset, 5]);
assert.equal(word(line, 0x14), 12);
assert.deepEqual([word(line, 0x34), word(line, 0x38)], [1, 1]);
assert.deepEqual(read('title-line-origin', word(command, 0x20), 16), line.subarray(0, 16));
console.log('PASS: heap-bounded saved textbox -> UISurface -> hal::Surface -> RootDrawing -> TextCommand -> LineObj');
console.log('text=Instellingen; surface=108,5,691,95; margins=0; horizontal=start1 vertical=center3 CharClipping=1');
console.log('retained line origin=23,5; horizontal ratio=1/1; BitmapFont X offset=-signed(font+5)=23');
console.log('NONATOMIC saved observations; font header added by later boot/exe-guarded view-only read.');
