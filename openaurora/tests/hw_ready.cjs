/* host.ready_after_autoload: a hardware-only stage has no UI writer. The core
 * must write the supervisor receipt only after every autoload module loaded and
 * start() returned 0; a missing/failed autoload must leave it absent. */
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

async function command(socket, text) {
    return bounded(new Promise((resolve, reject) => {
        const s = net.createConnection(socket);
        let data = '';
        let done = false;
        const finish = (error, value) => {
            if (done) return;
            done = true;
            s.destroy();
            error ? reject(error) : resolve(value);
        };
        s.on('connect', () => s.write(text + '\n'));
        s.on('error', finish);
        s.on('end', () => finish(Error('EOF before reply')));
        s.on('data', d => {
            data += d;
            if (data.includes('\n')) finish(null, data.slice(0, data.indexOf('\n')));
        });
        s.setTimeout(5000, () => finish(Error('socket timeout')));
    }), 7000);
}

async function run(build, autoload, expectReady) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-hw-ready-'));
    const modules = path.join(dir, 'modules');
    fs.mkdirSync(modules);
    const stub = path.join(build, 'tests', 'mod_plat_stub.so');
    assert.ok(fs.existsSync(stub), `missing ${stub}`);
    fs.copyFileSync(stub, path.join(modules, 'mod_plat.so'));
    const socket = path.join(dir, 'control');
    const ready = path.join(dir, 'ready');
    const config = path.join(dir, 'config');
    fs.writeFileSync(config,
        `autoload=${autoload}\n` +
        'host.heartbeat=' + path.join(dir, 'heartbeat') + '\n' +
        'host.ready_after_autoload=1\n' +
        `host.ready_file=${ready}\n` +
        'plat.enable=1\n');

    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket, '--log', '2'],
        { stdio: ['pipe', 'pipe', 'pipe'] });
    let stderr = '';
    child.stderr.on('data', d => { stderr += d; });
    const closed = new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', (code, signal) => resolve({ code, signal }));
    });
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

        if (expectReady) {
            await bounded(new Promise((resolve, reject) => {
                const started = Date.now();
                const timer = setInterval(() => {
                    if (fs.existsSync(ready)) { clearInterval(timer); resolve(); }
                    else if (Date.now() - started > 4000) {
                        clearInterval(timer); reject(Error('ready receipt missing'));
                    }
                }, 20);
            }), 5000);
            const text = fs.readFileSync(ready, 'utf8').trim();
            assert.match(text, new RegExp(`^${child.pid} \\d+$`),
                `bad receipt: ${text}`);
        } else {
            await new Promise(r => setTimeout(r, 500));
            assert.ok(!fs.existsSync(ready), 'ready receipt written after failed autoload');
        }
        assert.equal(await command(socket, 'quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    assert.ok(fs.existsSync(path.join(build, 'openaurora')),
        `missing ${path.join(build, 'openaurora')}`);
    await run(build, 'plat', true);
    await run(build, 'missing_module', false);
    console.log('ok hw ready: receipt written after all autoload starts succeed; absent after a missing module');
}

main().catch(e => { console.error(e); process.exitCode = 1; });
