/* Offline checks of the subsequent agent's evidence. No TV connection. */
const fs=require('fs'),assert=require('assert/strict'),crypto=require('crypto');
const root='artifacts/analysis/agent-work-review-2026-11-03/';
const elf=fs.readFileSync('artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf');
assert.equal(crypto.createHash('md5').update(elf).digest('hex'),
             '64c252340d3af6c5d66072611b7bad7a');
function word(address) {
    for(let i=0;i<elf.readUInt16LE(44);++i) {
        const p=elf.readUInt32LE(28)+i*elf.readUInt16LE(42);
        const base=elf.readUInt32LE(p+8),size=elf.readUInt32LE(p+16);
        if(elf.readUInt32LE(p)===1 && address>=base && address+4<=base+size)
            return elf.readUInt32LE(elf.readUInt32LE(p+4)+address-base);
    }
    throw Error('unmapped ELF address');
}
assert.equal(word(0x1500cfc+8+0x10),0xee53a9); // SubLauncherDialog::setFocus
assert.equal(word(0x1500cfc+8+0x18),0xedf1f1); // SubLauncherDialog::isShowing
const log=fs.readFileSync(root+'navigation-dispatch.txt','utf8');
assert(log.includes('MD5 64c252340d3af6c5d66072611b7bad7a'));
const start=log.indexOf('DialogNavigator::handleNavigationMessage(DialogNavigator *this,');
const end=log.indexOf('\n (GhidraScript)',start);assert(start>=0 && end>start);
const source=log.slice(start,end),cases=[...source.matchAll(/^  case (0x[0-9a-f]+|\d+):/gm)];
const table=fs.readFileSync('artifacts/analysis/ui-menu-flow-2026-11-04/nav-routing.txt','utf8');
const rows=[...table.matchAll(/^(0x[0-9a-f]+|\d+) -> (.+)$/gm)];
const expected=new Map(rows.map(m=>[Number(m[1]),m[2]]));
assert.equal(rows.length,154);assert.equal(expected.size,154);assert.equal(cases.length,154);
for(let i=0;i<cases.length;++i) {
    const body=source.slice(cases[i].index+cases[i][0].length,
                            i+1<cases.length?cases[i+1].index:source.length);
    const calls=[...new Set([...body.matchAll(/\b((?:doNavigate|handle)\w*|doModal)\s*\(/g)].map(m=>m[1]))];
    assert.equal(calls.join(', ')||'none',expected.get(Number(cases[i][1])),cases[i][1]);
}
function xml(name) {
    return new Map([...fs.readFileSync('artifacts/workbench/rootfs/vendor/'+name,'utf8')
        .matchAll(/KeyString="([^"]+)" KeyValue="(\d+)"/g)].map(m=>[m[1],Number(m[2])]));
}
const vk=xml('vk_keymapping_db.xml'),ice=xml('ice_keymapping_db.xml');
assert.deepEqual([vk.get('KeyUp'),ice.get('KeyUp')],[1020,522]);
assert.deepEqual([vk.get('KeyQuickMenu'),ice.get('KeyQuickMenu')],[1043,593]);
assert.equal(593,0x251);assert.notEqual(vk.get('KeyQuickMenu'),ice.get('KeyQuickMenu'));
console.log('PASS:154 case-local ordered navigation/handle/modal call lists match fresh decompile.');
console.log('PASS:SubLauncher virtual slots setFocus/isShowing and distinct VK/ICE Up/QuickMenu values.');
console.log('Scope:case-local syntactic calls are not complete goto/delegate control flow; XML names do not prove live translation.');
