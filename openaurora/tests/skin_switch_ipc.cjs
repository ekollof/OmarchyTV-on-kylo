/* A skin must be able to load another skin from its own key callback. The
 * project control key 240 asks the host for the switch; the host defers it
 * until the skin callback has returned, then loads/starts the new skin and
 * unloads the old one. Exercises omarchy -> diag -> omarchy. */
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
    const nav = path.join(build, 'tests', 'ui_nav_display.so');
    const engine = path.join(build, 'modules', 'mod_ui_engine.so');
    const omarchy = path.join(build, 'modules', 'mod_ui_omarchy.so');
    const diag = path.join(build, 'modules', 'mod_ui_diag.so');
    for (const f of [nav, engine, omarchy, diag])
        assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-skin-switch-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(nav, path.join(modules, 'mod_nav_display.so'));
    fs.copyFileSync(engine, path.join(modules, 'mod_ui_engine.so'));
    fs.copyFileSync(omarchy, path.join(modules, 'mod_ui_omarchy.so'));
    fs.copyFileSync(diag, path.join(modules, 'mod_ui_diag.so'));
    fs.writeFileSync(config,
        'autoload=\nui.paint=0\nui.fps=0\nui.ok_file=\n' +
        'ui.skin_timeout_ms=3000\n');

    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stderr = '';
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

    async function waitSkin(name) {
        const deadline = Date.now() + 10000;
        let last = '';
        while (Date.now() < deadline) {
            last = await command('skin');
            if (last.includes(`active ${name}`)) return last;
            await new Promise(r => setTimeout(r, 50));
        }
        throw Error(`skin did not become ${name}: ${last}\n${stderr.slice(-2000)}`);
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
        assert.equal(await command('load nav_display'), 'ok nav_display');
        assert.equal(await command('start nav_display'), 'ok');
        assert.equal(await command('load ui_engine'), 'ok ui_engine');
        assert.equal(await command('start ui_engine'), 'ok');
        assert.equal(await command('load ui_omarchy'), 'ok ui_omarchy');
        assert.equal(await command('start ui_omarchy'), 'ok');
        assert.match(await command('skin'), /active omarchy/);

        assert.equal(await command('emit-key 240'), 'ok');
        await waitSkin('diag');

        assert.equal(await command('emit-key 240'), 'ok');
        await waitSkin('omarchy');

        assert.equal(await command('stop ui_omarchy'), 'ok');
        assert.equal(await command('unload ui_omarchy'), 'ok');
        assert.equal(await command('stop ui_engine'), 'ok');
        assert.equal(await command('unload ui_engine'), 'ok');
        assert.equal(await command('stop nav_display'), 'ok');
        assert.equal(await command('unload nav_display'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok skin switch: omarchy -> diag -> omarchy from skin key callbacks');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
