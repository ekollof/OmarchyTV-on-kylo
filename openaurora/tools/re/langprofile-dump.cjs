#!/usr/bin/env node
/* Decode the vendor langprofile Dictionary for requested string keys.
 *
 * The EFSL/ROMProfile/Dictionary layout and the custom string hash are the
 * ones verified in artifacts/analysis/ui-row-labels-2026-11-02. This tool is
 * read-only and prints every locale's value, so UI code can use the same
 * keyed strings instead of fitting Dutch text.
 *
 *   node tools/re/langprofile-dump.cjs PROFILE KEY...
 *   node tools/re/langprofile-dump.cjs --json PROFILE KEY...
 */
'use strict';
const fs = require('node:fs');
const crypto = require('node:crypto');

const args = process.argv.slice(2);
const json = args[0] === '--json';
if (json) args.shift();
if (args.length < 2) {
    console.error('usage: langprofile-dump.cjs [--json] PROFILE KEY...');
    process.exit(2);
}
const profilePath = args[0];
const keys = args.slice(1);
const b = fs.readFileSync(profilePath);
if (b.length < 0x40 || b.subarray(0, 4).toString() !== 'EFSL')
    throw Error('not an EFSL profile');

function range(offset, size, end = b.length) {
    if (!Number.isSafeInteger(offset) || !Number.isSafeInteger(size) ||
        offset < 0 || size < 0 || offset > end || size > end - offset)
        throw Error(`out-of-range ${offset}+${size}`);
}
function word(p) { range(p, 4); return b.readUInt32LE(p); }
function hash(s) {
    if (!/^[\x01-\x7f]*$/.test(s)) throw Error('non-ASCII key: ' + s);
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
        if (!(next > p && next <= end + 20)) throw Error('bad profile entry');
        p = next;
    }
    throw Error('missing profile entry: ' + key);
}
const section = find(0x30, 'Languages', true);
const value = find(section, 'Dictionary', false);
const base = word(value + 12), length = word(value + 8), end = base + length;
range(base, length);
const languages = word(base), records = word(base + 4);
range(base + 8, records * 8, end);
let p = base + 8 + records * 8;
function string() {
    range(p, 1, end);
    const stop = b.indexOf(0, p);
    if (stop < p || stop >= end) throw Error('unterminated dictionary string');
    const start = p;
    p = stop + 1;
    return b.subarray(start, stop).toString('utf8');
}
const locales = [];
for (let i = 0; i < languages; ++i) locales.push([string(), string(), string()]);
const stringsBase = p;
const wanted = new Map();
for (const key of keys) wanted.set(hash(key), key);
const found = new Map();
let previous = 0;
for (let i = 0; i < records; ++i) {
    const entry = base + 8 + i * 8;
    const id = word(entry);
    if (i && id < previous) throw Error('unsorted dictionary');
    previous = id;
    if (!wanted.has(id)) continue;
    p = stringsBase + word(entry + 4);
    const values = [];
    for (let j = 0; j < languages; ++j) values.push(string());
    found.set(id, values);
}
const result = {};
for (const [id, key] of wanted) {
    const values = found.get(id);
    if (!values) throw Error('key not in dictionary: ' + key);
    result[key] = {};
    for (let i = 0; i < languages; ++i) result[key][locales[i][0]] = values[i];
}
if (json) {
    process.stdout.write(JSON.stringify(result, null, 2) + '\n');
} else {
    console.log(`profile=${profilePath} locales=${languages} records=${records} stringsBase=0x${stringsBase.toString(16)}`);
    for (const [id, key] of wanted) {
        console.log(`\n${key} hash=0x${id.toString(16)}`);
        const values = result[key];
        for (const locale of Object.keys(values))
            console.log(`  ${locale}=${JSON.stringify(values[locale])}`);
    }
}
