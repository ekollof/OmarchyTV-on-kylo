/* mod_audio_amp lifecycle over the real control socket, host build.
 *
 * Phase 1 (enable=0): the module publishes audio.amp, reports disabled, and
 * rejects amp-init with ENODEV without touching any device. Reload is fast.
 * Phase 2 (enable=1 on host): start must fail closed at the native I2C gate
 * (ENOTSUP) and publish nothing, even with a valid profile.
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
    const module = path.join(build, 'modules', 'mod_audio_amp.so');
    const plat = path.join(build, 'tests', 'mod_plat_stub.so');
    const profile = path.resolve(__dirname, '..', '..', 'artifacts', 'profiles',
                                 'mb230_hwprofile.bin');
    for (const f of [module, plat, profile])
        assert.ok(fs.existsSync(f), `missing ${f}`);

    async function phase(name, enable, check) {
        const dir = fs.mkdtempSync(path.join(os.tmpdir(), `oa-audio-amp-${name}-`));
        const modules = path.join(dir, 'modules');
        const config = path.join(dir, 'config');
        const socket = path.join(dir, 'control');
        fs.mkdirSync(modules);
        fs.copyFileSync(module, path.join(modules, 'mod_audio_amp.so'));
        fs.copyFileSync(plat, path.join(modules, 'mod_plat.so'));
        fs.writeFileSync(config,
            'autoload=\n' +
            `audio_amp.enable=${enable ? 1 : 0}\n` +
            `audio_amp.hwprofile=${profile}\n` +
            'audio_amp.cleanup_delay_ms=0\n');

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
                s.on('end', () => finish(Error('EOF before reply')));
                s.on('data', d => {
                    data += d;
                    if (data.includes('\n'))
                        finish(null, data.slice(0, data.indexOf('\n')));
                });
                s.setTimeout(3000, () => finish(Error('socket timeout')));
            }), 4000);
        }

        try {
            await waitSocket(socket, 4000);
            await check(command, output);
            assert.equal(await command('quit'), 'bye');
            const exit = await bounded(closed, 5000);
            assert.equal(exit.code, 0, output);
        } finally {
            cleanup();
        }
    }

    await phase('disabled', false, async (command) => {
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load audio_amp'), 'ok audio_amp');
        assert.equal(await command('start audio_amp'), 'ok');
        assert.match(await command('status audio_amp'),
                     /^name\s+audio_amp$/m);
        assert.equal(await command('amp-state'),
            'amp enabled 0 busy 0 result -1 ticket 0 reset 0 mute 0 live 0 known 0 balance 0 balance_known 0');
        assert.equal(await command('amp-init'), 'failed errno 19');
        assert.equal(await command('amp-mute 1'), 'failed errno 19');
        assert.equal(await command('amp-mute 2'), 'failed errno 22');
        assert.equal(await command('amp-balance 50'), 'failed errno 19');
        assert.equal(await command('amp-balance 101'), 'failed errno 22');
        assert.equal(await command('amp-balance x'), 'failed errno 22');
        const started = Date.now();
        assert.equal(await command('reload audio_amp'), 'ok');
        assert.ok(Date.now() - started < 1500,
                  'disabled reload must not block');
        assert.equal(await command('amp-state'),
            'amp enabled 0 busy 0 result -1 ticket 0 reset 0 mute 0 live 0 known 0 balance 0 balance_known 0');
        assert.equal(await command('stop audio_amp'), 'ok');
        assert.equal(await command('amp-state'), 'failed errno 19');
    });

    await phase('enabled-fail-closed', true, async (command, output) => {
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load audio_amp'), 'ok audio_amp');
        assert.equal(await command('start audio_amp'), 'failed');
        assert.equal(await command('amp-state'), 'failed errno 19');
        assert.equal(await command('amp-init'), 'failed errno 19');
        assert.equal(await command('amp-mute 1'), 'failed errno 19');
        assert.equal(await command('amp-balance 50'), 'failed errno 19');
        assert.equal(await command('stop audio_amp'), 'ok');
    });

    console.log('ok audio_amp module: disabled state/init/reload and enable=1 '
                + 'fails closed without publishing');
}

main().catch(e => { console.error(e); process.exitCode = 1; });
