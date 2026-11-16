/* Gated off-loop PWM writes over the real control socket. Host build has no
 * vendor HAL, so completions report -ENOTSUP; this proves the worker is used,
 * jobs are queued/completed on the loop and the lifecycle is clean. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-panel-async-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'modules', 'mod_panel.so'),
        path.join(modules, 'mod_panel.so'));
    fs.copyFileSync(path.join(build, 'modules', 'mod_plat.so'),
        path.join(modules, 'mod_plat.so'));
    fs.writeFileSync(config, [
        'autoload=', 'plat.enable=1', 'panel.start=40',
        'panel.cleanup_delay_ms=0', 'panel.async_set=1', ''
    ].join('\n'));

    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stderr = '';
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-65536); });
    const closed = new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', (code) => resolve({ code }));
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
        assert.equal(await command('panel-async-state'),
            'panel async pending 0 completed 0 failed 0');

        assert.equal(await command('panel 50'), 'ok');
        assert.equal(await command('panel 75'), 'ok');
        let state = await command('panel-async-state');
        for (let i = 0; i < 100 && !/completed 2/.test(state); i++) {
            await new Promise(r => setTimeout(r, 20));
            state = await command('panel-async-state');
        }
        assert.match(state, /^panel async pending 0 completed 2 failed 2$/);

        /* The host build has no vendor HAL: the async writes report failure
         * and the configured start percent remains unchanged. */
        assert.equal(await command('panel-state'), 'panel percent 40');
        assert.equal(await command('panel 101'), 'failed errno 22');

        let stop = await command('stop panel');
        if (stop.startsWith('failed')) {
            await new Promise(r => setTimeout(r, 400));
            stop = await command('stop panel');
        }
        assert.equal(stop, 'ok');
        assert.equal(await command('panel-async-state'), 'failed errno 19');
        assert.equal(await command('unload panel'), 'ok');
        child.kill('SIGTERM');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok panel async ipc: gated off-loop PWM queue, completions '
            + 'and clean stop/unload');
    } catch (e) {
        cleanup();
        throw e;
    } finally {
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

bounded(main(), 60000).catch(e => { console.error(e); process.exit(1); });
