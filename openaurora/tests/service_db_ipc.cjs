/* Real Unix-socket lifecycle for the service_db provider. */
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
    const root = path.resolve(__dirname, '..', '..');
    const db = path.join(root, 'artifacts/dumps/apdat_files/sqlite_db/settingsDB.db');
    if (!fs.existsSync(db)) {
        console.error('skipping: settingsDB.db unavailable');
        process.exit(77);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-service-db-ipc-'));
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config, `autoload=\nservice_db.path=${db}\nui.fps=0\nui.ok_file=\n`);
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-16384); });
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
        assert.equal(await command('load service_db'), 'ok service_db');
        assert.equal(await command('start service_db'), 'ok');
        stdout = '';
        child.stdin.write('status service_db\n');
        await waitFor(() => stdout.includes('service.list:v1') && stdout.includes('state   started'),
            'service.list published');
        assert.equal(await command('service-list'), 'count 525');
        assert.match(await command('service-list stats'),
            /^count 525 dvb 445 easi 67 analog 13 /);
        const row = await command('service-list 0');
        assert.match(row, /^source=dvb /);
        assert.match(row, / visible=/);
        assert.match(row, / name=/);
        assert.match(await command('service-list name 0'), /^source=.* name=/);
        assert.match(await command('service-list channel 0'), /^source=.* name=/);

        assert.equal(await command('reload service_db'), 'ok');
        assert.equal(await command('service-list'), 'count 525');
        assert.match(await command('service-list 0'), /^source=dvb /);

        assert.equal(await command('stop service_db'), 'ok');
        assert.equal(await command('service-list'), 'failed errno 19');
        assert.equal(await command('unload service_db'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok service_db IPC: 525-row inventory, service publication, reload, stop/unload');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
