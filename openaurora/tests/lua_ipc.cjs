/* Real Unix-socket reload coverage for the shared ui.lua service. No TV. */
const fs = require('fs'), os = require('os'), path = require('path'), net = require('net');
const { spawn } = require('child_process'), assert = require('assert/strict');

function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-lua-ipc-'));
    const script = path.join(dir, 'ui.lua');
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(script,
        'function oa_paint(skin) oa.theme(10,20,30,255,40,50,60,255) end\n');
    fs.writeFileSync(config,
        `autoload=\nui.fps=0\nui.ok_file=\nui.lua.script=${script}\n`);
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stderr = '';
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-16384); });
    const closed = new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', (code, signal) => resolve({ code, signal }));
    });

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
            const timer = setInterval(() => {
                if (fs.existsSync(socket)) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('socket timeout')); }, 5000);
        }), 6000);
        assert.equal(await command('load lua'), 'ok lua');
        assert.equal(await command('start lua'), 'ok');
        assert.equal(await command('reload lua'), 'ok');

        fs.writeFileSync(script, 'function oa_paint(\n');
        assert.equal(await command('reload lua'), 'failed');

        fs.writeFileSync(script,
            'function oa_paint(skin) oa.theme(1,2,3,255,4,5,6,255) end\n');
        assert.equal(await command('reload lua'), 'ok');

        assert.equal(await command('stop lua'), 'ok');
        assert.equal(await command('unload lua'), 'ok');
        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok lua IPC: socket load/start/reload keeps a good script and rejects a bad candidate');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exit(1); });
