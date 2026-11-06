/* ABI 3 async quiesce for mod_pvr: unpublish pvr synchronously, then close the
 * persistent backend synchronously or on a worker. The reload must reopen the
 * store and see the persisted reservations. */
const assert = require('assert/strict');
const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');

function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'modules', 'mod_pvr.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-pvr-quiesce-'));
    const db = path.join(dir, 'pvr.db');
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_pvr.so'));
    fs.writeFileSync(config,
        `autoload=\npvr.path=${db}\n` +
        'pvr.cleanup_delay_ms=400\n');

    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-65536); });
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-65536); });
    const closed = new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', (code, signal) => resolve({ code, signal }));
    });
    const cleanup = () => {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        fs.rmSync(dir, { recursive: true, force: true });
    };

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

    async function timed(text) {
        const started = Date.now();
        const reply = await command(text);
        return { reply, elapsed: Date.now() - started };
    }

    try {
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                if (fs.existsSync(socket)) { clearInterval(timer); resolve(); }
                else if (Date.now() - started > 4000) {
                    clearInterval(timer); reject(Error('socket timeout'));
                }
            }, 20);
        }), 5000);
        assert.equal(await command('load pvr'), 'ok pvr');
        assert.equal(await command('start pvr'), 'ok');
        assert.equal(await command('pvr-list'), 'count 0');
        assert.equal(await command('pvr-add 1 1000 2000'), 'id 1');
        assert.equal(await command('pvr-add 2 3000 4000'), 'id 2');

        const reload = await timed('reload pvr');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status pvr');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('pvr-list'), 'failed errno 19',
            'service must be unpublished while the backend closes');

        await new Promise(resolve => setTimeout(resolve, 600));
        const persisted = fs.readFileSync(db, 'utf8');
        assert.match(persisted, /^OAPVR1\n/);
        assert.equal(persisted.trim().split('\n').length, 3);

        assert.equal(await command('reload pvr'), 'ok',
            'retry must complete the reload');
        let reply = await commandAll('pvr-list');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /job id=1 service=1 state=0 start=1000 end=2000/);
        assert.match(reply, /job id=2 service=2 state=0 start=3000 end=4000/);

        assert.equal(await command('pvr-add 1 5000 6000'), 'id 3',
            'reopened backend must continue the persisted id sequence');
        assert.equal(await command('pvr-record 3'), 'ok');
        assert.equal(await command('pvr-stop 3'), 'ok');

        const stop = await timed('stop pvr');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('pvr-list'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop pvr'), 'ok');
        assert.equal(await command('unload pvr'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started pvr/);
        console.log(`ok pvr quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'store republished, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
