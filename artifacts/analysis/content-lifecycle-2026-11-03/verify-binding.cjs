const fs=require('fs'),path=require('path'),crypto=require('crypto'),assert=require('assert/strict');
const {execFileSync}=require('child_process');
const root=path.resolve(__dirname,'../../..');
const bytes=fs.readFileSync(path.join(root,'artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf'));
const md5=b=>crypto.createHash('md5').update(b).digest('hex');
assert.equal(md5(bytes),'64c252340d3af6c5d66072611b7bad7a');
function offset(address,length) {
    for(let i=0;i<bytes.readUInt16LE(44);++i) {
        const p=bytes.readUInt32LE(28)+i*32,base=bytes.readUInt32LE(p+8),size=bytes.readUInt32LE(p+16);
        if(bytes.readUInt32LE(p)===1 && address>=base && address-base<=size && length<=size-(address-base))
            return bytes.readUInt32LE(p+4)+address-base;
    }
    throw Error('unmapped source range');
}
const word=address=>bytes.readUInt32LE(offset(address,4));
function string(address) {
    const p=offset(address,1),end=bytes.indexOf(0,p);
    assert(end>=p && end-p<128);offset(address,end-p+1);return bytes.subarray(p,end).toString('ascii');
}
assert.equal(word(0xf5654c),0x1a1c718); // NvramStorage ctor's db_tables base
const table=string(word(0xf565b8));assert.equal(table,'HMCSettings'); // ctor writes db_tables[24], offset60
const names=[0xf553fc,0xf55410,0xf55418].map(p=>string(word(p)));
assert.deepEqual(names,['m_hotel_mode_availability','m_osd_menu_mode','m_pic_mode_availability']);
console.log('verified constructor-bound db_tables+60='+table+'; selected fields='+names.join(','));
const db=path.join(root,'artifacts/dumps/apdat_files/sqlite_db/settingsDB.db');
const before=md5(fs.readFileSync(db));assert.equal(before,'150e2be30e5948571860a8c60c9da525');
const result=execFileSync('sqlite3',['-readonly',db,'SELECT '+names.join(',')+' FROM '+table+' LIMIT 1'],{encoding:'utf8'});
assert.equal(result,'0|1|0\n');assert.equal(md5(fs.readFileSync(db)),before);
console.log('saved database '+before+' read-only row='+result.trim()+'; content hash unchanged');
