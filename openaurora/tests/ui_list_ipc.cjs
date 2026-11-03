/* Actual local Unix-socket control test: load ui_list, drive verified SVG-list
 * keys, reload with state handoff, suspend/resume, clean exit. No TV/network
 * connection, no display/input/HAL modules, no persistent files. */
const fs=require('fs'),os=require('os'),path=require('path'),net=require('net');
const {spawn}=require('child_process'),assert=require('assert/strict');
function bounded(promise,ms) {
    return new Promise((resolve,reject)=>{
        const timer=setTimeout(()=>reject(Error('test timeout')),ms);
        promise.then(v=>{clearTimeout(timer);resolve(v);},e=>{clearTimeout(timer);reject(e);});
    });
}
async function main() {
    const build=path.resolve(process.argv[2]||'openaurora/build');
    const dir=fs.mkdtempSync(path.join(os.tmpdir(),'oa-list-ipc-'));
    const config=path.join(dir,'config'),socket=path.join(dir,'control.sock');
    fs.writeFileSync(config,'autoload=\n');
    const child=spawn(path.join(build,'openaurora'),['--modules',path.join(build,'modules'),
        '--config',config,'--sock',socket,'--load','ui_list','--log','0'],{stdio:['pipe','pipe','pipe']});
    const closed=new Promise(resolve=>child.once('close',(code,signal)=>resolve({code,signal})));
    let stdout='',stderr='';
    child.stdout.on('data',d=>{stdout=(stdout+d).slice(-16384);});
    child.stderr.on('data',d=>{stderr=(stderr+d).slice(-16384);});
    try {
        await bounded(new Promise((resolve,reject)=>{
            child.once('error',reject);child.once('exit',()=>reject(Error('early exit: '+stderr)));
            child.stdout.on('data',()=>{if(stdout.includes('openaurora:'))resolve();});
        }),5000);
        async function command(text) {
            return bounded(new Promise((resolve,reject)=>{
                const s=net.createConnection(socket);let data='',done=false;
                const finish=(error,value)=>{
                    if(done)return;done=true;s.destroy();error?reject(error):resolve(value);
                };
                s.on('connect',()=>s.write(text+'\n'));
                s.on('error',e=>finish(e));s.on('end',()=>finish(Error('EOF before reply')));
                s.on('data',d=>{data+=d;if(data.includes('\n'))finish(null,data.slice(0,data.indexOf('\n')));});
                s.setTimeout(2000,()=>finish(Error('socket timeout')));
            }),3000);
        }
        const EBUSY=os.constants.errno.EBUSY,ENODEV=os.constants.errno.ENODEV,EINVAL=os.constants.errno.EINVAL;
        assert.equal(await command('list-state'),'first 0 size 5 selected 0 total 9 handled -1 anim 0 scroll 0 partial 0 focus 0 group 0');
        assert.equal(await command('list-key 523'),'handled 1');
        assert.equal(await command('list-state'),'first 0 size 5 selected 1 total 9 handled 1 anim 1 scroll 0 partial 0 focus 2 group 0');
        assert.equal(await command('reload ui_list'),'ok');
        assert.equal(await command('list-state'),'first 0 size 5 selected 1 total 9 handled 1 anim 1 scroll 0 partial 0 focus 2 group 0');
        assert.equal(await command('list-key 732'),'handled 1');
        assert.equal(await command('list-state'),'first 4 size 5 selected 4 total 9 handled 1 anim 1 scroll 0 partial 0 focus 2 group 1');
        assert.equal(await command('suspend'),'ok');
        assert.equal(await command('list-key 523'),'failed errno '+EBUSY);
        assert.equal(await command('list-state'),'first 4 size 5 selected 4 total 9 handled 1 anim 1 scroll 0 partial 0 focus 2 group 1');
        assert.equal(await command('reload ui_list'),'ok');
        assert.equal(await command('list-state'),'failed errno '+ENODEV);
        assert.equal(await command('resume'),'ok');
        assert.equal(await command('list-key 731'),'handled 1');
        assert.equal(await command('list-state'),'first 0 size 5 selected 0 total 9 handled 1 anim 1 scroll 0 partial 0 focus 2 group 2');
        assert.equal(await command('list-key 4660'),'handled 0');
        assert.equal(await command('list-state'),'first 0 size 5 selected 0 total 9 handled 0 anim 1 scroll 0 partial 0 focus 2 group 2');
        assert.equal(await command('list-key'),'failed errno '+EINVAL);
        assert.equal(await command('list-key 4294967296'),'failed errno '+EINVAL);
        assert.equal(await command('list-state extra'),'failed errno '+EINVAL);
        child.kill('SIGTERM');const exit=await bounded(closed,5000);
        assert.equal(exit.code,0,stderr);assert(!fs.existsSync(socket));
        console.log('ok actual Unix IPC: list keys/selection, reload handoff, standby gate and clean exit');
    } finally {
        if(child.exitCode===null && !child.signalCode)child.kill('SIGTERM');
        try { await bounded(closed,5000); } catch {child.kill('SIGKILL');await closed;}
        fs.rmSync(dir,{recursive:true,force:true});
    }
}
main().catch(e=>{console.error(e);process.exitCode=1;});
