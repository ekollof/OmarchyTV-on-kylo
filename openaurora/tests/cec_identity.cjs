/* End-to-end CEC identity queries over the real mod_cec receive path:
 * 0x83 -> broadcast 0x84 (PA 0x0000, TV), 0x85 -> broadcast 0x82 0x0000 while
 * the tuner is displayed. No TV, CEC or power hardware access. */
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

async function scenario(build, options) {
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-cec-id-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'tests', 'mod_cec_fake.so'),
                    path.join(modules, 'mod_cec.so'));
    fs.copyFileSync(path.join(build, 'tests', 'mod_cec_services_fake.so'),
                    path.join(modules, 'mod_cec_services_fake.so'));
    fs.writeFileSync(config,
        'autoload=\n' +
        'cec.enable=1\n' +
        'cec.device=/fixture/ntkcec\n' +
        'cec.poll_ms=20\n' +
        'cec.cleanup_delay_ms=0\n' +
        'cec.inactive_fallback=0\n' +
        'cec.active_source=0\n' +
        'cec.standby=0\n' +
        `cec.identity=${options.gate}\n` +
        `fake.source_external=${options.external}\n` +
        'fake.tuner_selected=200\n' +
        'fake.power_available=1\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'],
        { stdio: ['ignore', 'pipe', 'pipe'],
          env: { ...process.env, OA_CEC_FAKE_RX: 'raw:' + options.raw } });
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
            s.setTimeout(4000, () => finish(Error('socket timeout')));
        }), 5000);
    }

    try {
        await waitFor(() => fs.existsSync(socket), 4000, 'socket');
        assert.equal(await command('load cec_services_fake'),
                     'ok cec_services_fake');
        assert.equal(await command('start cec_services_fake'), 'ok');
        assert.equal(await command('load cec'), 'ok cec');
        assert.equal(await command('start cec'), 'ok');

        await waitFor(() => output.includes(`op ${options.op} len`), 4000,
                      `0x${options.op} receive`);
        if (options.tx !== null) {
            await waitFor(() => output.includes(options.tx), 4000,
                          'identity reply');
        } else {
            await new Promise(r => setTimeout(r, 150));
            assert.ok(!output.includes('fake cec tx'),
                      'gated/no-active frame still sent a reply');
        }

        assert.equal(await command('quit'), 'bye');
        await closed;
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'tests', 'mod_cec_fake.so');
    const services = path.join(build, 'tests', 'mod_cec_services_fake.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    assert.ok(fs.existsSync(services), `missing ${services}`);

    /* Directed Give Physical Address -> broadcast Report Physical Address. */
    await scenario(build, {
        gate: '1', external: '17', raw: '5083', op: '83',
        tx: 'fake cec tx len=5 0f 84 00 00 00'
    });
    /* Request Active Source while the tuner owns the screen. */
    await scenario(build, {
        gate: '1', external: '0', raw: '5f85', op: '85',
        tx: 'fake cec tx len=4 0f 82 00 00'
    });
    /* External source active: the TV must not claim the screen. */
    await scenario(build, {
        gate: '1', external: '17', raw: '5f85', op: '85', tx: null
    });
    /* Disabled gate. */
    await scenario(build, {
        gate: '0', external: '17', raw: '5083', op: '83', tx: null
    });
    console.log('ok cec identity: 0x83 -> 0x84 PA/type, 0x85 tuner-owned ' +
                '0x82, external-source and disabled gates');
}

main().catch(e => { console.error(e); process.exit(1); });
