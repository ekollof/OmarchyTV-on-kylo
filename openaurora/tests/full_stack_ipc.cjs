/* Offline rehearsal of the TV full-portal takeover sequence.
 *
 * Same module set/order as the deferred TV run, with the recorder display
 * instead of display_fb and the host plat fixture instead of real HAL. The
 * UART/BLAST hardware stays closed; key frames and reloads go through the real
 * Unix control socket. No TV or device access. */
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
    const root = path.resolve(__dirname, '..', '..');
    const db = path.join(root, 'artifacts/dumps/apdat_files/sqlite_db/settingsDB.db');
    const ui = path.join(root, 'artifacts/workbench/source-stage-20261101/ui');
    for (const [what, p] of [['settingsDB', db], ['UI assets', ui]]) {
        if (!fs.existsSync(p)) {
            console.error(`skipping: ${what} unavailable at ${p}`);
            process.exit(77);
        }
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-full-stack-'));
    const modules = path.join(dir, 'modules'), config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    for (const name of ['plat', 'ir', 'settings', 'service_db', 'image_decode',
        'ui_engine', 'ui_vendor']) {
        fs.copyFileSync(path.join(build, 'modules', `mod_${name}.so`),
            path.join(modules, `mod_${name}.so`));
    }
    fs.copyFileSync(path.join(build, 'tests/ui_nav_display.so'),
        path.join(modules, 'mod_nav_display.so'));
    fs.writeFileSync(config,
        `autoload=\nui.dir=${ui}\nui.fps=0\nui.ok_file=\nui.focus=2\n` +
        `ui.live_cards=0\nui.paint=1\nir.device=\nir.blast=0\n` +
        `settings.db=${db}\nsettings.readonly=1\nservice_db.path=${db}\n`);

    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket, '--log', '3'],
        { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-65536); });
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-65536); });
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
    async function stdinCommand(text, expected) {
        stdout = '';
        child.stdin.write(text + '\n');
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                if (stdout.includes(expected)) {
                    clearInterval(timer); resolve();
                } else if (Date.now() - started > 5000) {
                    clearInterval(timer); reject(Error('timeout: ' + expected));
                }
            }, 20);
        }), 6000);
    }
    async function keyWorks(code) {
        stdout = ''; stderr = '';
        assert.equal(await command('key ' + code), 'ok');
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                const logs = stdout + stderr;
                if (logs.includes('painted portal') ||
                    logs.includes('key rc=' + code)) {
                    clearInterval(timer); resolve();
                } else if (Date.now() - started > 5000) {
                    clearInterval(timer); reject(Error('timeout: key ' + code));
                }
            }, 20);
        }), 6000);
    }

    try {
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                if (fs.existsSync(socket)) { clearInterval(timer); resolve(); }
                else if (Date.now() - started > 6000) {
                    clearInterval(timer); reject(Error('socket timeout'));
                }
            }, 20);
        }), 7000);

        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load nav_display'), 'ok nav_display');
        assert.equal(await command('start nav_display'), 'ok');
        assert.equal(await command('load settings'), 'ok settings');
        assert.equal(await command('start settings'), 'ok');
        assert.equal(await command('load service_db'), 'ok service_db');
        assert.equal(await command('start service_db'), 'ok');
        assert.equal(await command('load image_decode'), 'ok image_decode');
        assert.equal(await command('start image_decode'), 'ok');
        assert.equal(await command('load ir'), 'ok ir');
        assert.equal(await command('start ir'), 'ok');
        assert.equal(await command('load ui_engine'), 'ok ui_engine');
        assert.equal(await command('start ui_engine'), 'ok');
        assert.equal(await command('load ui_vendor'), 'ok ui_vendor');
        assert.equal(await command('start ui_vendor'), 'ok');

        assert.equal(await command('ui-state'), 'portal screen 0');
        await stdinCommand('status ir', 'input:v1');
        await keyWorks(22);
        await keyWorks(22);
        assert.equal(await command('reload ir'), 'ok');
        await stdinCommand('status ir', 'input:v1');
        await keyWorks(21);
        assert.equal(await command('reload ui_vendor'), 'ok');
        assert.equal(await command('ui-state'), 'portal screen 0');
        await keyWorks(22);

        assert.equal(await command('stop ui_vendor'), 'ok');
        assert.equal(await command('stop ui_engine'), 'ok');
        assert.equal(await command('stop ir'), 'ok');
        assert.equal(await command('stop image_decode'), 'ok');
        assert.equal(await command('stop service_db'), 'ok');
        assert.equal(await command('stop settings'), 'ok');
        assert.equal(await command('stop nav_display'), 'ok');
        assert.equal(await command('stop plat'), 'ok');
        for (const name of ['ui_vendor', 'ui_engine', 'ir', 'image_decode',
            'service_db', 'settings', 'nav_display', 'plat']) {
            assert.equal(await command('unload ' + name), 'ok');
        }
        child.kill('SIGTERM');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok full-stack IPC rehearsal: plat/settings/service_db/image_decode/ir/ui_engine/ui_vendor, keys and reloads');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 8000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
