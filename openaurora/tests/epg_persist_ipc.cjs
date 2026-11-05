/* Real Unix-socket coverage for the opt-in persistent EPG schedule cache.
 * Sections are built locally with a correct MPEG-2 CRC; no TV/vendor data. */
const fs = require('fs'), os = require('os'), path = require('path'), net = require('net');
const { spawn } = require('child_process'), assert = require('assert/strict');

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
function bcd(value) { return ((Math.floor(value / 10) << 4) | (value % 10)) & 0xff; }
function startBytes(mjd, hour, minute, second) {
    return [(mjd >> 8) & 0xff, mjd & 0xff, bcd(hour), bcd(minute), bcd(second)];
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

async function session(build, config, socket) {
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
    async function stop() {
        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
    }
    for (let i = 0; i < 300 && !fs.existsSync(socket); i++)
        await new Promise(r => setTimeout(r, 20));
    assert(fs.existsSync(socket), 'socket not created');
    return { command, commandAll, stop, closed };
}

const events = () => section({ events: [
    { id: 100, start: startBytes(60000, 12, 0, 0),
      duration: durationBytes(1, 0, 0), running: 4,
      desc: shortEvent('Persisted', 'One') },
    { id: 101, start: startBytes(60000, 13, 0, 0),
      duration: durationBytes(0, 30, 0), running: 2,
      desc: shortEvent('Second', 'Two') }
] });

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-epg-persist-'));
    const db = path.join(dir, 'epg.db');
    const config = path.join(dir, 'config');
    fs.writeFileSync(config, `autoload=\nui.fps=0\nui.ok_file=\nepg.path=${db}\n`);

    let run = await session(build, config, path.join(dir, 'control1'));
    try {
        assert.equal(await run.command('load epg'), 'ok epg');
        assert.equal(await run.command('start epg'), 'ok');
        assert.equal(await run.command('epg-count'), 'count 0');
        assert.equal(await run.command('epg-ingest ' + events().toString('hex')),
            'ok events 2');
        assert.equal(await run.command('epg-count'), 'count 2');
        assert.equal(await run.command('reload epg'), 'ok');
        assert.equal(await run.command('epg-count'), 'count 2');
        let reply = await run.commandAll('epg-query 7 1677326400000 1677331800000');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /event id=100 .*name=Persisted/);
        await run.stop();
    } finally {
        if (run && run.closed) {
            try { await bounded(run.closed, 1000); } catch { /* stopped above */ }
        }
    }

    run = await session(build, config, path.join(dir, 'control2'));
    try {
        assert.equal(await run.command('load epg'), 'ok epg');
        assert.equal(await run.command('start epg'), 'ok');
        assert.equal(await run.command('epg-count'), 'count 2');
        assert.equal(await run.command('epg-clear'), 'ok');
        assert.equal(await run.command('epg-count'), 'count 0');
        assert.equal(await run.command('epg-ingest ' + events().toString('hex')),
            'ok events 2');
        await run.stop();
    } finally {
        if (run && run.closed) {
            try { await bounded(run.closed, 1000); } catch { /* stopped above */ }
        }
    }

    run = await session(build, config, path.join(dir, 'control3'));
    try {
        assert.equal(await run.command('load epg'), 'ok epg');
        assert.equal(await run.command('start epg'), 'ok');
        assert.equal(await run.command('epg-count'), 'count 2');
        assert.equal(await run.command('epg-clear'), 'ok');
        assert.equal(await run.command('stop epg'), 'ok');
        assert.equal(await run.command('unload epg'), 'ok');
        fs.writeFileSync(db, 'OAEPG1\nbad\n');
        assert.match(await run.command('load epg'), /^ok/);
        assert.match(await run.command('start epg'), /^failed/);
        assert.equal(await run.command('epg-count'), 'failed errno 19');
        assert.equal(await run.command('unload epg'), 'ok');
        fs.writeFileSync(db, 'OAEPG1\n');
        assert.equal(await run.command('load epg'), 'ok epg');
        assert.equal(await run.command('start epg'), 'ok');
        assert.equal(await run.command('epg-count'), 'count 0');
        await run.stop();
    } finally {
        if (run && run.closed) {
            try { await bounded(run.closed, 1000); } catch { /* stopped above */ }
        }
    }

    assert.equal(fs.readFileSync(db, 'utf8'), 'OAEPG1\n');
    assert(!fs.existsSync(db + '.tmp'), 'temporary file left');
    fs.rmSync(dir, { recursive: true, force: true });
    console.log('ok EPG persistence IPC: reload/restart, clear, malformed fail-closed');
}
main().catch(e => { console.error(e); process.exitCode = 1; });
