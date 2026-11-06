/* ABI 3 async quiesce for mod_epg: unpublish epg/epg.ingest synchronously,
 * detach the schedule cache, then persist and free it on a worker. Reload
 * proves the persisted cache is reloaded by the replacement instance. */
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

function crc32(buf) {
    let crc = 0xffffffff;
    for (const byte of buf) {
        crc ^= byte << 24;
        for (let bit = 0; bit < 8; bit++)
            crc = (crc & 0x80000000) ? ((crc << 1) ^ 0x04c11db7) >>> 0
                                     : (crc << 1) >>> 0;
    }
    return crc >>> 0;
}

function bcd(value) {
    return ((Math.floor(value / 10) << 4) | (value % 10)) & 0xff;
}

function startBytes(mjd, hour, minute, second) {
    return [(mjd >> 8) & 0xff, mjd & 0xff,
        bcd(hour), bcd(minute), bcd(second)];
}

function durationBytes(hour, minute, second) {
    return [bcd(hour), bcd(minute), bcd(second)];
}

function shortEvent(name, text) {
    const n = Buffer.from(name), t = Buffer.from(text);
    const payload = Buffer.concat([Buffer.from('eng'),
        Buffer.from([n.length]), n, Buffer.from([t.length]), t]);
    return Buffer.concat([Buffer.from([0x4d, payload.length]), payload]);
}

function section({ table = 0x4e, service = 7, version = 3, current = 1,
    events = [] }) {
    const body = [];
    const put16 = value => body.push((value >> 8) & 0xff, value & 0xff);
    put16(service);
    body.push(((version & 0x1f) << 1) | (current ? 1 : 0));
    body.push(0, 0);
    put16(0x1234);
    put16(0x5678);
    body.push(0);
    body.push(table);
    for (const event of events) {
        put16(event.id);
        body.push(...event.start, ...event.duration);
        const desc = event.desc || Buffer.alloc(0);
        body.push(((event.running & 7) << 5) | ((event.freeCa ? 1 : 0) << 4) |
            ((desc.length >> 8) & 0x0f));
        body.push(desc.length & 0xff);
        body.push(...desc);
    }
    const sectionLength = body.length + 4;
    const header = [table, 0xb0 | ((sectionLength >> 8) & 0x0f),
        sectionLength & 0xff];
    const pre = Buffer.from([...header, ...body]);
    const crc = crc32(pre);
    return Buffer.concat([pre, Buffer.from([(crc >>> 24) & 0xff,
        (crc >>> 16) & 0xff, (crc >>> 8) & 0xff, crc & 0xff])]);
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'modules', 'mod_epg.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-epg-quiesce-'));
    const cache = path.join(dir, 'epg.cache');
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_epg.so'));
    fs.writeFileSync(config,
        `autoload=\nepg.path=${cache}\n` +
        'epg.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load epg'), 'ok epg');
        assert.equal(await command('start epg'), 'ok');
        assert.equal(await command('epg-count'), 'count 0');

        const first = section({ events: [
            { id: 100, start: startBytes(60000, 12, 0, 0),
              duration: durationBytes(1, 0, 0), running: 4,
              desc: shortEvent('Hello', 'World') },
            { id: 101, start: startBytes(60000, 13, 0, 0),
              duration: durationBytes(0, 30, 0), running: 2,
              desc: shortEvent('Second', 'Text') }
        ] });
        assert.equal(await command('epg-ingest ' + first.toString('hex')),
            'ok events 2');
        assert.equal(await command('epg-count'), 'count 2');

        const reload = await timed('reload epg');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status epg');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('epg-count'), 'failed errno 19',
            'both epg services must be unpublished while the cache is saved');

        await new Promise(resolve => setTimeout(resolve, 600));
        const persisted = fs.readFileSync(cache, 'utf8');
        assert.match(persisted, /^OAEPG1\n/);
        assert.equal(persisted.trim().split('\n').length, 3,
            'worker must persist both events before the retry');
        assert.equal(await command('reload epg'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('epg-count'), 'count 2',
            'the replacement must reload the persisted cache');

        let reply = await commandAll('epg-query 7 1677326400000 1677331800000');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /event id=100 service=7 start=1677326400000 end=1677330000000 .*name=Hello/);
        assert.match(reply, /event id=101 service=7 start=1677330000000 end=1677331800000 .*name=Second/);

        assert.equal(await command('epg-clear'), 'ok');
        assert.equal(await command('epg-count'), 'count 0');

        const stop = await timed('stop epg');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('epg-count'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop epg'), 'ok');
        assert.equal(await command('unload epg'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log(`ok epg quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'cache persisted/reloaded, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
