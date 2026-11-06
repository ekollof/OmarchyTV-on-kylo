/* ABI 3 async quiesce for mod_ui_vendor: logical teardown (timers, service
 * holds, subscriptions, portal, skin registration) is synchronous; the font
 * pack and text caches are detached and freed on a worker. A diagnostic delay
 * makes the first reload/stop fail fast; a retry completes. Real font pack,
 * langprofile and menu; no display paint or TV. */
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
    const root = path.resolve(process.argv[3] || '.');
    const profile = path.join(root, 'artifacts/workbench/rootfs/vendor2/mb230_langprofile.bin');
    const font = path.join(root, 'artifacts/workbench/rootfs/vendor2/dynamic_ui/telefunken18_font_byte_data');
    const menu = path.join(root, 'openaurora/menus/sub_launcher.json');
    for (const f of [profile, `${font}.txt`, `${font}.bin`, menu])
        assert.ok(fs.existsSync(f), `missing ${f}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ui-vendor-quiesce-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    const copy = (from, to) => fs.copyFileSync(path.join(build, from),
        path.join(modules, to));
    copy('modules/mod_ui_engine.so', 'mod_ui_engine.so');
    copy('modules/mod_ui_vendor.so', 'mod_ui_vendor.so');
    copy('modules/mod_ui_anim.so', 'mod_ui_anim.so');
    copy('modules/mod_ui_menu.so', 'mod_ui_menu.so');
    copy('modules/mod_i18n.so', 'mod_i18n.so');
    copy('tests/mod_fake_clock.so', 'mod_fake_clock.so');
    copy('tests/ui_nav_display.so', 'mod_nav_display.so');
    fs.writeFileSync(config,
        `autoload=\nui.screen=sub_launcher\nui.paint=0\nui.text=1\n` +
        `ui.font_index=${font}.txt\nui.font_pack=${font}.bin\n` +
        `i18n.profile=${profile}\ni18n.language=dut\n` +
        `ui_menu.file=${menu}\nui_vendor.cleanup_delay_ms=400\n`);

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
        const started = Date.now();
        const reply = await bounded(new Promise((resolve, reject) => {
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
        for (const m of ['nav_display', 'ui_engine', 'fake_clock', 'ui_anim',
                         'ui_menu', 'i18n', 'ui_vendor']) {
            assert.equal((await command(`load ${m}`)).reply, `ok ${m}`,
                `load ${m}`);
            assert.equal((await command(`start ${m}`)).reply, 'ok', `start ${m}`);
        }
        assert.equal((await command('ui-state')).reply, 'portal screen 7');

        const first = await command('reload ui_vendor');
        assert.equal(first.reply, 'failed', 'reload must fail fast while cleanup runs');
        assert.ok(first.elapsed < 1500, `first reload took ${first.elapsed}ms`);
        const status = await command('status ui_vendor');
        assert.ok(status.elapsed < 1000, `status took ${status.elapsed}ms`);

        await new Promise(resolve => setTimeout(resolve, 600));
        const retry = await command('reload ui_vendor');
        if (retry.reply !== 'ok')
            console.error('retry reply:', retry.reply, '\nSTDOUT:\n', stdout,
                          '\nSTDERR:\n', stderr);
        assert.equal(retry.reply, 'ok', 'retry must complete the reload');
        assert.equal((await command('ui-state')).reply, 'portal screen 7',
            'portal must republish before reporting ready');

        const stop = await command('stop ui_vendor');
        assert.equal(stop.reply, 'failed', 'stop starts the same async gate');
        await new Promise(resolve => setTimeout(resolve, 600));
        assert.equal((await command('stop ui_vendor')).reply, 'ok');
        assert.equal((await command('unload ui_vendor')).reply, 'ok');
        assert.equal((await command('quit')).reply, 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        assert.match(stdout + stderr, /live settings text: fonts 7\+16/);
        console.log('ok ui_vendor quiesce: reload fails fast, portal republishes, retry completes');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
