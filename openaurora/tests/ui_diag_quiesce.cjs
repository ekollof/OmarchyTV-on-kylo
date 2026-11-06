/* ABI 3 async quiesce for mod_ui_diag: drop the engine/skin registration and
 * detach font buffers synchronously, then free them from a sched slice. The
 * engine skin query must stop naming diag while cleanup is pending. */
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
    const nav = path.join(build, 'tests', 'ui_nav_display.so');
    const engine = path.join(build, 'modules', 'mod_ui_engine.so');
    const diag = path.join(build, 'modules', 'mod_ui_diag.so');
    const fonts = path.join(root, 'artifacts/workbench/rootfs/vendor2/dynamic_ui');
    const fontIndex = path.join(fonts, 'telefunken18_font_byte_data.txt');
    const fontPack = path.join(fonts, 'telefunken18_font_byte_data.bin');
    for (const f of [nav, engine, diag, fontIndex, fontPack])
        assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ui-diag-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(nav, path.join(modules, 'mod_nav_display.so'));
    fs.copyFileSync(engine, path.join(modules, 'mod_ui_engine.so'));
    fs.copyFileSync(diag, path.join(modules, 'mod_ui_diag.so'));
    fs.writeFileSync(config,
        'autoload=\nui.skin=diag\nui.paint=0\nui.fps=0\nui.ok_file=\n' +
        `ui.font_index=${fontIndex}\nui.font_pack=${fontPack}\n` +
        'ui.diag.font=7\nui.diag.title_font=18\n' +
        'ui_diag.cleanup_delay_ms=400\n');

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
        assert.equal(await command('load nav_display'), 'ok nav_display');
        assert.equal(await command('start nav_display'), 'ok');
        assert.equal(await command('load ui_engine'), 'ok ui_engine');
        assert.equal(await command('start ui_engine'), 'ok');
        assert.equal(await command('load ui_diag'), 'ok ui_diag');
        assert.equal(await command('start ui_diag'), 'ok');
        assert.match(await command('skin'), /active diag/);

        const reload = await timed('reload ui_diag');
        assert.equal(reload.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(reload.elapsed < 1500, `first reload took ${reload.elapsed}ms`);
        const status = await timed('status ui_diag');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);
        assert.match(await command('skin'), /active -/,
            'engine must drop the diag skin while quiesce is pending');

        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('reload ui_diag'), 'ok',
            'retry must complete the reload');
        assert.match(await command('skin'), /active diag/,
            'the replacement must re-register the diag skin');

        const stop = await timed('stop ui_diag');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        assert.match(await command('skin'), /active -/);
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal(await command('stop ui_diag'), 'ok');
        assert.equal(await command('unload ui_diag'), 'ok');
        assert.equal(await command('stop ui_engine'), 'ok');
        assert.equal(await command('unload ui_engine'), 'ok');
        assert.equal(await command('stop nav_display'), 'ok');
        assert.equal(await command('unload nav_display'), 'ok');
        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stderr, /started ui_diag/);
        console.log(`ok ui_diag quiesce: reload fails fast (${reload.elapsed}ms), ` +
                    'skin detached, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
