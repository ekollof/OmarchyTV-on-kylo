/* Local process/Unix IPC only. Optional second argument is ARM qemu sysroot. */
const fs=require('fs'),os=require('os'),path=require('path'),net=require('net');
const {spawn}=require('child_process'),assert=require('assert/strict');
function hash(s) {
    let h=(0xf1d3b597^(s.length<<6))>>>0,sum=0,rolling=0,previous=0;
    for(const c of Buffer.from(s)) {
        h=((h+2*~previous)^(sum<<9))>>>0;sum=(sum+c)>>>0;
        h=((h+(c<<17))^rolling)>>>0;rolling=(c+(rolling<<8))>>>0;
        h=(h+(h<<(c%3)))>>>0;h=(h+(h>>>(previous%5)))>>>0;previous=c;
    }
    return h;
}
function fixture(file,value) {
    const b=Buffer.alloc(168);b.write('EFSL');b.writeUInt32LE(5,0x38);
    const names=['AudioSettings','Prescales','DefaultSpeaker','12Watt','Amp_Prescale'];
    const types=[1,1,5,1,5],counts=[4,3,0,1,0];
    names.forEach((name,i)=>{
        const p=0x44+20*i;b.writeUInt32LE(hash(name),p);b[p+4]=types[i];
        b.writeUInt32LE(counts[i],p+8);b.writeUInt32LE(i===2?3:i===4?value:0,p+12);
    });
    fs.writeFileSync(file,b);
}
function bounded(p,ms) {
    return new Promise((resolve,reject)=>{
        const t=setTimeout(()=>reject(Error('timeout')),ms);
        p.then(v=>{clearTimeout(t);resolve(v);},e=>{clearTimeout(t);reject(e);});
    });
}
async function main() {
    const build=path.resolve(process.argv[2]),dir=fs.mkdtempSync(path.join(os.tmpdir(),'oa-amp-ipc-'));
    const file=path.join(dir,'profile'),config=path.join(dir,'config'),socket=path.join(dir,'control');
    fixture(file,17);fs.writeFileSync(config,'autoload=audio_amp_profile\namp_profile.hwprofile='+file+'\n');
    const args=[path.join(build,'openaurora'),'--modules',path.join(build,'modules'),'--config',config,'--sock',socket,'--log','0'];
    const qemu=process.argv[3];if(qemu)args.unshift('-L',path.resolve(qemu));
    const exe=qemu?'qemu-arm-static':args.shift();
    const child=spawn(exe,args,{stdio:['ignore','pipe','pipe']});let stderr='';
    child.stdout.resume();child.stderr.on('data',b=>{stderr+=b;});
    const closed=new Promise((resolve,reject)=>{child.on('error',reject);child.on('close',(code,signal)=>resolve({code,signal}));});
    try {
        for(let i=0;i<150&&!fs.existsSync(socket);i++)await new Promise(r=>setTimeout(r,20));
        assert(fs.existsSync(socket),stderr);
        async function command(text) {
            return bounded(new Promise((resolve,reject)=>{
                const s=net.createConnection(socket);let data='',done=false;
                function finish(error,value){if(done)return;done=true;s.destroy();error?reject(error):resolve(value);}
                s.on('connect',()=>s.write(text+'\n'));s.on('error',finish);
                s.on('end',()=>finish(Error('EOF before reply')));
                s.on('data',b=>{data+=b;const n=data.indexOf('\n');if(n>=0)finish(null,data.slice(0,n));});
                s.setTimeout(2000,()=>finish(Error('reply timeout')));
            }),3000);
        }
        const query=async value=>assert.match(await command('amp-profile'),new RegExp("^power '12Watt' prescale "+value+' ch1 192 ch2 192 fine 192 '));
        await query(17);fixture(file,31);await query(17);
        assert.equal(await command('amp-profile extra'),'failed errno '+os.constants.errno.EINVAL);
        assert.equal(await command('amp-profile --force'),'failed errno '+os.constants.errno.EINVAL);
        assert.equal(await command('reload audio_amp_profile'),'ok');await query(31);
        assert.equal(await command('suspend'),'ok');fixture(file,63);await query(31);
        assert.equal(await command('reload audio_amp_profile'),'ok');
        assert.equal(await command('amp-profile'),'failed errno '+os.constants.errno.ENODEV);
        assert.equal(await command('resume'),'ok');await query(63);
        fs.unlinkSync(file);assert.match(await command('reload audio_amp_profile'),/^failed/);
        assert.equal(await command('amp-profile'),'failed errno '+os.constants.errno.ENODEV);
        fixture(file,79);assert.equal(await command('load audio_amp_profile'),'ok audio_amp_profile');
        assert.equal(await command('start audio_amp_profile'),'ok');await query(79);
        const corpus=path.resolve(__dirname,'../../artifacts/profiles/mb230_hwprofile.bin');
        if(fs.existsSync(corpus)) {
            fs.copyFileSync(corpus,file);assert.equal(await command('reload audio_amp_profile'),'ok');
            assert.match(await command('amp-profile'),/^power '12Watt' prescale 27 ch1 5 ch2 5 fine 128 /);
            console.log('ok amp profile Unix IPC: saved target hwprofile passes through the real module');
        }
        child.kill('SIGTERM');const exit=await bounded(closed,5000);
        assert.equal(exit.code,0,stderr);assert(!fs.existsSync(socket));
        console.log('ok amp profile Unix IPC: query validation, immutable snapshot, reload/suspend/resume, failed replacement recovery and clean exit');
    } finally {
        if(child.exitCode===null&&!child.signalCode)child.kill('SIGTERM');
        try{await bounded(closed,5000);}catch{child.kill('SIGKILL');await closed;}
        fs.rmSync(dir,{recursive:true,force:true});
    }
}
main().catch(e=>{console.error(e);process.exitCode=1;});
