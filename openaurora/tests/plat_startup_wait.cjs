/* Vendor cold-start order: ICE_HardwareInit waits for the kernel driver marker
 * before PLAT_DRV_INI_Init. Exercise the real mod_plat with a temp marker:
 * pre-existing, arriving during start, and timeout/refusal then retry. */
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
    const core = path.join(build, 'openaurora');
    const module = path.join(build, 'modules', 'mod_plat.so');
    assert.ok(fs.existsSync(core), `missing ${core}`);
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-plat-wait-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    const marker = path.join(dir, 'driver_ready');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_plat.so'));
    fs.writeFileSync(config,
        'autoload=\n' +
        'plat.enable=1\n' +
        'plat.driver_wait=1\n' +
        `plat.driver_marker=${marker}\n` +
        'plat.driver_timeout_ms=400\n');

    const child = spawn(core,
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
            s.setTimeout(5000, () => finish(Error('socket timeout')));
        }), 7000);
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

        fs.writeFileSync(marker, '1\n');
        assert.equal(await command('load plat'), 'ok plat');
        const warm = await timed('start plat');
        assert.equal(warm.reply, 'ok');
        assert.ok(warm.elapsed < 500, `pre-existing marker start took ${warm.elapsed}ms`);
        assert.equal(await command('stop plat'), 'ok');
        assert.equal(await command('unload plat'), 'ok');

        fs.rmSync(marker);
        assert.equal(await command('load plat'), 'ok plat');
        const waiting = timed('start plat');
        setTimeout(() => fs.writeFileSync(marker, '1\n'), 150);
        const arrived = await waiting;
        assert.equal(arrived.reply, 'ok');
        assert.ok(arrived.elapsed >= 120,
            `start did not wait for the marker (${arrived.elapsed}ms)`);
        assert.match(stderr, /plat: waiting for .*driver_ready/);
        assert.match(stderr, /plat: driver marker ready after \d+ ms/);
        assert.equal(await command('stop plat'), 'ok');
        assert.equal(await command('unload plat'), 'ok');

        fs.rmSync(marker);
        assert.equal(await command('load plat'), 'ok plat');
        const refused = await timed('start plat');
        assert.match(refused.reply, /^failed/);
        assert.ok(refused.elapsed >= 300,
            `timeout refusal returned too early (${refused.elapsed}ms)`);
        assert.match(stderr, /not readable after \d+ ms; refusing HAL bring-up/);
        fs.writeFileSync(marker, '1\n');
        assert.equal(await command('start plat'), 'ok', 'retry after the marker appears');
        assert.equal(await command('unload plat'), 'ok');

        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log(`ok plat startup wait: pre-existing ${warm.elapsed}ms, ` +
                    `arrived after ${arrived.elapsed}ms, timeout refused at ${refused.elapsed}ms then retried`);
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
