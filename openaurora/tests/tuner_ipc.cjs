/* Real Unix-socket coverage for mod_tuner (offline NIM stub) and its EIT
 * handoff to mod_epg. No hardware or vendor calls are involved. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-tuner-ipc-'));
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config, 'autoload=\nui.fps=0\nui.ok_file=\n');
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
    async function commandAll(text) {
        return bounded(new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '', done = false, idle = null;
            const finish = (error, value) => {
                if (done) return;
                done = true; if (idle) clearTimeout(idle); s.destroy();
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text + '\n'));
            s.on('error', e => finish(e));
            s.on('end', () => finish(null, data));
            s.on('data', d => {
                data += d;
                if (idle) clearTimeout(idle);
                idle = setTimeout(() => finish(null, data), 120);
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        }), 5000);
    }
    async function waitFor(predicate, label) {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (predicate()) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('timeout: ' + label)); }, 6000);
        }), 7000);
    }
    const UNKNOWN = '4294967295';

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load epg'), 'ok epg');
        assert.equal(await command('start epg'), 'ok');
        assert.equal(await command('load tuner'), 'ok tuner');
        assert.equal(await command('start tuner'), 'ok');

        assert.equal(await command('tuner-state'),
            'network 0 state 0 progress 0 lock 0 services 0 selected 0 error 0 ' +
            `strength 0 quality 0 ber 0.00 snr 0.00 unc ${UNKNOWN}`);
        assert.equal(await command('tuner-scan 0'), 'failed errno 22');
        assert.equal(await command('tuner-scan 4'), 'failed errno 95');
        assert.equal(await command('tuner-scan 6'), 'failed errno 22');
        assert.equal(await command('tuner-scan 1'), 'ok');

        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 25 lock 0 services 3 selected 0 error 0 ' +
            `strength 30 quality 10 ber 0.00 snr 0.00 unc ${UNKNOWN}`);
        /* The stub EIT output is pushed to epg.ingest during the scan. */
        assert.equal(await command('epg-count'), 'count 12');

        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 50 lock 1 services 6 selected 0 error 0 ' +
            'strength 80 quality 75 ber 0.00 snr 20.50 unc 0');
        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 75 lock 1 services 9 selected 0 error 0 ' +
            'strength 80 quality 75 ber 0.00 snr 20.50 unc 0');
        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 100 lock 1 services 12 selected 0 error 0 ' +
            'strength 80 quality 75 ber 0.00 snr 20.50 unc 0');
        assert.equal(await command('tuner-state'),
            'network 1 state 2 progress 100 lock 1 services 12 selected 0 error 0 ' +
            'strength 80 quality 75 ber 0.00 snr 20.50 unc 0');

        let reply = await commandAll('epg-query 101 1677326400000 1677330000000');
        assert.match(reply, /^count 1\n/);
        assert.match(reply, /event id=1001 service=101 start=1677326400000 end=1677328200000 crid=0 first=0 state=0 version=0 actual=1 running=4 name=Stub Service 1/);
        assert.equal((await commandAll('epg-query 0 1677326400000 1677330000000'))
            .split('\n')[0], 'count 12');

        assert.equal(await command('tuner-select 13'), 'failed errno 22');
        assert.equal(await command('tuner-select 0'), 'failed errno 22');
        assert.equal(await command('tuner-select 5'), 'ok');
        assert.match(await command('tuner-state'), / selected 5 error 0 /);

        assert.equal(await command('tuner-scan 2'), 'ok');
        assert.match(await command('tuner-state'), / selected 0 error 0 /);
        assert.equal(await command('epg-count'), 'count 12');
        assert.equal(await command('tuner-cancel'), 'ok');
        assert.equal(await command('tuner-state'),
            'network 2 state 3 progress 25 lock 0 services 3 selected 0 error 0 ' +
            `strength 30 quality 10 ber 0.00 snr 0.00 unc ${UNKNOWN}`);
        assert.equal(await command('tuner-cancel'), 'failed errno 22');

        /* The tuner still scans when epg.ingest is absent. */
        assert.equal(await command('stop epg'), 'ok');
        assert.equal(await command('tuner-scan 3'), 'ok');
        for (let i = 0; i < 5; i++) await command('tuner-state');
        assert.match(await command('tuner-state'),
            /^network 3 state 2 progress 100 lock 1 services 12 selected 0 error 0 /);

        assert.equal(await command('reload tuner'), 'ok');
        assert.equal(await command('tuner-state'),
            'network 0 state 0 progress 0 lock 0 services 0 selected 0 error 0 ' +
            `strength 0 quality 0 ber 0.00 snr 0.00 unc ${UNKNOWN}`);
        assert.equal(await command('tuner-state extra'), 'failed errno 22');

        assert.equal(await command('stop tuner'), 'ok');
        assert.equal(await command('tuner-state'), 'failed errno 19');
        assert.equal(await command('unload tuner'), 'ok');
        assert.equal(await command('unload epg'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok tuner IPC: NIM stub scan/signal, EIT handoff to epg, cancel/reload over the socket');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
