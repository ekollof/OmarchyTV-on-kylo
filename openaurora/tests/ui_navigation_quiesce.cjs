/* ABI 3 async quiesce for mod_ui_navigation: unpublish ui.navigation
 * synchronously, detach the heap state, then free it synchronously or on a
 * worker. Reload must hand the OAN2 state blob to the replacement. */
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
    const module = path.join(build, 'modules', 'mod_ui_navigation.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ui-nav-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_ui_navigation.so'));
    fs.writeFileSync(config,
        'autoload=\nui_navigation.cleanup_delay_ms=400\n');

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

    const selected = 'tab 2 item 109 submenu 1 pending 2';
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
        assert.equal(await command('load ui_navigation'), 'ok ui_navigation');
        assert.equal(await command('start ui_navigation'), 'ok');
        assert.equal(await command('nav-state'), 'tab 0 item 0 submenu 0 pending 0');
        assert.equal(await command('nav-result 109'), 'queued');
        assert.equal(await command('nav-operation 6'), 'queued');
        assert.equal(await command('nav-state'), selected);

        const reload = await timed('reload ui_navigation');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status ui_navigation');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('nav-state'), 'failed errno 19',
            'service must be unpublished while the state is detached');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ui_navigation'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('nav-state'), selected,
            'the OAN2 state must survive the async free');
        assert.equal(await command('nav-take'),
            'navigation 19 tab 2 item 109 submenu 1 pending 1',
            'the replacement must still deliver queued navigation');

        const stop = await timed('stop ui_navigation');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('nav-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ui_navigation'), 'ok');
        assert.equal(await command('unload ui_navigation'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started ui_navigation/);
        console.log(`ok ui_navigation quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'state handed off, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
