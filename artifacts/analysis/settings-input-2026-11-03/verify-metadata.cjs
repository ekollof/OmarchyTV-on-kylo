/* Bounded local source verification. No TV/device access. */
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const file = path.resolve(__dirname, '../../workbench/rootfs/vendor/com.nvt.idtv/aurora.elf');
const data = fs.readFileSync(file);
assert.equal(crypto.createHash('md5').update(data).digest('hex'), '64c252340d3af6c5d66072611b7bad7a');
assert.equal(data.toString('ascii', 0, 4), '\x7fELF');
assert.equal(data[4], 1); assert.equal(data[5], 1); assert.equal(data.readUInt16LE(18), 40);
function bytes(address, size) {
  const start = data.readUInt32LE(28), stride = data.readUInt16LE(42), count = data.readUInt16LE(44);
  assert.equal(stride, 32); assert(start + stride * count <= data.length);
  for (let i = 0; i < count; ++i) {
    const p = start + stride * i, v = data.readUInt32LE(p + 8), n = data.readUInt32LE(p + 16);
    if (data.readUInt32LE(p) !== 1 || address < v || address + size > v + n) continue;
    const offset = data.readUInt32LE(p + 4) + address - v;
    assert(offset + size <= data.length); return data.subarray(offset, offset + size);
  }
  throw new Error(`unmapped source address ${address.toString(16)}`);
}
assert.equal(bytes(0xd56c90, 4).readUInt32LE(), 0x14d4d52);
assert.equal(bytes(0xd57234, 4).readUInt32LE(), 0x14d4d52);
assert.equal(bytes(0x14d4d52, 6).toString('ascii'), 'BLANK\0');
assert.equal(bytes(0x14dbea4 + 8 + 0x84, 4).readUInt32LE(), 0x6b14ac);
assert.equal(bytes(0x14ddb7c + 8 + 0x84, 4).readUInt32LE(), 0xd6eb09);
console.log('ok shipped metadata: both description literals point to14d4d52 BLANK; primary+84 Settings=6b14ac pure-virtual PLT, Tab=d6eb09 updateSettings');
