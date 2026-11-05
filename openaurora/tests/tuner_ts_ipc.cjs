/* Real Unix-socket reload coverage for the TS replay EIT source. The fixture
 * is generated locally; no TV or tuner hardware is involved. */
const fs = require('fs'), os = require('os'), path = require('path'), net = require('net');
const { spawn, execFileSync } = require('child_process'), assert = require('assert/strict');

function bounded(promise, ms) {
    return new Promise((resolve, reject) => {
        const timer = setTimeout(() => reject(Error('test timeout')), ms);
        promise.then(v => { clearTimeout(timer); resolve(v); },
                     e => { clearTimeout(timer); reject(e); });
    });
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const fixture = path.join(build, 'tests', 'eit_ts_fixture');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-tuner-ts-ipc-'));
    const ts = path.join(dir, 'eit.ts'), config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    execFileSync(fixture, [ts]);
    fs.writeFileSync(config,
        `autoload=\nui.fps=0\nui.ok_file=\ntuner.ts_path=${ts}\n`);
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
            let data = '';
            let done = false;
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
    async function waitDone() {
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(async () => {
                try {
                    const state = await command('tuner-state');
                    if (/ state 2 /.test(state)) { clearInterval(timer); resolve(); }
                    else if (Date.now() - started > 5000) {
                        clearInterval(timer); reject(Error('scan timeout: ' + state));
                    }
                } catch (e) {
                    clearInterval(timer); reject(e);
                }
            }, 25);
        }), 7000);
    }

    try {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (fs.existsSync(socket)) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('socket timeout')); }, 5000);
        }), 6000);
        assert.equal(await command('load epg'), 'ok epg');
        assert.equal(await command('start epg'), 'ok');
        assert.equal(await command('load tuner'), 'ok tuner');
        assert.equal(await command('start tuner'), 'ok');
        assert.equal(await command('tuner-scan 1'), 'ok');
        await waitDone();
        assert.equal(await command('epg-count'), 'count 2');

        assert.equal(await command('reload tuner'), 'ok');
        assert.equal(await command('tuner-scan 1'), 'ok');
        await waitDone();
        assert.equal(await command('epg-count'), 'count 2');
        assert.equal(await command('stop tuner'), 'ok');
        assert.equal(await command('stop epg'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok tuner TS reload IPC: scan/reload/re-scan keep EIT replay and EPG state');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exit(1); });
