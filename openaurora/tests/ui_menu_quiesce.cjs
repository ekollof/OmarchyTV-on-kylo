/* ABI 3 async quiesce for mod_ui_menu: unpublish ui.menu and detach the
 * parsed JSON document, then free it on a worker. The first reload fails fast,
 * menu-state reports ENODEV, status stays responsive, and a retry completes
 * and republishes the same tree. */
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
    const menu = path.resolve(process.argv[3] ||
        'openaurora/menus/sub_launcher.json');
    assert.ok(fs.existsSync(menu), `missing ${menu}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ui-menu-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'modules', 'mod_ui_menu.so'),
        path.join(modules, 'mod_ui_menu.so'));
    fs.writeFileSync(config,
        `autoload=\nui_menu.file=${menu}\n` +
        'ui_menu.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load ui_menu'), 'ok ui_menu');
        assert.equal(await command('start ui_menu'), 'ok');
        const tree = 'menu sub_launcher dialogs 1 branches 5';
        assert.equal(await command('menu-state sub_launcher'), tree);

        const first = await timed('reload ui_menu');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await timed('status ui_menu');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('menu-state sub_launcher'),
            'failed errno 19', 'service must be unpublished while the worker runs');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ui_menu'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('menu-state sub_launcher'), tree,
            'the reloaded document must be republished');

        const stop = await timed('stop ui_menu');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('menu-state sub_launcher'),
            'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ui_menu'), 'ok');
        assert.equal(await command('unload ui_menu'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /ui_menu: loaded .* \(1 dialogs\)/);
        console.log(`ok ui_menu quiesce: reload fails fast (${first.elapsed}ms), ` +
                    'document republished, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
