/* ABI 3 async quiesce for mod_ui_anim: unpublish ui.anim, remove the timer and
 * detach the controller synchronously, then free it from a sched slice. */
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
    const mods = ['mod_ticker.so', 'mod_ui_clock.so', 'mod_ui_anim.so'];
    const sources = mods.map(m => path.join(build, 'modules', m));
    for (const f of sources) assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ui-anim-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    for (let i = 0; i < mods.length; i++)
        fs.copyFileSync(sources[i], path.join(modules, mods[i]));
    fs.writeFileSync(config, 'autoload=\nui_anim.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load ui_clock'), 'ok ui_clock');
        assert.equal(await command('start ui_clock'), 'ok');
        assert.equal(await command('load ui_anim'), 'ok ui_anim');
        assert.equal(await command('start ui_anim'), 'ok');
        assert.equal(await command('anim-state'), 'anim enabled 0 fault 0');

        const reload = await timed('reload ui_anim');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status ui_anim');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('anim-state'), 'failed errno 19',
            'service must be unpublished while the controller is detached');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ui_anim'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('anim-state'), 'anim enabled 0 fault 0',
            'the replacement must republish the animation service');

        const stop = await timed('stop ui_anim');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('anim-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ui_anim'), 'ok');
        assert.equal(await command('unload ui_anim'), 'ok');
        assert.equal(await command('stop ui_clock'), 'ok');
        assert.equal(await command('unload ui_clock'), 'ok');
        assert.equal(await command('stop ticker'), 'ok');
        assert.equal(await command('unload ticker'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started ui_anim/);
        console.log(`ok ui_anim quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'controller detached, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
