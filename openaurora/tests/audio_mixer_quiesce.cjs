/* ABI 3 async quiesce for mod_audio_mixer: stop/reload unpublishes the
 * service and detaches the broker channel, then closes it on a worker while
 * the reap timer retires the child. Suspend keeps the service published. */
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
    const module = path.join(build, 'modules', 'mod_audio_mixer.so');
    const peer = path.join(build, 'tests', 'mixer_peer');
    for (const f of [module, peer]) assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-mixer-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_audio_mixer.so'));
    fs.writeFileSync(config,
        `autoload=\naudio_mixer.broker=${peer}\n` +
        'audio_mixer.cleanup_delay_ms=400\n');

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

    async function collect(device) {
        const reply = await command(`mixer-query ${device}`);
        const match = /^ticket (\d+)$/.exec(reply);
        assert.ok(match, `mixer-query: ${reply}`);
        const ticket = match[1];
        for (let i = 0; i < 200; i++) {
            const result = await command(`mixer-result ${ticket}`);
            if (result.startsWith('result ')) return result;
            assert.ok(result.startsWith('pending '), `mixer-result: ${result}`);
            await new Promise(r => setTimeout(r, 10));
        }
        throw Error(`no mixer result for ${ticket}`);
    }

    function children() {
        const out = [];
        for (const name of fs.readdirSync('/proc')) {
            if (!/^\d+$/.test(name)) continue;
            try {
                const stat = fs.readFileSync(`/proc/${name}/stat`, 'utf8');
                const ppid = Number(stat.slice(stat.lastIndexOf(')') + 2).split(' ')[1]);
                if (ppid === child.pid) out.push(Number(name));
            } catch {}
        }
        return out;
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
        assert.equal(await command('load audio_mixer'), 'ok audio_mixer');
        assert.equal(await command('start audio_mixer'), 'ok');
        assert.match(await collect(0), /error 0 device 0 left 123 right 456$/);

        const first = await timed('reload audio_mixer');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await timed('status audio_mixer');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('mixer-query 0'), 'failed errno 19',
            'service must be unpublished while the worker closes the channel');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload audio_mixer'), 'ok',
            'retry must complete the reload');
        assert.match(await collect(0), /error 0 device 0 left 123 right 456$/,
            'reloaded mixer must have a fresh broker channel');

        const stop = await timed('stop audio_mixer');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('mixer-query 0'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop audio_mixer'), 'ok');
        assert.equal(await command('unload audio_mixer'), 'ok');
        assert.deepEqual(children(), [], 'broker child must be reaped before quit');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started audio_mixer/);
        console.log(`ok audio_mixer quiesce: reload fails fast (${first.elapsed}ms), ` +
                    'broker republished and reaped, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
