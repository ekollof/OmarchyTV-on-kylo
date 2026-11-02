/* Offline evidence check for TabDialog title keys, not a runtime implementation.
 * Traversal mirrors verify-label.cjs (same hash-checked profile, same bounds);
 * keys come from TabDialog::updateGlyphAndTitle (0xd6faed) literal pool:
 * non-Panasonic arms (this set is Telefunken) plus the SETTINGS default and
 * the tab_text default used by updateTitles. */
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
const section = find(0x30, 'Languages', true);
const value = find(section, 'Dictionary', false);
const base = word(value + 12), length = word(value + 8), end = base + length;
range(base, length);
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
assert.deepEqual(locales[0], ['eng', 'eng', 'eng']);
assert.deepEqual(locales[10], ['dut', 'dut', 'dut']);
const stringsBase = p;
const want = new Map(['PICTURE', 0, 'SOUND', 1, 'NETWORK', 2, 'INSTALLATION_STR', 3,
    'SYSTEM_STR', 4, 'SETTINGS', 'default']
    .flatMap((v, i, a) => i % 2 ? [] : [[v, a[i + 1]]]));
// NOTE: tab_text (0x14ddb58) is a dialog-description key, not a Dictionary
// key, so it correctly has no record here. The five tab button keys live in
// the runtime .bss table and are likewise not statically resolvable.
const hashes = new Map([...want.keys()].map(k => [hash(k), k]));
const found = {};
let previous = 0;
for (let i = 0; i < records; ++i) {
    const entry = base + 8 + i * 8, id = word(entry);
    assert(i === 0 || id >= previous);
    previous = id;
    if (!hashes.has(id)) continue;
    p = stringsBase + word(entry + 4);
    const values = [];
    for (let j = 0; j < languages; ++j) values.push(string());
    const key = hashes.get(id);
    found[key] = { record: i, eng: values[0], dut: values[10] };
    console.log(key + ' record=' + i + ' hash=' + id.toString(16) +
        ' eng=' + JSON.stringify(values[0]) + ' dut=' + JSON.stringify(values[10]));
}
for (const k of want.keys()) assert(k in found, 'missing key ' + k);
console.log('PASS: all ' + want.size + ' title keys resolved');
