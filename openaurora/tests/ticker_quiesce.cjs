/* ABI 3 async quiesce for mod_ticker: unpublish clock and remove the timer
 * synchronously, snapshot/detach the tick counter, then free it from a sched
 * slice. The count must survive the delayed reload. */
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
    const module = path.join(build, 'modules', 'mod_ticker.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ticker-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_ticker.so'));
    fs.writeFileSync(config, 'autoload=\nticker.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load ticker'), 'ok ticker');
        assert.equal(await command('start ticker'), 'ok');
        await new Promise(resolve => setTimeout(resolve, 1100));
        const before = await command('ticker-state');
        const match = /^ticker ticks (\d+)$/.exec(before);
        assert.ok(match, before);
        const ticks = Number(match[1]);
        assert.ok(ticks >= 1, `expected at least one tick, got ${before}`);

        const reload = await timed('reload ticker');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status ticker');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('ticker-state'), 'failed errno 19',
            'clock service must be unpublished while the counter is detached');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ticker'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('ticker-state'), `ticker ticks ${ticks}`,
            'the tick counter must survive the delayed reload');

        const stop = await timed('stop ticker');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('ticker-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ticker'), 'ok');
        assert.equal(await command('unload ticker'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started ticker/);
        console.log(`ok ticker quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'counter handed off, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
