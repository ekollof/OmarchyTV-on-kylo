/* Real Unix-socket coverage for the persistent PVR module. A second process
 * uses the same pvr.path to prove records survive restart. */
const fs = require('fs'), os = require('os'), path = require('path'), net = require('net');
const { spawn } = require('child_process'), assert = require('assert/strict');

function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

async function session(build, config, socket) {
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stderr = '';
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-16384); });
    const closed = new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', (code, signal) => resolve({ code, signal }));
    });
    async function command(text) {
        return bounded(new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '', done = false;
            const finish = (error, value) => {
                if (done) return;
                done = true; s.destroy();
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text + '\n'));
            s.on('error', e => finish(e));
            s.on('end', () => finish(Error('EOF before reply')));
            s.on('data', d => {
                data += d;
                if (data.includes('\n')) finish(null, data.slice(0, data.indexOf('\n')));
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        }), 4000);
    }
    async function commandAll(text) {
        return bounded(new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '', done = false, idle = null;
            const finish = (error, value) => {
                if (done) return;
                done = true; if (idle) clearTimeout(idle); s.destroy();
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text + '\n'));
            s.on('error', e => finish(e));
            s.on('end', () => finish(null, data));
            s.on('data', d => {
                data += d;
                if (idle) clearTimeout(idle);
                idle = setTimeout(() => finish(null, data), 120);
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        }), 5000);
    }
    async function stop() {
        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
    }
    for (let i = 0; i < 300 && !fs.existsSync(socket); i++)
        await new Promise(r => setTimeout(r, 20));
    assert(fs.existsSync(socket), 'socket not created');
    return { command, commandAll, stop, closed };
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-pvr-ipc-'));
    const db = path.join(dir, 'pvr.db');
    const config = path.join(dir, 'config');
    fs.writeFileSync(config,
        `autoload=\nui.fps=0\nui.ok_file=\npvr.path=${db}\n`);

    let run = await session(build, config, path.join(dir, 'control1'));
    try {
        assert.equal(await run.command('load pvr'), 'ok pvr');
        assert.equal(await run.command('start pvr'), 'ok');
        assert.equal(await run.command('pvr-list'), 'count 0');

        assert.equal(await run.command('pvr-add 1 1000 2000'), 'id 1');
        assert.equal(await run.command('pvr-add 1 1500 2500'), 'failed errno 16');
        assert.equal(await run.command('pvr-add 2 3000 4000 127 30 -15'), 'id 2');
        assert.equal(await run.command('pvr-add 0 1000 2000'), 'failed errno 22');
        assert.equal(await run.command('pvr-add 1 2000 1000'), 'failed errno 22');
        let reply = await run.commandAll('pvr-list');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /job id=1 service=1 state=0 start=1000 end=2000 repeat=0 pad=0\/0 name=cli reservation/);
        assert.match(reply, /job id=2 service=2 state=0 start=3000 end=4000 repeat=127 pad=30\/-15 name=cli reservation/);

        assert.equal(await run.command('pvr-record 2'), 'ok');
        assert.equal(await run.command('pvr-record 2'), 'failed errno 114');
        assert.equal(await run.command('pvr-stop 2'), 'ok');
        assert.equal(await run.command('pvr-stop 2'), 'failed errno 114');
        assert.equal(await run.command('pvr-cancel 2'), 'failed errno 114');
        assert.equal(await run.command('pvr-record 99'), 'failed errno 2');
        assert.match(await run.commandAll('pvr-list'),
            /job id=2 service=2 state=2 start=3000/);

        assert.equal(await run.command('pvr-cancel 1'), 'ok');
        assert.equal(await run.command('pvr-cancel 1'), 'failed errno 114');
        assert.equal(await run.command('pvr-cancel 99'), 'failed errno 2');
        assert.match(await run.commandAll('pvr-list'),
            /job id=1 service=1 state=3 start=1000/);

        assert.equal(await run.command('reload pvr'), 'ok');
        reply = await run.commandAll('pvr-list');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /job id=1 .*state=3 /);
        assert.match(reply, /job id=2 .*state=2 /);

        assert.equal(await run.command('stop pvr'), 'ok');
        assert.equal(await run.command('pvr-list'), 'failed errno 19');
        assert.equal(await run.command('unload pvr'), 'ok');
        await run.stop();
    } finally {
        if (run && run.closed) {
            try { await bounded(run.closed, 1000); } catch { /* stopped above */ }
        }
    }

    /* A fresh process must see the same records and continue the id sequence. */
    run = await session(build, config, path.join(dir, 'control2'));
    try {
        assert.equal(await run.command('load pvr'), 'ok pvr');
        assert.equal(await run.command('start pvr'), 'ok');
        let reply = await run.commandAll('pvr-list');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /job id=1 service=1 state=3 start=1000 end=2000/);
        assert.match(reply, /job id=2 service=2 state=2 start=3000 end=4000 repeat=127 pad=30\/-15/);
        assert.equal(await run.command('pvr-add 1 5000 6000'), 'id 3');
        assert.match(await run.commandAll('pvr-list'), /^count 3\n/);
        await run.stop();
    } finally {
        if (run && run.closed) {
            try { await bounded(run.closed, 1000); } catch { /* stopped above */ }
        }
    }

    const file = fs.readFileSync(db, 'utf8');
    assert.match(file, /^OAPVR1\n/);
    assert.match(file, /\t3\tcli reservation\n/);
    fs.rmSync(dir, { recursive: true, force: true });
    console.log('ok pvr IPC: conflict add, record/stop states, reload and cross-process persistence');
}
main().catch(e => { console.error(e); process.exitCode = 1; });
