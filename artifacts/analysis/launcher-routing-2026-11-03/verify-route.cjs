/* Independent corpus: shipped table + fresh decompile vs maintained C output.
 * Input is tests/launcher_route --dump, optionally from ARM/qemu. */
const fs=require('fs'),assert=require('assert/strict'),crypto=require('crypto');
const dir='artifacts/analysis/launcher-routing-2026-11-03/';
const elf=fs.readFileSync('artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf');
assert.equal(crypto.createHash('md5').update(elf).digest('hex'),
             '64c252340d3af6c5d66072611b7bad7a');
function offset(address) {
    for(let i=0;i<elf.readUInt16LE(44);++i) {
        const p=elf.readUInt32LE(28)+i*elf.readUInt16LE(42);
        const base=elf.readUInt32LE(p+8),n=elf.readUInt32LE(p+16);
        if(elf.readUInt32LE(p)===1 && address>=base && address<base+n)
            return elf.readUInt32LE(p+4)+address-base;
    }
    throw Error('unmapped address');
}
const dump=fs.readFileSync(process.argv[2]||dir+'host-corpus.txt','utf8');
const ids=[...dump.matchAll(/^id (\d+) (-?\d+)$/gm)];assert.equal(ids.length,62);
const table=elf.readUInt32LE(offset(0xe4ae4c));assert.equal(table,0x14efd84);
for(let i=0;i<62;++i)assert.deepEqual(ids[i].slice(1).map(Number),[i,elf.readInt16LE(offset(table)+2*i)]);
const raw=fs.readFileSync(dir+'selection-operation.txt','utf8');
assert(raw.includes('MD5 64c252340d3af6c5d66072611b7bad7a'));
const start=raw.indexOf('LAB_00d3ddc8:'),end=raw.indexOf('\n (GhidraScript)',start);
assert(start>=0 && end>start);const source=raw.slice(start,end);
const labels=[...source.matchAll(/^  case (0x[0-9a-f]+|\d+):/gm)];
const expected=new Map();let carry='';
for(let i=labels.length-1;i>=0;--i) {
    let body=source.slice(labels[i].index+labels[i][0].length,
                          i+1<labels.length?labels[i+1].index:source.length);
    if(body.trim())carry=body;else body=carry;
    const id=Number(labels[i][1]);
    if(id===0x4b || id===0xa6)continue; // additional portal/runtime branch work
    const commands=[];
    for(const m of body.matchAll(/\b(setOpenSubMenuOfTabDialog|setTabDialogLastSelectedIndex|setSettingsListMenuLastSelectedItem|DialogNavigationMessage::DialogNavigationMessage)\s*\(([\s\S]*?)\);/g)) {
        const value=m[2].match(/,(true|0x[0-9a-f]+|\d+|param_1)\s*$/);assert(value,m[0]);
        const kinds={setOpenSubMenuOfTabDialog:3,setTabDialogLastSelectedIndex:1,
            setSettingsListMenuLastSelectedItem:2,'DialogNavigationMessage::DialogNavigationMessage':4};
        commands.push([kinds[m[1]],value[1]==='param_1'?id:value[1]==='true'?1:Number(value[1])]);
    }
    if(!commands.some(([kind,value])=>kind===4 && value===0x13))continue;
    assert(!/\bif\s*\(/.test(body),'unmodeled guard '+labels[i][1]);
    expected.set(id,commands.map(c=>c.join(':')).join(' '));
}
assert.equal(expected.size,28);
const plans=[...dump.matchAll(/^plan (\d+) (.+)$/gm)];assert.equal(plans.length,expected.size);
for(const p of plans)assert.equal(p[2],expected.get(Number(p[1])),p[1]);
console.log('PASS:all62 signed operation results match shipped table14efd84;28 ordered settings plans match fresh firmware calls.');
console.log('No logging/telemetry emitted. Modal/provider/portal work, guarded branches and UI integration remain separate.');
