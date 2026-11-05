/* Real Unix-socket lifecycle for graphics + scene_display with the fixture
 * scene renderer. No display/HAL/TV: the broker child loads
 * tests/scene-renderer-fixture.so and exits on channel close. */
const fs = require('fs'), os = require('os'), path = require('path'), net = require('net');
const { spawn } = require('child_process'), assert = require('assert/strict');

function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const broker = path.join(build, 'oa-scene-broker');
    const renderer = path.join(build, 'tests/scene-renderer-fixture.so');
    if (!fs.existsSync(broker) || !fs.existsSync(renderer)) {
        console.error('skipping: scene broker or fixture renderer unavailable');
        process.exit(77);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-graphics-ipc-'));
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config,
        'autoload=\n' +
        `graphics.broker=${broker}\ngraphics.renderer=${renderer}\n` +
        'ui.fps=0\nui.ok_file=\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-32768); });
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-32768); });
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
    async function waitFor(predicate, label) {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (predicate()) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('timeout: ' + label)); }, 6000);
        }), 7000);
    }
    async function retryCommand(text, expected = 'ok', tries = 40) {
        let last = '';
        for (let i = 0; i < tries; i++) {
            last = await command(text);
            if (last === expected) return last;
            await new Promise(r => setTimeout(r, 100));
        }
        return last;
    }
    async function status(name, ...needles) {
        stdout = '';
        child.stdin.write(`status ${name}\n`);
        await waitFor(() => needles.every(n => stdout.includes(n)),
            `${name} status: ${needles.join(', ')}`);
    }
    function hostChildren() {
        const file = `/proc/${child.pid}/task/${child.pid}/children`;
        try { return fs.readFileSync(file, 'utf8').trim(); } catch { return 'unavailable'; }
    }
    async function noChildren(label) {
        await waitFor(() => hostChildren() === '', label);
    }

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load graphics'), 'ok graphics');
        assert.equal(await command('start graphics'), 'ok');
        await status('graphics', 'state   started', 'display.gpu:v2');

        assert.equal(await command('load scene_display'), 'ok scene_display');
        assert.equal(await command('start scene_display'), 'ok');
        await status('scene_display', 'state   started', 'display.gpu',
            'display:v2', 'display.alpha:v1', 'display.async:v1');

        assert.equal(await command('reload scene_display'), 'ok');
        await status('scene_display', 'state   started', 'display:v2');

        /* A live scene_display holds display.gpu: graphics must refuse to unload. */
        assert.equal(await command('reload graphics'), 'failed');
        await status('graphics', 'state   started', 'display.gpu:v2');

        assert.equal(await command('stop scene_display'), 'ok');
        const oldBroker = hostChildren();
        assert.match(oldBroker, /^\d+$/, 'expected one graphics broker child');
        assert.equal(await retryCommand('reload graphics'), 'ok');
        await status('graphics', 'state   started', 'display.gpu:v2');
        await waitFor(() => {
            const now = hostChildren();
            return /^\d+$/.test(now) && now !== oldBroker;
        }, 'old broker reaped and replacement spawned');

        assert.equal(await command('start scene_display'), 'ok');
        await status('scene_display', 'state   started', 'display:v2');
        assert.equal(await command('stop scene_display'), 'ok');
        assert.equal(await retryCommand('stop graphics'), 'ok');
        await noChildren('broker child reaped after graphics stop');

        assert.equal(await command('unload scene_display'), 'ok');
        assert.equal(await command('unload graphics'), 'ok');
        assert.equal(await command('load graphics'), 'ok graphics');
        assert.equal(await command('start graphics'), 'ok');
        assert.equal(await command('load scene_display'), 'ok scene_display');
        assert.equal(await command('start scene_display'), 'ok');
        await status('scene_display', 'state   started', 'display.async:v1');
        assert.equal(await command('stop scene_display'), 'ok');
        assert.equal(await retryCommand('stop graphics'), 'ok');
        await noChildren('broker child reaped after reuse');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual graphics/scene_display IPC: gpu/display services, hold refusal, dependency-ordered reload, child reap and reuse');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
