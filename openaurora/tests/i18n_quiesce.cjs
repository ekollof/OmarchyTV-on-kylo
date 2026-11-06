/* ABI 3 async quiesce for mod_i18n: a diagnostic cleanup delay keeps the
 * profile free on a worker. The first reload fails fast, the loop stays
 * responsive, and a later retry completes. Real langprofile input; no TV. */
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
    const profile = path.resolve(process.argv[3] || 'artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin');
    assert.ok(fs.existsSync(profile), `missing ${profile}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-i18n-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'modules', 'mod_i18n.so'),
        path.join(modules, 'mod_i18n.so'));
    fs.writeFileSync(config,
        `autoload=\ni18n.profile=${profile}\ni18n.language=dut\n` +
        'i18n.cleanup_delay_ms=400\n');

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
        assert.equal((await command('load i18n')).reply, 'ok i18n');
        assert.equal((await command('start i18n')).reply, 'ok');

        const first = await command('reload i18n');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await command('status i18n');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.match(stdout + stderr, /cleanup pending|cleanup_pending 1/);

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal((await command('reload i18n')).reply, 'ok',
            'retry must complete the reload');
        const stop = await command('stop i18n');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal((await command('stop i18n')).reply, 'ok');
        assert.equal((await command('unload i18n')).reply, 'ok');
        assert.equal((await command('quit')).reply, 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok i18n quiesce: reload fails fast, stays responsive, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
