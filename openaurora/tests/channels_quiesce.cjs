/* ABI 3 async quiesce for mod_channels: unpublish channels synchronously,
 * detach the heap-owned runtime list, then destroy it synchronously or on a
 * worker. The reload must start with an empty list again. */
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
    const module = path.join(build, 'modules', 'mod_channels.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-channels-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_channels.so'));
    fs.writeFileSync(config,
        'autoload=\nchannels.cleanup_delay_ms=400\n');

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

    const xml = Buffer.from(
        "<active_list network='Cable' count='2' total_size='2'>" +
        "<service rsn='1' name='Alpha &amp; Beta' handle='11' onid='1' tsid='2' sid='3' type='1'/>" +
        "<service rsn='2' name='O&apos;Neil' handle='12' onid='4' tsid='5' sid='6' type='2'/>" +
        "</active_list>");

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
        assert.equal(await command('load channels'), 'ok channels');
        assert.equal(await command('start channels'), 'ok');
        assert.equal(await command('channel-state'),
            'mode stored count 0 lcn unknown visibility unknown');
        assert.equal(await command('channel-ingest ' + xml.toString('hex')),
            'ok channels 2');
        assert.equal(await command('channel-state'),
            'mode runtime count 2 lcn unknown visibility unknown');

        const reload = await timed('reload channels');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status channels');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('channel-state'), 'failed errno 19',
            'service must be unpublished while the list is detached');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload channels'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('channel-state'),
            'mode stored count 0 lcn unknown visibility unknown',
            'the replacement must start with an empty runtime list');

        assert.equal(await command('channel-ingest ' + xml.toString('hex')),
            'ok channels 2');
        let reply = await commandAll('channel-list');
        assert.match(reply, /^source runtime count 2 lcn unknown visibility unknown\n/);
        assert.match(reply, /row rsn=1 handle=11 name=Alpha & Beta/);
        assert.match(reply, /row rsn=2 handle=12 name=O'Neil/);

        const stop = await timed('stop channels');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('channel-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop channels'), 'ok');
        assert.equal(await command('unload channels'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started channels/);
        console.log(`ok channels quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'list detached, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
