/* Actual Unix IPC with only the local settings module and an owned fixture DB. */
const fs=require('fs'),os=require('os'),path=require('path'),net=require('net');
const {spawn,execFileSync}=require('child_process'),assert=require('assert/strict');
function bounded(promise,ms) {
    return new Promise((resolve,reject)=>{
        const timer=setTimeout(()=>reject(Error('test timeout')),ms);
        promise.then(v=>{clearTimeout(timer);resolve(v);},e=>{clearTimeout(timer);reject(e);});
    });
}
async function main() {
    const build=path.resolve(process.argv[2]||'openaurora/build');
    const dir=fs.mkdtempSync(path.join(os.tmpdir(),'oa-hotel-ipc-'));
    const db=path.join(dir,'settings.db'),config=path.join(dir,'config'),socket=path.join(dir,'control');
    const sql=text=>execFileSync('sqlite3',[db,text]);
    sql('CREATE TABLE HMCSettings(m_hotel_mode_availability INTEGER,m_osd_menu_mode INTEGER,m_pic_mode_availability INTEGER); INSERT INTO HMCSettings VALUES(0,1,0)');
    fs.writeFileSync(config,'autoload=settings\nsettings.db='+db+'\nsettings.readonly=1\n');
    const child=spawn(path.join(build,'openaurora'),['--modules',path.join(build,'modules'),'--config',config,'--sock',socket,'--log','0'],{stdio:['ignore','pipe','pipe']});
    let stderr='';child.stdout.resume();child.stderr.on('data',b=>{stderr+=b;});
    const closed=new Promise((resolve,reject)=>{child.on('error',reject);child.on('close',(code,signal)=>resolve({code,signal}));});
    try {
        for(let i=0;i<100 && !fs.existsSync(socket);++i)await new Promise(r=>setTimeout(r,20));
        assert(fs.existsSync(socket),stderr);
        async function command(text) {
            return bounded(new Promise((resolve,reject)=>{
                const s=net.createConnection(socket);let data='',done=false;
                const finish=(error,value)=>{if(done)return;done=true;s.destroy();error?reject(error):resolve(value);};
                s.on('connect',()=>s.write(text+'\n'));s.on('error',e=>finish(e));s.on('end',()=>finish(Error('EOF before reply')));
                s.on('data',d=>{data+=d;if(data.includes('\n'))finish(null,data.slice(0,data.indexOf('\n')));});
                s.setTimeout(2000,()=>finish(Error('socket timeout')));
            }),3000);
        }
        assert.equal(await command('hotel-state'),'hotel 0 osd 1 picture 0');
        assert.equal(await command('hotel-state extra'),'failed errno '+os.constants.errno.EINVAL);
        assert.equal(await command('hotel-state --force'),'failed errno '+os.constants.errno.EINVAL);
        sql('UPDATE HMCSettings SET m_hotel_mode_availability=-1,m_osd_menu_mode=-1,m_pic_mode_availability=258');
        assert.equal(await command('hotel-state'),'hotel 255 osd 4294967295 picture 2');
        assert.equal(await command('reload settings'),'ok');
        assert.equal(await command('hotel-state'),'hotel 255 osd 4294967295 picture 2');
        sql('DELETE FROM HMCSettings');
        assert.equal(await command('hotel-state'),'failed errno '+os.constants.errno.ENODATA);
        assert.equal(await command('stop settings'),'ok');
        assert.equal(await command('hotel-state'),'failed errno '+os.constants.errno.ENODEV);
        child.kill('SIGTERM');const exit=await bounded(closed,5000);
        assert.equal(exit.code,0,stderr);assert(!fs.existsSync(socket));
        console.log('ok actual hotel Unix IPC: persisted fields, narrowing, strict query, live fixture changes, reload/error/unavailability and clean exit');
    } finally {
        if(child.exitCode===null && !child.signalCode)child.kill('SIGTERM');
        try {await bounded(closed,5000);}catch{child.kill('SIGKILL');await closed;}
        fs.rmSync(dir,{recursive:true,force:true});
    }
}
main().catch(e=>{console.error(e);process.exitCode=1;});
