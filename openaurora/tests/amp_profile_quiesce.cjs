/* ABI 3 async quiesce for mod_audio_amp_profile: unpublish audio.amp_profile
 * synchronously, detach profile+power, then free them synchronously or on a
 * worker. The reload must reread the changed hwprofile. */
const assert = require('assert/strict');
const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');

function hash(s) {
    let h = (0xf1d3b597 ^ (s.length << 6)) >>> 0, sum = 0, rolling = 0, previous = 0;
    for (const c of Buffer.from(s)) {
        h = ((h + 2 * ~previous) ^ (sum << 9)) >>> 0; sum = (sum + c) >>> 0;
        h = ((h + (c << 17)) ^ rolling) >>> 0; rolling = (c + (rolling << 8)) >>> 0;
        h = (h + (h << (c % 3))) >>> 0; h = (h + (h >>> (previous % 5))) >>> 0; previous = c;
    }
    return h;
}

function fixture(file, value) {
    const b = Buffer.alloc(168);
    b.write('EFSL'); b.writeUInt32LE(5, 0x38);
    const names = ['AudioSettings', 'Prescales', 'DefaultSpeaker', '12Watt', 'Amp_Prescale'];
    const types = [1, 1, 5, 1, 5], counts = [4, 3, 0, 1, 0];
    names.forEach((name, i) => {
        const p = 0x44 + 20 * i; b.writeUInt32LE(hash(name), p); b[p + 4] = types[i];
        b.writeUInt32LE(counts[i], p + 8); b.writeUInt32LE(i === 2 ? 3 : i === 4 ? value : 0, p + 12);
    });
    fs.writeFileSync(file, b);
}

function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'modules', 'mod_audio_amp_profile.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-amp-quiesce-'));
    const file = path.join(dir, 'profile.bin');
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_audio_amp_profile.so'));
    fixture(file, 17);
    fs.writeFileSync(config,
        `autoload=\namp_profile.hwprofile=${file}\n` +
        'audio_amp_profile.cleanup_delay_ms=400\n');

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

    const query = value => new RegExp("^power '12Watt' prescale " + value +
        ' ch1 192 ch2 192 fine 192 ');
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
        assert.equal(await command('load audio_amp_profile'), 'ok audio_amp_profile');
        assert.equal(await command('start audio_amp_profile'), 'ok');
        assert.match(await command('amp-profile'), query(17));

        const reload = await timed('reload audio_amp_profile');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status audio_amp_profile');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('amp-profile'), 'failed errno 19',
            'service must be unpublished while profile+power are detached');

        fixture(file, 31);
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload audio_amp_profile'), 'ok',
            'retry must complete the reload');
        assert.match(await command('amp-profile'), query(31),
            'the replacement must reread the changed hwprofile');

        const stop = await timed('stop audio_amp_profile');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('amp-profile'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop audio_amp_profile'), 'ok');
        assert.equal(await command('unload audio_amp_profile'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started audio_amp_profile/);
        console.log(`ok audio_amp_profile quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'profile detached, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
