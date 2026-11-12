/* Real Unix-socket lifecycle for the plat-dependent host modules:
 * audio, source, ir and panel, using a host-only plat service fixture.
 * No HAL, GPIO, I2C, UART, PWM or TV is touched. */
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
    const fixture = path.join(build, 'tests/mod_plat_fixture.so');
    const names = ['mod_plat_fixture.so', 'mod_audio.so', 'mod_source.so',
        'mod_ir.so', 'mod_panel.so'];
    if (!names.every(n => fs.existsSync(n === 'mod_plat_fixture.so' ? fixture
            : path.join(build, 'modules', n)))) {
        console.error('skipping: plat fixture or consumer modules unavailable');
        process.exit(77);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-plat-ipc-'));
    const modules = path.join(dir, 'modules');
    fs.mkdirSync(modules);
    for (const name of names) {
        const source = name === 'mod_plat_fixture.so' ? fixture
            : path.join(build, 'modules', name);
        fs.symlinkSync(source, path.join(modules, name));
    }
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config,
        'autoload=\n' +
        'audio.enable=0\nsource.enable=0\nir.device=\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-32768); });
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-32768); });
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
    async function waitFor(predicate, label) {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (predicate()) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('timeout: ' + label)); }, 6000);
        }), 7000);
    }
    async function retryCommand(text, expected = 'ok', tries = 40) {
        let last = '';
        for (let i = 0; i < tries; i++) {
            last = await command(text);
            if (last === expected) return last;
            await new Promise(r => setTimeout(r, 100));
        }
        return last;
    }
    async function status(name, ...needles) {
        stdout = '';
        child.stdin.write(`status ${name}\n`);
        await waitFor(() => needles.every(n => stdout.includes(n)),
            `${name} status: ${needles.join(', ')}`);
    }
    const consumers = [
        ['audio', 'audio:v1'],
        ['source', 'source:v3'],
        ['ir', 'input:v1'],
        ['panel', 'panel:v1']
    ];

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load plat_fixture'), 'ok plat_fixture');
        assert.equal(await command('start plat_fixture'), 'ok');
        await status('plat_fixture', 'state   started', 'plat:v1');

        for (const [name] of consumers) {
            assert.equal(await command('load ' + name), 'ok ' + name);
            assert.equal(await command('start ' + name), 'ok');
            await status(name, 'state   started', 'plat');
        }
        for (const [name, service] of consumers) await status(name, service);

        /* Consumers require/hold plat, so the provider cannot be reloaded live. */
        assert.equal(await command('reload plat_fixture'), 'failed');
        await status('plat_fixture', 'state   started', 'plat:v1');

        assert.equal(await retryCommand('stop audio'), 'ok');
        assert.equal(await command('stop source'), 'ok');
        assert.equal(await command('stop ir'), 'ok');
        assert.equal(await command('stop panel'), 'ok');
        assert.equal(await retryCommand('reload plat_fixture'), 'ok');
        await status('plat_fixture', 'state   started', 'plat:v1');

        for (const [name] of consumers) {
            assert.equal(await command('start ' + name), 'ok');
            await status(name, 'state   started');
        }
        for (const [name, service] of consumers) await status(name, service);

        assert.equal(await retryCommand('reload audio'), 'ok');
        await status('audio', 'state   started', 'audio:v1');
        for (const name of ['source', 'ir', 'panel']) {
            assert.equal(await command('reload ' + name), 'ok');
            await status(name, 'state   started');
        }

        assert.equal(await retryCommand('stop audio'), 'ok');
        for (const name of ['source', 'ir', 'panel']) {
            assert.equal(await command('stop ' + name), 'ok');
        }
        assert.equal(await retryCommand('stop plat_fixture'), 'ok');
        for (const name of ['audio', 'source', 'ir', 'panel', 'plat_fixture']) {
            assert.equal(await retryCommand('unload ' + name), 'ok');
        }

        assert.equal(await command('load plat_fixture'), 'ok plat_fixture');
        assert.equal(await command('start plat_fixture'), 'ok');
        for (const [name] of consumers) {
            assert.equal(await command('load ' + name), 'ok ' + name);
            assert.equal(await command('start ' + name), 'ok');
        }
        await status('audio', 'state   started', 'audio:v1');
        await status('panel', 'state   started', 'panel:v1');
        assert.equal(await retryCommand('stop audio'), 'ok');
        for (const name of ['source', 'ir', 'panel']) {
            assert.equal(await command('stop ' + name), 'ok');
        }
        assert.equal(await retryCommand('stop plat_fixture'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual plat-consumer IPC: audio/source/ir/panel services, plat hold refusal, dependency-ordered reload and reuse');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
