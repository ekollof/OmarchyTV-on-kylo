/* Real Unix-socket lifecycle for the non-HAL UI stack:
 * settings (launcher.db), image.decode, recorder display, ui_engine, ui_vendor.
 * Uses a copied settingsDB fixture and repository image assets. No TV/HAL. */
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
    const database = path.join(root, 'artifacts/dumps/apdat_files/sqlite_db/settingsDB.db');
    const png = path.join(root, 'artifacts/workbench/thumbs/launcher_app_thumb61');
    if (!fs.existsSync(database) || !fs.existsSync(png)) {
        console.error('skipping: stack corpus unavailable');
        process.exit(77);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-stack-ipc-'));
    const modules = path.join(dir, 'modules'), ui = path.join(dir, 'ui');
    const db = path.join(dir, 'settings.db');
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.mkdirSync(modules); fs.mkdirSync(ui);
    fs.copyFileSync(database, db);
    for (const name of ['settings', 'image_decode', 'ui_engine', 'ui_vendor']) {
        fs.copyFileSync(path.join(build, 'modules', `mod_${name}.so`),
            path.join(modules, `mod_${name}.so`));
    }
    fs.copyFileSync(path.join(build, 'tests/ui_nav_display.so'),
        path.join(modules, 'mod_nav_display.so'));
    fs.writeFileSync(config,
        `autoload=\nsettings.db=${db}\nsettings.readonly=1\n` +
        `ui.dir=${ui}\nui.fps=0\nui.ok_file=\n`);
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'] });
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
    async function waitFor(predicate, label) {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (predicate()) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('timeout: ' + label)); }, 4000);
        }), 5000);
    }
    async function stdinCommand(text, expected) {
        stdout = '';
        child.stdin.write(text + '\n');
        await waitFor(() => stdout.includes(expected), expected);
    }
    const enodev = 'failed errno ' + os.constants.errno.ENODEV;

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('launcher-items 0'), enodev);

        assert.equal(await command('load settings'), 'ok settings');
        assert.equal(await command('start settings'), 'ok');
        const count = await command('launcher-items 0');
        assert.match(count, /^count \d+$/);
        assert(parseInt(count.slice(6), 10) > 0, count);

        assert.equal(await command('load image_decode'), 'ok image_decode');
        assert.equal(await command('start image_decode'), 'ok');
        assert.equal(await command('image-decode ' + png),
            'image 400x225 fnv 12815d59be310b91');

        assert.equal(await command('load nav_display'), 'ok nav_display');
        assert.equal(await command('start nav_display'), 'ok');
        assert.equal(await command('load ui_engine'), 'ok ui_engine');
        assert.equal(await command('start ui_engine'), 'ok');
        assert.equal(await command('load ui_vendor'), 'ok ui_vendor');
        assert.equal(await command('start ui_vendor'), 'ok');
        assert.equal(await command('ui-state'), 'portal screen 0');
        await stdinCommand('status settings', 'name    settings');
        await waitFor(() => stdout.includes('state   started'), 'settings started');

        assert.equal(await command('reload image_decode'), 'ok');
        assert.equal(await command('image-decode ' + png),
            'image 400x225 fnv 12815d59be310b91');

        assert.equal(await command('stop ui_vendor'), 'ok');
        assert.equal(await command('reload settings'), 'ok');
        assert.match(await command('launcher-items 0'), /^count \d+$/);
        assert.equal(await command('start ui_vendor'), 'ok');
        assert.equal(await command('ui-state'), 'portal screen 0');

        assert.equal(await command('stop ui_vendor'), 'ok');
        assert.equal(await command('reload ui_engine'), 'ok');
        assert.equal(await command('start ui_vendor'), 'ok');
        assert.equal(await command('ui-state'), 'portal screen 0');

        assert.equal(await command('stop ui_vendor'), 'ok');
        assert.equal(await command('stop ui_engine'), 'ok');
        assert.equal(await command('reload nav_display'), 'ok');
        assert.equal(await command('start ui_engine'), 'ok');
        assert.equal(await command('start ui_vendor'), 'ok');
        assert.equal(await command('ui-state'), 'portal screen 0');

        assert.equal(await command('stop ui_vendor'), 'ok');
        assert.equal(await command('unload ui_vendor'), 'ok');
        assert.equal(await command('load ui_vendor'), 'ok ui_vendor');
        assert.equal(await command('start ui_vendor'), 'ok');
        assert.equal(await command('ui-state'), 'portal screen 0');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 5000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual non-HAL UI stack IPC: settings/launcher rows, image.decode, recorder display and ui_vendor reloads');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 5000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
