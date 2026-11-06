/* ABI 3 async quiesce for mod_ui_theme: unpublish ui.theme synchronously,
 * detach the parsed SVG pack, then free it synchronously or on a worker. The
 * reload must load the changed fixture instead of the detached pack. */
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

function record(x) {
    const b = Buffer.from([1, 0x72, 0, 0, 0, 0, 0, 7, 0, 9, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0]);
    b[3] = x;
    return b;
}

function fixture(index, binary, records) {
    fs.writeFileSync(binary, Buffer.concat(records.map(record)));
    fs.writeFileSync(index,
        `${records.length}\n` +
        Array(records.length).fill('0xe8eb982e 20').join(' ') + '\n');
}

async function main() {
    const build = path.resolve(process.argv[2] || 'openaurora/build');
    const module = path.join(build, 'modules', 'mod_ui_theme.so');
    assert.ok(fs.existsSync(module), `missing ${module}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-theme-quiesce-'));
    const index = path.join(dir, 'theme.index');
    const binary = path.join(dir, 'theme.bin');
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(module, path.join(modules, 'mod_ui_theme.so'));
    fixture(index, binary, [11]);
    fs.writeFileSync(config,
        'autoload=\n' +
        `ui_theme.index=${index}\nui_theme.binary=${binary}\n` +
        'ui_theme.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load ui_theme'), 'ok ui_theme');
        assert.equal(await command('start ui_theme'), 'ok');
        assert.equal(await command('theme-state'), 'theme count 1');

        const reload = await timed('reload ui_theme');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status ui_theme');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.equal(await command('theme-state'), 'failed errno 19',
            'service must be unpublished while the pack is detached');

        /* Change the on-disk pack while the old one is detached. */
        fixture(index, binary, [11, 66]);
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ui_theme'), 'ok',
            'retry must complete the reload');
        assert.equal(await command('theme-state'), 'theme count 2',
            'the replacement must load the changed pack');

        const stop = await timed('stop ui_theme');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.equal(await command('theme-state'), 'failed errno 19');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ui_theme'), 'ok');
        assert.equal(await command('unload ui_theme'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started ui_theme/);
        console.log(`ok ui_theme quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'pack detached, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
