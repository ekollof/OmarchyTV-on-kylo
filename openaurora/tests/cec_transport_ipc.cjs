/* mod_cec transport gate over the real control socket, host build.
 *
 * enable=0 publishes policy only; open/send are ENODEV. enable=1 must fail
 * closed at the native authority mapping on the host and publish nothing.
 */
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

async function waitSocket(socket, ms) {
    const started = Date.now();
    for (;;) {
        if (fs.existsSync(socket)) return;
        if (Date.now() - started > ms) throw Error('socket timeout');
        await new Promise(r => setTimeout(r, 20));
    }
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'modules', 'mod_cec.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const disabled =
        'cec owner 1 audio 1 supported 0 local 1 enabled 0 opening 0 open 0 '
        + 'rx 0 tx 0 error 0 last 0';

    async function phase(name, enable, check) {
        const dir = fs.mkdtempSync(path.join(os.tmpdir(), `oa-cec-${name}-`));
        const modules = path.join(dir, 'modules');
        const config = path.join(dir, 'config');
        const socket = path.join(dir, 'control');
        fs.mkdirSync(modules);
        fs.copyFileSync(module, path.join(modules, 'mod_cec.so'));
        fs.writeFileSync(config,
            'autoload=\n' +
            `cec.enable=${enable ? 1 : 0}\n` +
            'cec.device=/tmp/oa-cec-test-node\n' +
            'cec.poll_ms=50\n' +
            'cec.cleanup_delay_ms=0\n');
        const child = spawn(path.join(build, 'openaurora'),
            ['--modules', modules, '--config', config, '--sock', socket,
             '--log', '3'],
            { stdio: ['ignore', 'pipe', 'pipe'] });
        let output = '';
        child.stdout.on('data', d => { output = (output + d).slice(-65536); });
        child.stderr.on('data', d => { output = (output + d).slice(-65536); });
        const closed = new Promise((resolve, reject) => {
            child.on('error', reject);
            child.on('close', (code, signal) => resolve({ code, signal }));
        });
        const cleanup = () => {
            if (child.exitCode === null && !child.signalCode)
                child.kill('SIGTERM');
            fs.rmSync(dir, { recursive: true, force: true });
        };
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
                    if (data.includes('\n'))
                        finish(null, data.slice(0, data.indexOf('\n')));
                });
                s.setTimeout(3000, () => finish(Error(`socket timeout for '${text}'`)));
            }), 4000);
        }
        try {
            await waitSocket(socket, 4000);
            await check(command, () => output);
            assert.equal(await command('quit'), 'bye');
            const exit = await bounded(closed, 5000);
            assert.equal(exit.code, 0, output);
        } finally {
            cleanup();
        }
    }

    await phase('disabled', false, async (command) => {
        assert.equal(await command('load cec'), 'ok cec');
        assert.equal(await command('start cec'), 'ok');
        assert.equal(await command('cec-state'), disabled);
        assert.equal(await command('cec-open'), 'failed errno 19');
        assert.equal(await command('cec-close'), 'failed errno 19');
        assert.equal(await command('cec-send 04'), 'failed errno 19');
        assert.equal(await command('cec-send zz'), 'failed errno 22');
        assert.equal(await command('cec-send 0'), 'failed errno 22');
    });

    await phase('enabled-fail-closed', true, async (command, output) => {
        assert.equal(await command('load cec'), 'ok cec');
        assert.equal(await command('start cec'), 'failed');
        assert.equal(await command('cec-state'), 'failed errno 19');
        assert.equal(await command('cec-open'), 'failed errno 19');
    });

    console.log('ok cec transport gate: disabled policy services, ENODEV '
                + 'open/send, enable=1 fails closed without publishing');
}

main().catch(e => { console.error(e); process.exitCode = 1; });
