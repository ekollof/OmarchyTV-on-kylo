/* Offline evidence check, not a runtime localization implementation.
 * Firmware ARM sources and scope are identified in README.md. */
'use strict';
const fs = require('node:fs');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const b = fs.readFileSync('artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin');
assert.equal(crypto.createHash('md5').update(b).digest('hex'),
             '2726ce3299039204ef54dbefa43cf33f');
function range(offset, size, end = b.length) {
    assert(Number.isSafeInteger(offset) && Number.isSafeInteger(size));
    assert(offset >= 0 && size >= 0 && offset <= end && size <= end - offset);
}
function word(p) { range(p, 4); return b.readUInt32LE(p); }
function hash(s) {
    assert(/^[\x01-\x7f]*$/.test(s)); // ASCII keys only; ARM uses signed bytes.
    let h = (0xf1d3b597 ^ (s.length << 6)) >>> 0, prev = 0, sum = 0, rolling = 0;
    for (const c of Buffer.from(s)) {
        h = ((h + 2 * ~prev) ^ (sum << 9)) >>> 0;
        sum = (sum + c) >>> 0;
        h = ((h + (c << 17)) ^ rolling) >>> 0;
        rolling = (c + (rolling << 8)) >>> 0;
        h = (h + (h << (c % 3))) >>> 0;
        h = (h + (h >>> (prev % 5))) >>> 0;
        prev = c;
    }
    return h;
}
function find(parent, key, section) {
    const end = parent + word(parent + 8) * 20;
    range(parent, end - parent + 20);
    for (let p = parent + 20; p <= end;) {
        const kind = b[p + 4];
        if ((kind === 1) === section && word(p) === hash(key)) return p;
        const next = p + (kind === 1 ? 20 * (word(p + 8) + 1) : 20);
        assert(next > p && next <= end + 20);
        p = next;
    }
    throw Error('Missing profile entry: ' + key);
}
assert.equal(b.subarray(0, 4).toString(), 'EFSL');
assert.equal(word(0x20), 0); // ROMProfile initializes the data base to the file.
const section = find(0x30, 'Languages', true);
const value = find(section, 'Dictionary', false);
assert.equal(section, 0x44);
assert.equal(value, 0x38c);
assert.equal(b[value + 4], 4);
const base = word(value + 12), length = word(value + 8), end = base + length;
range(base, length);
assert.equal(base, 0x3dc);
assert.equal(length, 5170703);
const languages = word(base), records = word(base + 4);
assert.equal(languages, 41);
assert.equal(records, 2435);
range(base + 8, records * 8, end);
let p = base + 8 + records * 8;
function string() {
    range(p, 1, end);
    const stop = b.indexOf(0, p);
    assert(stop >= p && stop < end);
    const start = p;
    p = stop + 1;
    return b.subarray(start, stop).toString('utf8');
}
const locales = [];
for (let i = 0; i < languages; ++i) locales.push([string(), string(), string()]);
const stringsBase = p;
assert.equal(stringsBase, 0x51e8);
assert.deepEqual(locales[0], ['eng', 'eng', 'eng']);
assert.deepEqual(locales[10], ['dut', 'dut', 'dut']);
let previous = 0, matches = 0;
for (let i = 0; i < records; ++i) {
    const entry = base + 8 + i * 8, id = word(entry);
    assert(i === 0 || id >= previous);
    previous = id;
    p = stringsBase + word(entry + 4);
    const values = [];
    for (let j = 0; j < languages; ++j) values.push(string());
    if (id !== hash('MODE')) continue;
    ++matches;
    assert.equal(i, 987);
    assert.equal(id, 0x5aa5907f);
    assert.equal(values[0], 'Mode');
    assert.equal(values[10], 'Modus');
    console.log('MODE record=' + i + ' hash=' + id.toString(16) +
                ' relativeOffset=' + word(entry + 4));
    console.log('eng=' + JSON.stringify(values[0]) + ' dut=' + JSON.stringify(values[10]));
    console.log('dut bytes=' + Buffer.from(values[10] + '\0').toString('hex'));
}
assert.equal(matches, 1);
console.log('PASS: bounded traversal of ' + records + ' records x ' + languages + ' locales');
