/* Offline check of saved non-atomic context observations and shipped vtables.
 * This proves configured dispatch, not that a particular frame executed it. */
const fs=require('fs'),assert=require('assert/strict'),crypto=require('crypto');
const base='artifacts/workbench/title-horizontal-20261103/';
const elf=fs.readFileSync('artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf');
assert.equal(crypto.createHash('md5').update(elf).digest('hex'),
             '64c252340d3af6c5d66072611b7bad7a');
function word(address) {
    for(let i=0;i<elf.readUInt16LE(44);++i) {
        const p=elf.readUInt32LE(28)+i*elf.readUInt16LE(42);
        const v=elf.readUInt32LE(p+8),n=elf.readUInt32LE(p+16);
        if(elf.readUInt32LE(p)===1 && address>=v && address+4<=v+n)
            return elf.readUInt32LE(elf.readUInt32LE(p+4)+address-v);
    }
    throw Error('unmapped vtable');
}
const draw=fs.readFileSync(base+'draw-context-header.bin');
assert.equal(draw.length,44);assert.equal(draw.readUInt32LE(0),0x13f9404);
assert.equal(word(0x13f9404+0xc),0xb43681); // getGraphicsContext returns object+28.
assert.equal(draw.readUInt32LE(0x28),0x19f7ff8);
const gl=fs.readFileSync(base+'gl-singleton.bin');
assert.equal(gl.length,60);assert.equal(gl.readUInt32LE(0),0x13f8800);
assert.equal(word(0x13f8800+0x48),0xb34671); // native coverage drawText.
assert.deepEqual([gl.readUInt32LE(4),gl.readUInt32LE(8)],[1920,1080]);
assert.equal(gl.readUInt32LE(0x30),8);assert.equal(gl[0x38],1);
console.log('PASS saved context: nonnull GPU getter + vtable drawText slot; dimensions1920x1080 font program8 initialized1');
console.log('Dispatch branch ARM:1131bbe..1131bfa; frame execution and current object lifetime are not observed.');
