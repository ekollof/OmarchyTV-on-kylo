/* ABI 3 async quiesce for mod_tuner: unpublish tuner synchronously, stop
 * any scan, then close the TS FILE* synchronously or on a worker. The TS
 * replay source makes the file descriptor observable in /proc. */
const assert = require('assert/strict');
const { spawn, execFileSync } = require('child_process');
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
    const fixture = path.join(build, 'tests', 'eit_ts_fixture');
    const module = path.join(build, 'modules', 'mod_tuner.so');
    for (const f of [fixture, module]) assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-tuner-quiesce-'));
    const ts = path.join(dir, 'eit.ts');
    execFileSync(fixture, [ts]);
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_tuner.so'));
    fs.writeFileSync(config,
        `autoload=\ntuner.ts_path=${ts}\n` +
        'tuner.cleanup_delay_ms=400\n');

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

    async function timed(text) {
        const started = Date.now();
        const reply = await command(text);
        return { reply, elapsed: Date.now() - started };
    }

    function ts_fds() {
        const dirfd = `/proc/${child.pid}/fd`;
        const out = [];
        for (const fd of fs.readdirSync(dirfd)) {
            try {
                if (fs.readlinkSync(path.join(dirfd, fd)) === ts) out.push(fd);
            } catch {}
        }
        return out;
    }

    const initial = 'network 0 state 0 progress 0 lock 0 services 0 selected 0 error 0 ';
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
        assert.equal(await command('load tuner'), 'ok tuner');
        assert.equal(await command('start tuner'), 'ok');
        assert.match(await command('tuner-state'), new RegExp('^' + initial));
        assert.equal(ts_fds().length, 1, 'TS replay fd must be open');

        const first = await timed('reload tuner');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await timed('status tuner');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('tuner-state'), 'failed errno 19',
            'service must be unpublished while the TS worker runs');

        await new Promise(resolve => setTimeout(resolve, 500));
        assert.equal(ts_fds().length, 0,
            'worker must close the old TS fd before the reload retry');
        await new Promise(resolve => setTimeout(resolve, 200));
        assert.equal(await command('reload tuner'), 'ok',
            'retry must complete the reload');
        assert.match(await command('tuner-state'), new RegExp('^' + initial));
        assert.equal(ts_fds().length, 1, 'reloaded tuner must reopen the TS source');

        assert.equal(await command('tuner-scan 1'), 'ok');
        for (let i = 0; i < 10; i++) {
            const state = await command('tuner-state');
            if (/ state 2 /.test(state)) break;
            assert.match(state, / state 1 /, state);
        }
        assert.match(await command('tuner-state'), / state 2 /);
        assert.equal(ts_fds().length, 1);

        const stop = await timed('stop tuner');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('tuner-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 500));
        assert.equal(ts_fds().length, 0,
            'stop worker must close the TS fd');
        assert.equal(await command('stop tuner'), 'ok');
        assert.equal(await command('unload tuner'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log(`ok tuner quiesce: reload fails fast (${first.elapsed}ms), ` +
                    'TS fd closed then reopened, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
