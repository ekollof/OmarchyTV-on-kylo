/* ABI 3 async quiesce for mod_audio: unpublish audio and release plat
 * synchronously, then finish the owner worker on a worker with a diagnostic
 * delay. The saved volume must survive the reload and be applied again. */
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
    const module = path.join(build, 'tests', 'audio-mods', 'mod_audio.so');
    const plat = path.join(build, 'modules', 'mod_plat.so');
    for (const f of [module, plat]) assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-audio-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_audio.so'));
    fs.copyFileSync(plat, path.join(modules, 'mod_plat.so'));
    fs.writeFileSync(config,
        'autoload=\nplat.enable=1\naudio.enable=1\n' +
        'audio.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load audio'), 'ok audio');
        assert.equal(await command('start audio'), 'ok');
        assert.equal(await command('audio-state'), 'audio enabled 1 volume -1');
        assert.equal(await command('audio-volume 42'), 'ok');
        assert.equal(await command('audio-state'), 'audio enabled 1 volume 42');

        const first = await timed('reload audio');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await timed('status audio');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('audio-state'), 'failed errno 19',
            'service must be unpublished while the owner worker finishes');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload audio'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('audio-state'), 'audio enabled 1 volume 42',
            'restored volume must be applied by the new audio service');

        const stop = await timed('stop audio');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('audio-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop audio'), 'ok');
        assert.equal(await command('unload audio'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /vendor audio HAL linked/);
        assert.match(stderr, /volume -> 42/);
        console.log(`ok audio quiesce: reload fails fast (${first.elapsed}ms), ` +
                    'volume 42 restored and reapplied, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
