/* Read shipped shader pointers through ELF PT_LOAD mappings; never use a
 * blanket VA subtraction for writable data. Vendor strings remain local. */
const fs=require('fs'),crypto=require('crypto');
const file=process.argv[2]||'artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf';
const b=fs.readFileSync(file);
if(crypto.createHash('md5').update(b).digest('hex')!=='64c252340d3af6c5d66072611b7bad7a')
    throw Error('unexpected source ELF');
function offset(address) {
    for(let i=0;i<b.readUInt16LE(44);++i) {
        const p=b.readUInt32LE(28)+i*b.readUInt16LE(42);
        const base=b.readUInt32LE(p+8),size=b.readUInt32LE(p+16);
        if(b.readUInt32LE(p)===1 && address>=base && address<base+size)
            return b.readUInt32LE(p+4)+address-base;
    }
    throw Error('unmapped address');
}
for(const [name,address] of [['fragment_font',0x178b8bc],['vertex_texture',0x178b8c0],
                            ['fragment_texture',0x178b8c4]]) {
    const value=b.readUInt32LE(offset(address)),p=offset(value),end=b.indexOf(0,p);
    if(end<0 || end-p>8192)throw Error('unterminated shader');
    console.log(`${name} pointer@${address.toString(16)} -> ${value.toString(16)}\n${b.subarray(p,end).toString()}`.trimEnd());
}
