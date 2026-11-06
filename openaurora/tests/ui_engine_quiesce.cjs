/* ABI 3 async quiesce for mod_ui_engine: synchronous service/display/timer
 * teardown, then the icon cache is detached and freed on a worker. A real
 * OAI1 asset is loaded through the engine before reload to exercise the
 * heavy path. No TV/display paint. */
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
    const root = path.resolve(process.argv[3] || '.');
    const ui = path.join(root, 'artifacts/workbench/source-stage-20261101/ui');
    assert.ok(fs.existsSync(path.join(ui, 'settings_top.rgba')), 'missing asset dir');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ui-engine-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'modules', 'mod_ui_engine.so'),
        path.join(modules, 'mod_ui_engine.so'));
    fs.copyFileSync(path.join(build, 'tests', 'ui_nav_display.so'),
        path.join(modules, 'mod_nav_display.so'));
    fs.writeFileSync(config,
        `autoload=\nui.dir=${ui}\nui.fps=0\n` +
        'ui_engine.cleanup_delay_ms=400\n');

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
        assert.equal((await command('load nav_display')).reply, 'ok nav_display');
        assert.equal((await command('start nav_display')).reply, 'ok');
        assert.equal((await command('load ui_engine')).reply, 'ok ui_engine');
        assert.equal((await command('start ui_engine')).reply, 'ok');
        assert.equal((await command('ui-icon settings_top')).reply,
            'icon settings_top 1920x180');

        const first = await command('reload ui_engine');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await command('status ui_engine');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);

        await new Promise(resolve => setTimeout(resolve, 600));
        const retry = await command('reload ui_engine');
        if (retry.reply !== 'ok')
            console.error('retry reply:', retry.reply, '\nSTDERR:\n', stderr);
        assert.equal(retry.reply, 'ok', 'retry must complete the reload');
        assert.equal((await command('ui-icon settings_top')).reply,
            'icon settings_top 1920x180',
            'engine service and asset cache must work after reload');

        const stop = await command('stop ui_engine');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal((await command('stop ui_engine')).reply, 'ok');
        assert.equal((await command('unload ui_engine')).reply, 'ok');
        assert.equal((await command('quit')).reply, 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok ui_engine quiesce: reload fails fast, asset cache reloads, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
