/* reload-suite must stop the core cleanly with exit status 75 so oa-supervise
 * can relaunch the replacement without a watchdog reboot. */
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
    assert.ok(fs.existsSync(core), `missing ${core}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-reload-suite-'));
    const sock = path.join(dir, 'control');
    const config = path.join(dir, 'config');
    fs.writeFileSync(config, 'autoload=\n');
    const child = spawn(core, ['--modules', path.join(build, 'modules'),
        '--config', config, '--sock', sock, '--log', '0'],
        { stdio: ['ignore', 'pipe', 'pipe'] });
    let output = '';
    child.stdout.on('data', d => { output += d; });
    child.stderr.on('data', d => { output += d; });
    const closed = new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', code => resolve(code));
    });
    try {
        await bounded(new Promise((resolve, reject) => {
            const start = Date.now();
            const timer = setInterval(() => {
                if (fs.existsSync(sock)) { clearInterval(timer); resolve(); }
                else if (Date.now() - start > 4000) {
                    clearInterval(timer); reject(Error('socket timeout'));
                }
            }, 20);
        }), 5000);
        const reply = await bounded(new Promise((resolve, reject) => {
            const s = net.createConnection(sock);
            let data = '', done = false;
            const finish = (error, value) => {
                if (done) return;
                done = true; s.destroy();
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write('reload-suite\n'));
            s.on('error', finish);
            s.on('end', () => finish(Error('EOF before reply')));
            s.on('data', d => {
                data += d;
                if (data.includes('\n'))
                    finish(null, data.slice(0, data.indexOf('\n')));
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        }), 4000);
        assert.equal(reply, 'bye');
        assert.equal(await bounded(closed, 5000), 75, output);
        console.log('ok reload-suite: clean exit 75 for supervisor relaunch');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
