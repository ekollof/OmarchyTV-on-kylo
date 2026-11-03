/* Actual local Unix-socket control test. Starts only ui_navigation, never a
 * display/input/HAL module. No TV/network connection or persistent files. */
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
    const dir=fs.mkdtempSync(path.join(os.tmpdir(),'oa-navigation-ipc-'));
    const config=path.join(dir,'config'),socket=path.join(dir,'control.sock');
    fs.writeFileSync(config,'autoload=\n');
    const child=spawn(path.join(build,'openaurora'),['--modules',path.join(build,'modules'),
        '--config',config,'--sock',socket,'--load','ui_navigation','--log','0'],{stdio:['pipe','pipe','pipe']});
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
        assert.equal(await command('nav-state'),'tab 0 item 0 submenu 0 pending 0');
        assert.equal(await command('nav-result 109'),'queued');
        assert.equal(await command('nav-operation 6'),'queued');
        assert.equal(await command('nav-state'),'tab 2 item 109 submenu 1 pending 2');
        assert.equal(await command('reload ui_navigation'),'ok');
        assert.equal(await command('nav-state'),'tab 2 item 109 submenu 1 pending 2');
        assert.equal(await command('suspend'),'ok');
        assert.equal(await command('nav-operation 2'),'failed errno '+os.constants.errno.EBUSY);
        assert.equal(await command('reload ui_navigation'),'ok');
        assert.equal(await command('nav-state'),'failed errno '+os.constants.errno.ENODEV);
        assert.equal(await command('resume'),'ok');
        assert.equal(await command('nav-take'),'navigation 19 tab 2 item 109 submenu 1 pending 1');
        assert.equal(await command('nav-take'),'navigation 19 tab 2 item 109 submenu 1 pending 0');
        assert.equal(await command('nav-take'),'failed errno '+os.constants.errno.EAGAIN);
        assert.equal(await command('nav-result -1'),'failed errno '+os.constants.errno.ENOTSUP);
        child.kill('SIGTERM');const exit=await bounded(closed,5000);
        assert.equal(exit.code,0,stderr);assert(!fs.existsSync(socket));
        console.log('ok actual Unix IPC: queued modal/operation state, reload, standby/deferred activation, destructive take and clean exit');
    } finally {
        if(child.exitCode===null && !child.signalCode)child.kill('SIGTERM');
        try { await bounded(closed,5000); } catch {child.kill('SIGKILL');await closed;}
        fs.rmSync(dir,{recursive:true,force:true});
    }
}
main().catch(e=>{console.error(e);process.exitCode=1;});
