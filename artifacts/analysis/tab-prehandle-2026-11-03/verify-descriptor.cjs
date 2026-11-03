const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert/strict');
const root=path.resolve(__dirname,'../../..');
const bytes=fs.readFileSync(path.join(root,'artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf'));
assert.equal(crypto.createHash('md5').update(bytes).digest('hex'),'64c252340d3af6c5d66072611b7bad7a');
function offset(address,length) {
    for(let i=0;i<bytes.readUInt16LE(44);++i) {
        const p=bytes.readUInt32LE(28)+i*32,base=bytes.readUInt32LE(p+8),size=bytes.readUInt32LE(p+16);
        if(bytes.readUInt32LE(p)===1 && address>=base && address-base<=size && length<=size-(address-base))
            return bytes.readUInt32LE(p+4)+address-base;
    }
    throw Error('unmapped source range');
}
const address=bytes.readUInt32LE(offset(0xd703c8,4));
assert.equal(address,0x14d9097);
const at=offset(address,14);assert.equal(bytes.subarray(at,at+14).toString('ascii'),'vertical_tabs\0');
console.log('verified pre-handler description literal d703c8 ->14d9097: vertical_tabs');
