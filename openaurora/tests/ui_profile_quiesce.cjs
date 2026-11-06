/* ABI 3 async quiesce for mod_ui_profile: unpublish ui.profile synchronously,
 * detach the copied profile, then free it synchronously or on a worker. The
 * reload must read the changed fixture instead of the detached snapshot. */
const assert = require('assert/strict');
const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');

function put(b, off, value) { b.writeUInt32LE(value >>> 0, off); }
function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

function fixture(file, connectivity, hotel) {
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

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'modules', 'mod_ui_profile.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-profile-quiesce-'));
    const profile = path.join(dir, 'profile.bin');
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_ui_profile.so'));
    fixture(profile, 1, 0);
    fs.writeFileSync(config,
        `autoload=\nui_profile.swprofile=${profile}\n` +
        'ui_profile.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load ui_profile'), 'ok ui_profile');
        assert.equal(await command('start ui_profile'), 'ok');
        assert.equal(await command('profile-state'),
            'profile connectivity 1 enabled 1 hotel 0 enabled 0 present 1/1');

        const reload = await timed('reload ui_profile');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status ui_profile');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('profile-state'), 'failed errno 19',
            'service must be unpublished while the profile is detached');

        fixture(profile, 2, 1);
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ui_profile'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('profile-state'),
            'profile connectivity 2 enabled 0 hotel 1 enabled 1 present 1/1',
            'the replacement must load the changed profile');

        const stop = await timed('stop ui_profile');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('profile-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ui_profile'), 'ok');
        assert.equal(await command('unload ui_profile'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started ui_profile/);
        console.log(`ok ui_profile quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'profile detached, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
