/* ABI 3 async quiesce for mod_panel: unpublish panel synchronously, then run
 * the vendor PWM close on a worker after the diagnostic delay. The host build
 * still exercises the gate timing and state restore without touching PWM. */
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
    const panel = path.join(build, 'modules', 'mod_panel.so');
    const plat = path.join(build, 'modules', 'mod_plat.so');
    for (const f of [panel, plat]) assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-panel-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(panel, path.join(modules, 'mod_panel.so'));
    fs.copyFileSync(plat, path.join(modules, 'mod_plat.so'));
    fs.writeFileSync(config,
        'autoload=\nplat.enable=1\npanel.start=40\n' +
        'panel.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load panel'), 'ok panel');
        assert.equal(await command('start panel'), 'ok');
        assert.equal(await command('panel-state'), 'panel percent 40');
        assert.equal(await command('panel 50'), 'failed errno 95',
            'host build has no vendor HAL');
        assert.equal(await command('panel 101'), 'failed errno 22');
        assert.equal(await command('panel'), 'failed errno 22');
        assert.equal(await command('panel-state'), 'panel percent 40');

        const reload = await timed('reload panel');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status panel');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('panel-state'), 'failed errno 19',
            'service must be unpublished while the PWM close is pending');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload panel'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('panel-state'), 'panel percent 40',
            'the replacement must restore/republish the requested percent');

        const stop = await timed('stop panel');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('panel-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop panel'), 'ok');
        assert.equal(await command('unload panel'), 'ok');
        assert.equal(await command('stop plat'), 'ok');
        assert.equal(await command('unload plat'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started panel/);
        console.log(`ok panel quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'publish invalidated, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
