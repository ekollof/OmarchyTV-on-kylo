/* Real Unix-socket coverage for the runtime active-list module. XML is built
 * locally in the recovered ACTIVECHANNELLIST shape; no TV or runtime EASI. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-channels-ipc-'));
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

    const xml = Buffer.from(
        "<active_list network='Cable' count='2' total_size='2'>" +
        "<service rsn='1' name='Alpha &amp; Beta' handle='11' onid='1' tsid='2' sid='3' type='1'/>" +
        "<service rsn='2' name='O&apos;Neil' handle='12' onid='4' tsid='5' sid='6' type='2'/>" +
        "</active_list>");
    const malformed = Buffer.from("<active_list><service rsn='1' name='X' handle='1' onid='1' tsid='1' sid='1' type='1'/>");

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load channels'), 'ok channels');
        assert.equal(await command('start channels'), 'ok');
        assert.equal(await command('channel-state'),
            'mode stored count 0 lcn unknown visibility unknown');
        assert.equal(await command('channel-list'), 'source stored count 0');

        assert.equal(await command('channel-ingest ' + xml.toString('hex')),
            'ok channels 2');
        assert.equal(await command('channel-state'),
            'mode runtime count 2 lcn unknown visibility unknown');
        let reply = await commandAll('channel-list');
        assert.match(reply, /^source runtime count 2 lcn unknown visibility unknown\n/);
        assert.match(reply, /row rsn=1 handle=11 name=Alpha & Beta onid=1 tsid=2 sid=3 type=1/);
        assert.match(reply, /row rsn=2 handle=12 name=O'Neil onid=4 tsid=5 sid=6 type=2/);

        /* A malformed replacement must not discard the active list. */
        assert.equal(await command('channel-ingest ' + malformed.toString('hex')),
            'failed errno 22');
        assert.equal(await command('channel-state'),
            'mode runtime count 2 lcn unknown visibility unknown');
        assert.equal(await command('channel-ingest zz'), 'failed errno 22');

        assert.equal(await command('channel-clear'), 'ok');
        assert.equal(await command('channel-state'),
            'mode stored count 0 lcn unknown visibility unknown');

        assert.equal(await command('channel-ingest ' + xml.toString('hex')),
            'ok channels 2');
        assert.equal(await command('reload channels'), 'ok');
        assert.equal(await command('channel-state'),
            'mode stored count 0 lcn unknown visibility unknown');

        assert.equal(await command('stop channels'), 'ok');
        assert.equal(await command('channel-state'), 'failed errno 19');
        assert.equal(await command('unload channels'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 10000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok channels IPC: runtime XML order, replacement guards, clear and reload');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 10000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
