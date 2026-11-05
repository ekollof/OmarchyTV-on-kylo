/* Real Unix-socket lifecycle for fixture-backed modules:
 * ui_theme, ui_profile, audio_amp_profile, audio_curve and audio_mixer.
 * Synthetic BinSVG/profile/AQ fixtures plus the extracted hwprofile; no TV/HAL. */
const fs = require('fs'), os = require('os'), path = require('path'), net = require('net');
const { spawn } = require('child_process'), assert = require('assert/strict');

function put(b, off, value) { b.writeUInt32LE(value >>> 0, off); }
function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

function themeFixture(index, binary, x) {
    const b = Buffer.from([1, 0x72, 0, 0, 0, 0, 0, 7, 0, 9, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0]);
    b[3] = x;
    fs.writeFileSync(binary, b);
    fs.writeFileSync(index, '1\n0xe8eb982e 20\n');
}

function profileFixture(file, connectivity, hotel) {
    const b = Buffer.alloc(168);
    b.write('EFSL');
    put(b, 0x38, 5);
    const rec = (off, hash, kind, count, value) => {
        put(b, off, hash); b[off + 4] = kind;
        put(b, off + 8, count); put(b, off + 12, value);
    };
    rec(0x44, 0xe6e0d5c2, 1, 4, 0);
    rec(0x58, 0xcbed0877, 1, 1, 0);
    rec(0x6c, 0xcbed0877, 5, 0, connectivity);
    rec(0x80, 0x643f8ec3, 1, 1, 0);
    rec(0x94, 0xa5317464, 5, 0, hotel);
    fs.writeFileSync(file, b);
}

function aqFixture(file, base) {
    const b = Buffer.alloc(1668);
    for (let i = 0; i < 50; i++) {
        b.writeUInt16LE(base + i, 0x24 + 4 * i);
        b.writeUInt16LE(base + 500 + i, 0x1b8 + 4 * i);
    }
    fs.writeFileSync(file, b);
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const root = path.resolve(__dirname, '..', '..');
    const hwprofile = path.join(root, 'artifacts/profiles/mb230_hwprofile.bin');
    if (!fs.existsSync(hwprofile)) {
        console.error('skipping: hwprofile unavailable');
        process.exit(77);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-fixture-ipc-'));
    const index = path.join(dir, 'theme.index'), binary = path.join(dir, 'theme.bin');
    const profile = path.join(dir, 'profile.bin'), aq = path.join(dir, 'aq.bin');
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    themeFixture(index, binary, 11);
    profileFixture(profile, 1, 0);
    aqFixture(aq, 100);
    fs.writeFileSync(config,
        'autoload=\n' +
        `ui_theme.index=${index}\nui_theme.binary=${binary}\n` +
        `ui_profile.swprofile=${profile}\n` +
        `amp_profile.hwprofile=${hwprofile}\n` +
        `audio_curve.aq=${aq}\naudio_curve.profile=0\n` +
        `audio_mixer.broker=${path.join(build, 'oa-audio-broker')}\n`);
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
    async function statusService(name, service) {
        stdout = '';
        child.stdin.write(`status ${name}\n`);
        await waitFor(() => stdout.includes('name    ' + name), name + ' status');
        await waitFor(() => stdout.includes('state   started'), name + ' started');
        await waitFor(() => stdout.includes(service), name + ' ' + service);
    }

    const modules = ['ui_theme', 'ui_profile', 'audio_amp_profile',
        'audio_curve', 'audio_mixer'];
    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        for (const name of modules) assert.equal(await command('load ' + name), 'ok ' + name);
        for (const name of modules) assert.equal(await command('start ' + name), 'ok');
        await statusService('ui_theme', 'ui.theme:v1');
        await statusService('ui_profile', 'ui.profile:v1');
        await statusService('audio_amp_profile', 'audio.amp_profile:v1');
        await statusService('audio_curve', 'audio.curve:v1');
        await statusService('audio_mixer', 'audio.mixer:v1');

        assert.equal(await command('audio-curve 10'), 'curve 110');
        assert.match(await command('audio-curve-dump'), /^curve 101 100 101 102 /);
        assert.equal(await command('audio-speaker 50 -50'),
            'speaker level 50 left 50 right 50 balance 0');
        assert.equal(await command('audio-speaker 150 100'),
            'speaker level 100 left 100 right 100 balance 100');
        assert.match(await command('amp-profile'), /^power '12Watt' prescale 27 /);

        assert.equal(await command('reload ui_theme'), 'ok');
        assert.equal(await command('reload ui_profile'), 'ok');
        assert.equal(await command('reload audio_amp_profile'), 'ok');
        assert.equal(await command('reload audio_curve'), 'ok');
        assert.equal(await retryCommand('reload audio_mixer'), 'ok');
        await statusService('ui_theme', 'ui.theme:v1');
        await statusService('audio_curve', 'audio.curve:v1');

        assert.equal(await command('stop audio_curve'), 'ok');
        assert.equal(await command('start audio_curve'), 'ok');
        assert.equal(await command('stop audio_amp_profile'), 'ok');
        assert.equal(await command('start audio_amp_profile'), 'ok');
        assert.match(await command('amp-profile'), /^power '12Watt' prescale 27 /);

        assert.equal(await retryCommand('stop audio_mixer'), 'ok');
        assert.equal(await retryCommand('unload audio_mixer'), 'ok');
        assert.equal(await command('load audio_mixer'), 'ok audio_mixer');
        assert.equal(await command('start audio_mixer'), 'ok');
        await statusService('audio_mixer', 'audio.mixer:v1');

        assert.equal(await command('stop ui_theme'), 'ok');
        assert.equal(await command('start ui_theme'), 'ok');
        assert.equal(await command('reload ui_profile'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual fixture-backed module IPC: theme/profile/amp/curve/mixer start, curve queries, reload and reuse');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 8000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
