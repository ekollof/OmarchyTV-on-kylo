/* End-to-end CEC TX/RX over the real module and control socket, with an
 * injected authority page and raw device. No TV, /dev/mem or ntkcec access. */
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

async function waitFor(fn, ms, what) {
    const started = Date.now();
    for (;;) {
        const value = await fn();
        if (value) return value;
        if (Date.now() - started > ms) throw Error(`timeout: ${what}`);
        await new Promise(r => setTimeout(r, 20));
    }
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'tests', 'mod_cec_fake.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-cec-module-fake-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_cec.so'));
    fs.writeFileSync(config,
        'autoload=\n' +
        'cec.enable=1\n' +
        'cec.device=/fixture/ntkcec\n' +
        'cec.poll_ms=20\n' +
        'cec.cleanup_delay_ms=0\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'],
        { stdio: ['ignore', 'pipe', 'pipe'],
          env: { ...process.env, OA_CEC_FAKE_RX_DELAY_MS: '1500' } });
    let output = '';
    child.stdout.on('data', d => { output = (output + d).slice(-65536); });
    child.stderr.on('data', d => { output = (output + d).slice(-65536); });
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
            s.on('end', () => finish(Error(`EOF before reply to '${text}'`)));
            s.on('data', d => {
                data += d;
                if (data.includes('\n')) finish(null, data.trim());
            });
        }), 4000);
    }

    try {
        await waitFor(() => fs.existsSync(socket), 4000, 'socket');
        assert.equal(await command('load cec'), 'ok cec');
        assert.equal(await command('start cec'), 'ok');
        const initial = await command('cec-state');
        assert.match(initial, /supported 1 .*enabled 1/);

        const opened = await waitFor(async () => {
            const s = await command('cec-state');
            return /open 1/.test(s) ? s : null;
        }, 4000, 'auto open');
        assert.match(opened, /enabled 1/);

        /* Hold the first receive in the fake device, then send. The owner has
         * one job slot; the module must queue the send, not return EBUSY. */
        await waitFor(() => output.includes('fake cec rx delay'), 4000,
                      'first receive delay');
        assert.equal(await command('cec-send 0404'), 'ok');

        const received = await waitFor(async () => {
            const s = await command('cec-state');
            return /rx 1/.test(s) ? s : null;
        }, 4000, 'rx frame');
        assert.match(received, /rx 1/);
        assert.ok(output.includes('cec rx 5->0 op 7a len 1 [a8 ]'),
            'missing received-frame log');

        const sent = await waitFor(async () => {
            const s = await command('cec-state');
            return /tx 1/.test(s) ? s : null;
        }, 4000, 'tx frame');
        assert.match(sent, /tx 1/);
        assert.ok(output.includes('fake cec tx len=2 04 04'),
            'missing fake TX bytes');

        assert.equal(await command('cec-close'), 'ok');
        await waitFor(async () => {
            const s = await command('cec-state');
            return /open 0/.test(s) ? s : null;
        }, 4000, 'close');

        assert.equal(await command('quit'), 'bye');
        await closed;
        console.log('ok cec module fake transport: open, RX, policy log, TX bytes and close');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error(e); process.exit(1); });
