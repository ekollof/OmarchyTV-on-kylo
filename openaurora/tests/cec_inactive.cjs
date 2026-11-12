/* End-to-end CEC 0x9D routing over the real mod_cec receive path and fake
 * source/channel/tuner services. No TV, tuner, DTV or CEC hardware access. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-cec-9d-'));
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
        `cec.enable=1\n` +
        'cec.device=/fixture/ntkcec\n' +
        'cec.poll_ms=20\n' +
        'cec.cleanup_delay_ms=0\n' +
        `cec.inactive_fallback=${options.gate}\n` +
        `fake.source_external=${options.external}\n` +
        `fake.tuner_selected=${options.selected}\n` +
        `fake.channels_empty=${options.channelsEmpty ? '1' : '0'}\n`);
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'],
        { stdio: ['ignore', 'pipe', 'pipe'],
          env: { ...process.env, OA_CEC_FAKE_RX: '9d' } });
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

        await waitFor(() => output.includes('cec rx 5->15 op 9d len 2'),
                      4000, '0x9d receive');
        /* The routing runs synchronously in the same completion. Give the log
         * pipe a moment before asserting the negative cases. */
        await new Promise(r => setTimeout(r, 150));

        const call = `fake source inactive position ${options.expect}`;
        if (options.expect !== null) {
            assert.ok(output.includes(call), `missing '${call}'\n${output}`);
            assert.ok(output.includes(
                `cec 0x9d inactive source: external ${options.external} ` +
                `handle ${options.selected} position ${options.expect} selected`),
                'missing routing decision log');
        } else {
            assert.ok(!output.includes('fake source inactive position'),
                      'disabled/TV gate still called the source worker');
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

    await scenario(build, {
        gate: '1', external: '17', selected: '200', expect: 1
    });
    await scenario(build, {
        gate: '1', external: '17', selected: '300', expect: 2
    });
    await scenario(build, {
        gate: '1', external: '17', selected: '200', expect: 1,
        channelsEmpty: true
    });
    await scenario(build, {
        gate: '0', external: '17', selected: '200', expect: null
    });
    await scenario(build, {
        gate: '1', external: '0', selected: '200', expect: null
    });
    console.log('ok cec 0x9d inactive source: gated route, saved-position ' +
                'inverse and external-source gate');
}

main().catch(e => { console.error(e); process.exit(1); });
