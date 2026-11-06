/* ABI 3 async quiesce for mod_settings: a diagnostic close delay keeps the
 * SQLite handle on a worker. The first reload fails fast, the loop stays
 * responsive, and a retry completes and republishes the picture service with
 * the same values. Uses a copy of the shipped settingsDB; no TV. */
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
    const settingsdb = path.resolve(process.argv[3] ||
        'artifacts/dumps/apdat_files/sqlite_db/settingsDB.db');
    assert.ok(fs.existsSync(settingsdb), `missing ${settingsdb}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-settings-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    const db = path.join(dir, 'settingsDB.db');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'modules', 'mod_settings.so'),
        path.join(modules, 'mod_settings.so'));
    fs.copyFileSync(settingsdb, db);
    fs.writeFileSync(config,
        `autoload=\nsettings.db=${db}\nsettings.readonly=0\n` +
        'settings.cleanup_delay_ms=400\n');

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
        const started = Date.now();
        const reply = await bounded(new Promise((resolve, reject) => {
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
        assert.equal((await command('load settings')).reply, 'ok settings');
        assert.equal((await command('start settings')).reply, 'ok');
        assert.equal((await command('picture-state 1')).reply,
            'picture source 1 mode 1 zoom 1 energy 3');

        const first = await command('reload settings');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await command('status settings');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal((await command('reload settings')).reply, 'ok',
            'retry must complete the reload');
        assert.equal((await command('picture-state 1')).reply,
            'picture source 1 mode 1 zoom 1 energy 3',
            'picture service must republish with the persisted values');
        const stop = await command('stop settings');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal((await command('stop settings')).reply, 'ok');
        assert.equal((await command('unload settings')).reply, 'ok');
        assert.equal((await command('quit')).reply, 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok settings quiesce: reload fails fast, picture service republished, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
