/* Real mod_source (fake HAL) over the control socket: drives the gated
 * TunerManager display-select, verifies queued/completed effects and the
 * effect worker's reload/stop lifecycle. No TV, /dev or vendor library. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-tuner-select-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    const vin = path.join(dir, 'vin.log');
    fs.mkdirSync(modules);
    fs.copyFileSync(path.join(build, 'modules', 'mod_plat.so'),
        path.join(modules, 'mod_plat.so'));
    fs.copyFileSync(path.join(build, 'tests', 'mod_source_fake.so'),
        path.join(modules, 'mod_source.so'));
    fs.writeFileSync(config, [
        'autoload=', 'plat.enable=1', 'source.enable=0',
        'source.cleanup_delay_ms=200',
        'source.tuner_select_effects=1', 'source.tuner_select_sink=1',
        'source.video_mute=1', ''
    ].join('\n'));

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

        const initial = await command('tuner-select-state');
        assert.match(initial, /^effects 0 sink 0 applied 0 queued 0 completed 0 sink_failed 0 completion_failed 0 unsupported 0 no_backend 0 dispatched 0$/);

        const select = await command('tuner-select-display 1');
        assert.match(select,
            /^ran 1 pre_mute 1 unmute 1 show 1 reject 0$/);
        const bad = await command('tuner-select-display 9');
        assert.equal(bad, 'failed errno 22');

        /* HAL calls are queued on the worker, not applied inline; status is
         * the loop-side collection point. */
        let state = await command('tuner-select-state');
        let m = state.match(/^effects (\d+) sink (-?\d+) applied (\d+) queued (\d+) completed (\d+) sink_failed (\d+) completion_failed (\d+) unsupported (\d+) no_backend (\d+) dispatched (\d+)$/);
        assert.ok(m, state);
        assert.ok(Number(m[1]) > 0, 'effects were emitted');
        assert.equal(Number(m[2]), 0, 'sink status available');
        assert.ok(Number(m[4]) >= 2, `hide/show were queued: ${state}`);
        assert.ok(Number(m[8]) > 0, 'unsupported effects counted');
        /* No audio service is loaded in this fixture, so the two mute
         * effects must be rejected rather than faked as applied. */
        assert.equal(Number(m[3]), 0, 'nothing applied inline');
        assert.ok(Number(m[6]) >= 2, 'mute effects without a service failed');
        for (let i = 0; i < 100 && Number(m[5]) < 2; i++) {
            await new Promise(r => setTimeout(r, 20));
            state = await command('tuner-select-state');
            m = state.match(/^effects (\d+) sink (-?\d+) applied (\d+) queued (\d+) completed (\d+) sink_failed (\d+) completion_failed (\d+) unsupported (\d+) no_backend (\d+) dispatched (\d+)$/);
            assert.ok(m, state);
        }
        assert.ok(Number(m[5]) >= 2, 'queued effects completed on the worker');
        assert.equal(Number(m[7]), 0, 'no completion failures');
        const trace = log();
        assert.ok(trace.includes('vm dev idx=0'), 'DRM chain resolved the device');
        assert.ok(trace.includes('vm path=0'), 'main video path used');
        assert.ok(trace.includes('vm mute fd=17 plane=42 en=1 on=1'),
            'hide video drove the DRM mute');
        assert.ok(trace.includes('vm mute fd=17 plane=42 en=1 on=0'),
            'show video restored the DRM mute');

        /* Worker lifecycle: reload and stop drain and join cleanly. */
        let reply = await command('reload source');
        if (reply.startsWith('failed')) {
            await new Promise(r => setTimeout(r, 400));
            reply = await command('reload source');
        }
        assert.equal(reply, 'ok', 'reload completed after worker quiesce');
        assert.equal((await command('tuner-select-display 1')).startsWith('ran 1'), true);
        reply = await command('stop source');
        if (reply.startsWith('failed')) {
            await new Promise(r => setTimeout(r, 400));
            reply = await command('stop source');
        }
        assert.equal(reply, 'ok', 'stop completed after worker quiesce');
        assert.equal(await command('unload source'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log('ok tuner select ipc: gated display-select, queued/completed '
            + 'effects, DRM mute trace and worker reload/stop lifecycle');
    } catch (e) {
        cleanup();
        throw e;
    } finally {
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

bounded(main(), 60000).catch(e => { console.error(e); process.exit(1); });
