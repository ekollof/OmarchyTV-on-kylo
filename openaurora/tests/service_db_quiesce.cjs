/* ABI 3 async quiesce for mod_service_db: unpublish service.list
 * synchronously, detach the inventory row array, then free it synchronously or
 * on a worker. The reload re-reads the same settingsDB inventory. */
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
    const root = path.resolve(__dirname, '..', '..');
    const db = path.join(root, 'artifacts/dumps/apdat_files/sqlite_db/settingsDB.db');
    if (!fs.existsSync(db)) {
        console.error('skipping: settingsDB.db unavailable');
        process.exit(77);
    }
    const module = path.join(build, 'modules', 'mod_service_db.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-service-db-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_service_db.so'));
    fs.writeFileSync(config,
        `autoload=\nservice_db.path=${db}\n` +
        'service_db.cleanup_delay_ms=400\n');

    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'], { stdio: ['pipe', 'pipe', 'pipe'] });
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

    async function timed(text) {
        const started = Date.now();
        const reply = await command(text);
        return { reply, elapsed: Date.now() - started };
    }

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
        assert.equal(await command('load service_db'), 'ok service_db');
        assert.equal(await command('start service_db'), 'ok');
        assert.equal(await command('service-list'), 'count 525');
        assert.match(await command('service-list stats'),
            /^count 525 dvb 445 easi 67 analog 13 /);

        const reload = await timed('reload service_db');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status service_db');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('service-list'), 'failed errno 19',
            'service must be unpublished while the rows are detached');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload service_db'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('service-list'), 'count 525',
            'the replacement must re-read the inventory');
        assert.match(await command('service-list 0'), /^source=dvb /);
        assert.match(await command('service-list name 0'), /^source=.* name=/);

        const stop = await timed('stop service_db');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('service-list'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop service_db'), 'ok');
        assert.equal(await command('unload service_db'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /service_db: 525 rows/);
        console.log(`ok service_db quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'inventory republished, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
