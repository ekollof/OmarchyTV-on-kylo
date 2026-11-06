/* Real mod_source over a fake Hal_Vin fixture. Proves the decoded
 * source-type -> VIN input-id table and the source.enable gate over the
 * control socket. No TV, /dev/ntkdvif or vendor HAL. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-source-fake-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    let socket = path.join(dir, 'control');
    const vin = path.join(dir, 'vin.log');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'modules', 'mod_plat.so'),
        path.join(modules, 'mod_plat.so'));
    fs.copyFileSync(path.join(build, 'tests', 'mod_source_fake.so'),
        path.join(modules, 'mod_source.so'));
    const write_config = enable => fs.writeFileSync(config,
        `autoload=\nplat.enable=1\nsource.enable=${enable ? 1 : 0}\n`);
    write_config(true);

    let child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'],
        { stdio: ['pipe', 'pipe', 'pipe'],
          env: { ...process.env, FAKE_VIN_LOG: vin } });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-65536); });
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-65536); });
    const closed = new Promise((resolve, reject) => {
        child.on('error', reject);
        child.on('close', (code, signal) => resolve({ code, signal }));
    });
    const cleanup = () => {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
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
                if (data.includes('\n')) finish(null, data.slice(0, data.indexOf('\n')));
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        }), 4000);
    }
    const log = () => fs.existsSync(vin) ? fs.readFileSync(vin, 'utf8') : '';

    try {
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                if (fs.existsSync(socket)) { clearInterval(timer); resolve(); }
                else if (Date.now() - started > 4000) {
                    clearInterval(timer); reject(Error('socket timeout'));
                }
            }, 20);
        }), 5000);
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load source'), 'ok source');
        assert.equal(await command('start source'), 'ok');
        assert.equal(await command('source-state'),
            'source type -1 available 1');

        const cases = [
            [17, 3],  /* HDMI1 */
            [18, 3],  /* HDMI2 */
            [19, 66], /* HDMI3 */
            [15, 1],  /* BACK AV / CVBS */
            [16, 2],  /* VGA/PC */
            [20, 3],  /* YPbPr */
            [0, 1],   /* TV */
        ];
        for (const [type, id] of cases) {
            assert.equal(await command(`source-select ${type}`), 'ok');
            assert.equal(await command('source-state'),
                `source type ${type} available 1`);
            assert.ok(log().includes(`select fd=100 id=${id}`),
                `missing VIN id ${id} for type ${type}`);
        }
        assert.equal(await command('source-select 99'), 'failed errno 22');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);

        /* source.enable is read at module start from the host's cached
         * config, so prove the gate in a fresh process. */
        write_config(false);
        socket = path.join(dir, 'control-disabled');
        child = spawn(path.join(build, 'openaurora'),
            ['--modules', modules, '--config', config, '--sock', socket,
             '--log', '3'],
            { stdio: ['pipe', 'pipe', 'pipe'],
              env: { ...process.env, FAKE_VIN_LOG: vin } });
        child.stdout.on('data', d => { stdout = (stdout + d).slice(-65536); });
        child.stderr.on('data', d => { stderr = (stderr + d).slice(-65536); });
        const closed2 = new Promise((resolve, reject) => {
            child.on('error', reject);
            child.on('close', (code, signal) => resolve({ code, signal }));
        });
        await bounded(new Promise((resolve, reject) => {
            const started = Date.now();
            const timer = setInterval(() => {
                if (fs.existsSync(socket)) { clearInterval(timer); resolve(); }
                else if (Date.now() - started > 4000) {
                    clearInterval(timer); reject(Error('socket timeout'));
                }
            }, 20);
        }), 5000);
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load source'), 'ok source');
        assert.equal(await command('start source'), 'ok');
        assert.equal(await command('source-select 17'), 'failed disabled');
        assert.equal(await command('source-state'),
            'source type -1 available 1');
        child.kill('SIGTERM');
        const exit2 = await bounded(closed2, 8000);
        assert.equal(exit2.code, 0, stderr);
        const text = log();
        assert.ok((text.match(/open idx=0/g) || []).length >= 2,
            'source must reopen the VIN device after reload');
        assert.ok(text.includes('close fd=100'));
        console.log('ok source fake-VIN IPC: decoded ids, state query and source.enable gate');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
