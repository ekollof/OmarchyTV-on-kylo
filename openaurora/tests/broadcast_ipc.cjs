/* Real Unix-socket lifecycle for the tuner/epg/pvr fixture contracts. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-broadcast-ipc-'));
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config, 'autoload=\nui.fps=0\nui.ok_file=\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'tests'), '--config', config,
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
    /* Multi-line replies keep the connection open; settle after a brief quiet
     * period once the server has stopped writing. */
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

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load broadcast_fixture'), 'ok broadcast_fixture');
        assert.equal(await command('start broadcast_fixture'), 'ok');

        assert.equal(await command('tuner-state'),
            'network 0 state 0 progress 0 services 0 selected 0 error 0');
        assert.equal(await command('tuner-scan 0'), 'failed errno 22');
        assert.equal(await command('tuner-scan 6'), 'failed errno 22');
        assert.equal(await command('tuner-scan 1'), 'ok');
        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 25 services 3 selected 0 error 0');
        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 50 services 6 selected 0 error 0');
        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 75 services 9 selected 0 error 0');
        assert.equal(await command('tuner-state'),
            'network 1 state 1 progress 100 services 12 selected 0 error 0');
        assert.equal(await command('tuner-state'),
            'network 1 state 2 progress 100 services 12 selected 0 error 0');
        assert.equal(await command('tuner-select 13'), 'failed errno 22');
        assert.equal(await command('tuner-select 0'), 'failed errno 22');
        assert.equal(await command('tuner-select 5'), 'ok');
        assert.match(await command('tuner-state'), / selected 5 error 0$/);

        assert.equal(await command('tuner-scan 2'), 'ok');
        assert.match(await command('tuner-state'), / selected 0 error 0$/);
        assert.equal(await command('tuner-cancel'), 'ok');
        assert.equal(await command('tuner-state'),
            'network 2 state 3 progress 25 services 3 selected 0 error 0');
        assert.equal(await command('tuner-cancel'), 'failed errno 22');

        let reply = await commandAll('epg-query 1 0 4000');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /event id=100 service=1 start=1000 end=2000 name=Alpha News/);
        assert.match(reply, /event id=101 service=1 start=2000 end=3000 name=Alpha Movie/);
        assert.equal((await commandAll('epg-query 0 0 10000')).split('\n')[0], 'count 4');
        assert.equal((await commandAll('epg-query 99 0 10000')).split('\n')[0], 'count 0');
        assert.equal(await command('epg-query 1 4000 4000'), 'failed errno 22');
        assert.equal(await command('epg-query 1 5000 4000'), 'failed errno 22');

        assert.equal(await command('pvr-list'), 'count 0');
        assert.equal(await command('pvr-add 1 1000 2000'), 'id 1');
        assert.equal(await command('pvr-add 2 3000 4000'), 'id 2');
        assert.equal(await command('pvr-add 0 1000 2000'), 'failed errno 22');
        assert.equal(await command('pvr-add 1 2000 1000'), 'failed errno 22');
        reply = await commandAll('pvr-list');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /job id=1 service=1 state=0 start=1000 end=2000 name=cli reservation/);
        assert.match(reply, /job id=2 service=2 state=0 start=3000 end=4000 name=cli reservation/);
        assert.equal(await command('pvr-cancel 1'), 'ok');
        assert.equal(await command('pvr-cancel 1'), 'failed errno 114');
        assert.equal(await command('pvr-cancel 99'), 'failed errno 2');
        assert.match(await commandAll('pvr-list'), /job id=1 service=1 state=3 start=1000/);

        /* Missing/invalid arity must not reach the module. */
        assert.equal(await command('tuner-state extra'), 'failed errno 22');
        assert.equal(await command('epg-query 1 0'), 'failed errno 22');
        assert.equal(await command('pvr-add 1 1000'), 'failed errno 22');

        assert.equal(await command('reload broadcast_fixture'), 'ok');
        assert.equal(await command('tuner-state'),
            'network 0 state 0 progress 0 services 0 selected 0 error 0');
        assert.equal(await command('pvr-list'), 'count 0');

        assert.equal(await command('stop broadcast_fixture'), 'ok');
        assert.equal(await command('tuner-state'), 'failed errno 19');
        assert.equal(await command('epg-query 1 0 4000'), 'failed errno 19');
        assert.equal(await command('pvr-list'), 'failed errno 19');
        assert.equal(await command('unload broadcast_fixture'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok broadcast IPC: tuner scan/select, EPG query and PVR add/list/cancel over the control socket');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
