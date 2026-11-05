/* Verify the ABI 3 async cleanup gate for mod_ir with a real pty UART.
 * A diagnostic cleanup delay keeps the worker busy: the first reload must
 * fail fast with cleanup_pending, the control loop must stay responsive, and a
 * later retry must complete the reload. No TV/BLAST device. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ir-quiesce-'));
    const modules = path.join(dir, 'modules');
    const tty = path.join(dir, 'tty');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    for (const name of ['plat', 'ir']) {
        fs.copyFileSync(path.join(build, 'modules', `mod_${name}.so`),
            path.join(modules, `mod_${name}.so`));
    }
    fs.writeFileSync(config,
        `autoload=\nir.blast=0\nir.device=${tty}\nir.cleanup_delay_ms=400\n`);

    const socat = spawn('socat', ['-d', '-d',
        `pty,raw,echo=0,link=${tty},waitslave`, '-'],
        { stdio: ['pipe', 'ignore', 'ignore'] });
    let child = null;
    const closed = () => new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', (code, signal) => resolve({ code, signal }));
    });
    const cleanup = () => {
        if (child && child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        if (socat.exitCode === null && !socat.signalCode) socat.kill('SIGTERM');
        fs.rmSync(dir, { recursive: true, force: true });
    };

    try {
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                if (fs.existsSync(tty)) { clearInterval(timer); resolve(); }
                else if (Date.now() - started > 4000) {
                    clearInterval(timer); reject(Error('pty link timeout'));
                }
            }, 20);
        }), 5000);

        child = spawn(path.join(build, 'openaurora'),
            ['--modules', modules, '--config', config, '--sock', socket,
             '--log', '3'], { stdio: ['pipe', 'pipe', 'pipe'] });
        let stdout = '', stderr = '';
        child.stdout.on('data', d => { stdout = (stdout + d).slice(-65536); });
        child.stderr.on('data', d => { stderr = (stderr + d).slice(-65536); });
        const exited = closed();

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
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                if (fs.existsSync(socket)) { clearInterval(timer); resolve(); }
                else if (Date.now() - started > 4000) {
                    clearInterval(timer); reject(Error('socket timeout'));
                }
            }, 20);
        }), 5000);

        assert.equal((await command('load plat')).reply, 'ok plat');
        assert.equal((await command('start plat')).reply, 'ok');
        assert.equal((await command('load ir')).reply, 'ok ir');
        assert.equal((await command('start ir')).reply, 'ok');

        const first = await command('reload ir');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);

        const status = await command('status ir');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.match(stdout + stderr, /cleanup pending|cleanup_pending 1/);

        await new Promise(resolve => setTimeout(resolve, 600));
        const retry = await command('reload ir');
        assert.equal(retry.reply, 'ok', 'retry must complete the reload');
        const stop = await command('stop ir');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal((await command('stop ir')).reply, 'ok');
        assert.equal((await command('unload ir')).reply, 'ok');
        assert.equal((await command('quit')).reply, 'bye');
        const exit = await bounded(exited, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok ir quiesce: reload fails fast, stays responsive, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
