/* ABI 3 async quiesce for mod_ui_omarchy: invalidate callbacks and unregister
 * the engine skin synchronously, snapshot/detach cached skin state, then free
 * it from a sched slice. The engine must report no active skin while pending,
 * and the replacement must restore the handoff blob and re-register. */
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
    const files = {
        core: path.join(build, 'openaurora'),
        engine: path.join(build, 'modules', 'mod_ui_engine.so'),
        omarchy: path.join(build, 'modules', 'mod_ui_omarchy.so'),
        lua: path.join(build, 'modules', 'mod_lua.so'),
        display: path.join(build, 'tests', 'ui_nav_display.so'),
    };
    for (const [name, file] of Object.entries(files))
        assert.ok(fs.existsSync(file), `missing ${name}: ${file}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-omarchy-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    const script = path.join(dir, 'skin.lua');
    fs.mkdirSync(modules);
    fs.copyFileSync(files.engine, path.join(modules, 'mod_ui_engine.so'));
    fs.copyFileSync(files.omarchy, path.join(modules, 'mod_ui_omarchy.so'));
    fs.copyFileSync(files.lua, path.join(modules, 'mod_lua.so'));
    fs.copyFileSync(files.display, path.join(modules, 'mod_nav_display.so'));
    fs.writeFileSync(script,
        "oa.theme(9, 8, 7, 255, 6, 5, 4, 255)\n" +
        "oa.layout('rail', 0, 0, 40, 100)\n" +
        "function oa_paint(skin) end\n");
    fs.writeFileSync(config,
        'autoload=\n' +
        'ui.skin=omarchy\n' +
        'ui.ok_file=\n' +
        'ui.fps=0\n' +
        `ui.dir=${dir}\n` +
        `ui.lua.script=${script}\n` +
        `ui.lua.data=${dir}\n` +
        'ui.lua.control=2\n' +
        'ui.omarchy.lua=1\n' +
        'ui.paint=1\n' +
        'ui_omarchy.cleanup_delay_ms=400\n');

    const child = spawn(files.core,
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

    async function command_all(text) {
        return bounded(new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '', done = false, idle = null;
            const finish = (error, value) => {
                if (done) return;
                done = true; s.destroy();
                if (idle) clearTimeout(idle);
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text + '\n'));
            s.on('error', e => finish(e));
            s.on('end', () => finish(null, data));
            s.on('data', d => {
                data += d;
                if (idle) clearTimeout(idle);
                idle = setTimeout(() => finish(null, data), 150);
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        }), 4000);
    }

    async function timed(text) {
        const started = Date.now();
        const reply = await command(text);
        return { reply, elapsed: Date.now() - started };
    }

    async function wait_for(text, pattern, ms = 5000) {
        const started = Date.now();
        let reply = '';
        while (Date.now() - started < ms) {
            try {
                reply = await command(text);
            } catch (e) {
                reply = e.message;
            }
            if (pattern.test(reply)) return reply;
            await new Promise(resolve => setTimeout(resolve, 30));
        }
        throw Error(`${text} did not match ${pattern}: ${reply}`);
    }

    const state = 'omarchy colors 1 bg 9 8 7 255 accent 6 5 4 ' +
                  'layout 1 0 0 40 100 screen 1 1920 1080';
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
        for (const [name, cmd] of [
            ['nav_display', 'load nav_display'], ['nav_display', 'start nav_display'],
            ['lua', 'load lua'], ['lua', 'start lua'],
            ['ui_engine', 'load ui_engine'], ['ui_engine', 'start ui_engine'],
            ['ui_omarchy', 'load ui_omarchy'], ['ui_omarchy', 'start ui_omarchy'],
        ]) {
            const reply = await command(cmd);
            assert.ok(/^(ok\b|ok )/.test(reply), `${cmd}: ${reply}`);
        }
        await wait_for('skin', /active omarchy/);
        assert.equal(await command('omarchy-state'), state);

        const reload = await timed('reload ui_omarchy');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status ui_omarchy');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.match(await command_all('status ui_omarchy'), /cleanup_pending 1/);
        assert.doesNotMatch(await command('skin'), /active omarchy/,
            'the old skin must be unregistered synchronously');
        assert.equal(await command('omarchy-state'), state,
            'the detached state must still be readable');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ui_omarchy'), 'ok',
            'retry must complete the reload');
        assert.match(stderr, /ui_omarchy: restored 72 bytes of state/);
        await wait_for('skin', /active omarchy/);
        assert.equal(await command('omarchy-state'), state,
            'the replacement must restore the cached skin state');

        const stop = await timed('stop ui_omarchy');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.doesNotMatch(await command('skin'), /active omarchy/);
        assert.equal(await command('omarchy-state'), state);
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ui_omarchy'), 'ok');
        assert.equal(await command('unload ui_omarchy'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log(`ok ui_omarchy quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'skin unregistered, state handed off, retry re-registers');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
