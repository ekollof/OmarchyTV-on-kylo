/* Real Unix-socket coverage for the EIT-backed epg module. Sections are built
 * locally with a correct MPEG-2 CRC; no TV or vendor data is used. */
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
    body.push(0, 0);          // section_number, last_section_number
    put16(0x1234);            // transport_stream_id
    put16(0x5678);            // original_network_id
    body.push(0);             // segment_last_section_number
    body.push(table);         // last_table_id
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-epg-ipc-'));
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

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load epg'), 'ok epg');
        assert.equal(await command('start epg'), 'ok');
        assert.equal(await command('epg-count'), 'count 0');
        assert.equal((await commandAll('epg-query 7 1677326400000 1677331800000'))
            .split('\n')[0], 'count 0');

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

        let reply = await commandAll('epg-query 7 1677326400000 1677331800000');
        assert.match(reply, /^count 2\n/);
        assert.match(reply, /event id=100 service=7 start=1677326400000 end=1677330000000 crid=0 first=0 state=0 version=3 actual=1 running=4 name=Hello/);
        assert.match(reply, /event id=101 service=7 start=1677330000000 end=1677331800000 crid=0 first=0 state=0 version=3 actual=1 running=2 name=Second/);
        assert.equal((await commandAll('epg-query 8 0 2000000000000')).split('\n')[0], 'count 0');
        assert.equal((await commandAll('epg-query 7 0 1677326400000')).split('\n')[0], 'count 0');

        assert.match(await command('epg-nownext 7 1677328200000'),
            /^now id=100 name='Hello' next id=101 name='Second'/);
        assert.match(await command('epg-nownext 7 1677326400000'),
            /^now id=100 name='Hello' next id=101 /);
        assert.match(await command('epg-nownext 7 1677330900000'),
            /^now id=101 name='Second' next -/);
        assert.match(await command('epg-nownext 7 1677322800000'),
            /^now - next id=100 name='Hello'/);
        assert.match(await command('epg-nownext 7 1677331800000'),
            /^now - next -/);
        assert.equal(await command('epg-nownext 7'), 'failed errno 22');
        assert.equal(await command('epg-nownext 0 1'), 'failed errno 22');
        assert.equal(await command('epg-nownext 7 -1'), 'failed errno 22');
        assert.equal(await command('epg-nownext 7 1 x'), 'failed errno 22');

        /* Same (service,event) keys replace rather than duplicate. */
        const replaced = section({ version: 4, events: [
            { id: 100, start: startBytes(60000, 12, 0, 0),
              duration: durationBytes(1, 0, 0), running: 1,
              desc: shortEvent('Hello2', 'World2') }
        ] });
        assert.equal(await command('epg-ingest ' + replaced.toString('hex')),
            'ok events 1');
        assert.equal(await command('epg-count'), 'count 2');
        reply = await commandAll('epg-query 7 1677326400000 1677330000000');
        assert.match(reply, /event id=100 .*version=4 actual=1 running=1 name=Hello2/);

        /* Bad CRC and non-current sections do not change the cache. */
        const bad = Buffer.from(first);
        bad[bad.length - 1] ^= 1;
        assert.equal(await command('epg-ingest ' + bad.toString('hex')),
            'failed errno 22');
        assert.equal(await command('epg-count'), 'count 2');
        const stale = section({ current: 0, events: [
            { id: 102, start: startBytes(60000, 14, 0, 0),
              duration: durationBytes(0, 10, 0), running: 0 }
        ] });
        assert.equal(await command('epg-ingest ' + stale.toString('hex')),
            'ok events 0');
        assert.equal(await command('epg-count'), 'count 2');

        assert.equal(await command('epg-ingest zz'), 'failed errno 22');
        assert.equal(await command('epg-clear'), 'ok');
        assert.equal(await command('epg-count'), 'count 0');
        assert.match(await command('epg-nownext 7 1677328200000'),
            /^now - next -/);

        /* Reload restarts the module and clears the in-memory schedule. */
        assert.equal(await command('reload epg'), 'ok');
        assert.equal(await command('epg-count'), 'count 0');

        assert.equal(await command('stop epg'), 'ok');
        assert.equal(await command('epg-query 7 0 2000000000'), 'failed errno 19');
        assert.equal(await command('epg-count'), 'failed errno 19');
        assert.equal(await command('epg-nownext 7 1677328200000'), 'failed errno 19');
        assert.equal(await command('unload epg'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok epg IPC: EIT ingest, key replacement, query/clear and reload over the control socket');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
