/* Real display_fb module over an LD_PRELOAD fake /dev/fb1. Verifies start,
 * service publication, reload, stop, and full region/pan/pixel snapshot
 * restoration. No real framebuffer, HAL or TV. */
const fs = require('fs'), os = require('os'), path = require('path'), net = require('net');
const { spawn } = require('child_process'), assert = require('assert/strict');

function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}
function state(file) {
    const text = fs.readFileSync(file, 'utf8').trim();
    const out = {};
    for (const part of text.split(/\s+/)) {
        const at = part.indexOf('=');
        if (at > 0) out[part.slice(0, at)] = part.slice(at + 1);
    }
    return out;
}
async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const preload = path.join(build, 'tests/fake_fb_preload.so');
    if (!fs.existsSync(preload)) {
        console.error('skipping: fake fb preload unavailable');
        process.exit(77);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-fakefb-'));
    const final = path.join(dir, 'final'), changed = path.join(dir, 'changed');
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config,
        'autoload=\n' +
        'display.fb=/dev/fb1\ndisplay.restore_snapshot=1\n' +
        'display.pan=1\ndisplay.set_region=1\ndisplay.test=1\n' +
        'display.open_timeout_ms=1000\ndisplay.cleanup_delay_ms=400\n');
    const env = { ...process.env, LD_PRELOAD: preload,
        OA_FAKE_FB_STATE: final, OA_FAKE_FB_CHANGED: changed };
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'], env });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-16384); });
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
    async function timedCommand(text) {
        const started = Date.now();
        const reply = await command(text);
        return { reply, elapsed: Date.now() - started };
    }
    async function waitFor(predicate, label) {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (predicate()) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('timeout: ' + label)); }, 5000);
        }), 6000);
    }
    async function retryCommand(text, expected = 'ok', tries = 30) {
        let last = '';
        for (let i = 0; i < tries; i++) {
            last = await command(text);
            if (last === expected) return last;
            await new Promise(r => setTimeout(r, 100));
        }
        return last;
    }
    async function statusServices() {
        stdout = '';
        child.stdin.write('status display_fb\n');
        await waitFor(() => stdout.includes('name    display_fb'), 'display status');
        await waitFor(() => stdout.includes('state   started'), 'display started');
        await waitFor(() => stdout.includes('display:v2'), 'display v2');
        await waitFor(() => stdout.includes('display.alpha:v1'), 'display alpha');
    }
    function checkFinal() {
        const s = state(final);
        assert.equal(s.current_yoffset, s.initial_yoffset);
        assert.equal(s.current_mau, s.initial_mau);
        assert.equal(s.current_region, s.initial_region);
        assert.equal(s.current_pixels, s.initial_pixels);
    }

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load display_fb'), 'ok display_fb');
        fs.rmSync(final, { force: true });
        fs.rmSync(changed, { force: true });
        assert.equal(await command('start display_fb'), 'ok');
        await statusServices();
        await waitFor(() => {
            try {
                return fs.readFileSync(changed, 'utf8').includes('current_pixels=');
            } catch {
                return false;
            }
        }, 'changed state with pixels');
        const changed_state = state(changed);
        assert.notEqual(changed_state.current_region, changed_state.initial_region);
        assert.notEqual(changed_state.current_yoffset, changed_state.initial_yoffset);
        assert.notEqual(changed_state.current_pixels, changed_state.initial_pixels);

        /* ABI3 async gate: the diagnostic delay keeps restore/close on a
         * worker. The first reload must fail fast, status must stay
         * responsive, and a retry must complete and republish display v2. */
        const first = await timedCommand('reload display_fb');
        assert.match(first.reply, /^failed/, 'first reload must fail fast');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await timedCommand('status display_fb');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        await new Promise(r => setTimeout(r, 600));
        assert.equal(await retryCommand('reload display_fb'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('display-info'),
            'display 1920x1080 32bpp 2 frames');
        await statusServices();

        fs.rmSync(final, { force: true });
        assert.equal(await command('suspend'), 'ok');
        await waitFor(() => fs.existsSync(final), 'suspend restore');
        checkFinal();
        assert.equal(await command('resume'), 'ok');
        await statusServices();

        fs.rmSync(final, { force: true });
        assert.equal(await retryCommand('stop display_fb'), 'ok');
        await waitFor(() => fs.existsSync(final), 'final state');
        checkFinal();

        fs.rmSync(final, { force: true });
        assert.equal(await command('start display_fb'), 'ok');
        fs.rmSync(final, { force: true });
        assert.equal(await retryCommand('reload display_fb'), 'ok');
        await waitFor(() => fs.existsSync(final), 'reload final');
        checkFinal();
        fs.rmSync(final, { force: true });
        assert.equal(await retryCommand('stop display_fb'), 'ok');
        await waitFor(() => fs.existsSync(final), 'stop final');
        checkFinal();

        assert.equal(await retryCommand('unload display_fb'), 'ok');
        assert.equal(await command('load display_fb'), 'ok display_fb');
        fs.rmSync(final, { force: true });
        assert.equal(await command('start display_fb'), 'ok');
        fs.rmSync(final, { force: true });
        assert.equal(await retryCommand('stop display_fb'), 'ok');
        await waitFor(() => fs.existsSync(final), 'reuse final');
        checkFinal();

        child.kill('SIGTERM');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual fake-fbdev display_fb IPC: async gate, start/suspend/resume/reload/stop/reuse, full snapshot restoration');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 8000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
