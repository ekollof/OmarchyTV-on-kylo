/* End-to-end CEC 0x44/0x45 navigation keys over the real mod_cec receive
 * path. The standard codes map to the UI's existing RC ids; the fake module
 * records the emitted key events. No TV, UI or CEC hardware access. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-cec-keys-'));
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
        'cec.active_source=1\n' +
        'cec.standby=0\n' +
        'cec.identity=0\n' +
        'cec.power_status=0\n' +
        `cec.rcp=${options.gate}\n` +
        'cec.osd_name=TV\n' +
        'fake.source_external=17\n' +
        'fake.tuner_selected=200\n' +
        'fake.power_available=1\n' +
        'fake.audio_volume=42\n' +
        'fake.audio_available=1\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'],
        { stdio: ['ignore', 'pipe', 'pipe'],
          env: { ...process.env, OA_CEC_FAKE_RX: 'seq:' + options.seq } });
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

        await waitFor(() => output.includes(`op ${options.op} len 1`), 4000,
                      `0x${options.op} receive`);
        await new Promise(r => setTimeout(r, 250));

        if (options.expectKey !== null) {
            assert.ok(output.includes(
                `fake key ${options.expectKey} kind ${options.expectKind}`),
                'missing emitted key event');
            assert.ok(output.includes(options.expectLog),
                      'missing key decision log');
        } else {
            assert.ok(!output.includes('fake key'),
                      'gated/unauthorised navigation key was emitted');
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

    const keys = [
        ['5f4400', 53, 'cec 0x44 key 53 la 5'],
        ['5f4401', 20, 'cec 0x44 key 20 la 5'],
        ['5f4402', 19, 'cec 0x44 key 19 la 5'],
        ['5f4403', 21, 'cec 0x44 key 21 la 5'],
        ['5f4404', 22, 'cec 0x44 key 22 la 5'],
        ['5f440d', 10, 'cec 0x44 key 10 la 5']
    ];
    for (const [code, key, log] of keys) {
        await scenario(build, {
            gate: '1', seq: `5f821000,${code}`, op: '44',
            expectKey: key, expectKind: 0, expectLog: log
        });
    }
    await scenario(build, {
        gate: '1', seq: '5f821000,5f4500', op: '45',
        expectKey: 53, expectKind: 2,
        expectLog: 'cec 0x45 release key 53 la 5'
    });
    await scenario(build, {
        gate: '1', seq: '5f821000,4f4400', op: '44',
        expectKey: null, expectKind: null, expectLog: null
    });
    await scenario(build, {
        gate: '0', seq: '5f821000,5f4400', op: '44',
        expectKey: null, expectKind: null, expectLog: null
    });
    console.log('ok cec rcp keys: Select/Up/Down/Left/Right/Exit, release, ' +
                'wrong LA and disabled gate');
}

main().catch(e => { console.error(e); process.exit(1); });
