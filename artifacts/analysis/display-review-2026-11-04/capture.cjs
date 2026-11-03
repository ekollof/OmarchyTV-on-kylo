/* Capture held frames from the real TV; the probe owns drawing/restoration. */
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process');
const root=path.resolve(__dirname,'../../..'),log=path.join(__dirname,'tv-runtime.txt');
const wanted=['normal','focus0','focus1','focus2','focus3','focus4','focus7'],captured=new Set();
const deadline=Date.now()+60000;
(async()=>{
  while(Date.now()<deadline && captured.size<wanted.length) {
    const text=fs.existsSync(log)?fs.readFileSync(log,'utf8'):'';
    const matches=[...text.matchAll(/SETTINGS phase=(\w+) serial=/g)];
    const phase=matches.at(-1)?.[1];
    if(phase && !captured.has(phase)) {
      if(phase!==wanted[captured.size])throw Error(`missed screenshot phase: ${phase}`);
      const out=path.join(__dirname,`${phase}.png`);
      const r=cp.spawnSync(path.join(root,'tools/tvshot.py'),['--no-wake','--layer','1','--frames','2','--out',out],{encoding:'utf8',timeout:5000});
      fs.writeFileSync(path.join(__dirname,`${phase}-capture.txt`),(r.stdout||'')+(r.stderr||''));
      if(r.error || r.status!==0)throw Error(`capture ${phase}: ${r.error||r.status}`);
      captured.add(phase);process.stdout.write(`captured ${phase}\n`);
    }
    await new Promise(resolve=>setTimeout(resolve,100));
  }
  if(captured.size!==wanted.length)throw Error(`only captured ${captured.size} phases`);
})().catch(e=>{process.stderr.write(`${e.stack}\n`);process.exitCode=1;});
