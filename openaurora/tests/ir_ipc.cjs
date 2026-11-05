/* Real Unix-socket lifecycle for mod_ir with the recorder display, ui_engine
 * and ui_vendor. The UART stays closed (ir.device=) and BLAST is disabled
 * (ir.blast=0), so key frames come through the control `key` command and the
 * shipped parser. Covers stop/start/reload/unload and a ui_vendor reload while
 * input is live. No TV/HAL device. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ir-ipc-'));
    const modules = path.join(dir, 'modules'), ui = path.join(dir, 'ui');
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.mkdirSync(modules); fs.mkdirSync(ui);
    fs.copyFileSync(path.join(build, 'tests/ui_nav_display.so'),
        path.join(modules, 'mod_nav_display.so'));
    for (const name of ['plat', 'ir', 'ui_engine', 'ui_vendor']) {
        fs.copyFileSync(path.join(build, 'modules', `mod_${name}.so`),
            path.join(modules, `mod_${name}.so`));
    }
    fs.writeFileSync(config,
        `autoload=\nui.dir=${ui}\nui.fps=0\nui.ok_file=\n` +
        `ui.live_cards=0\nui.paint=1\nir.device=\nir.blast=0\n`);
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
    async function waitFor(predicate, label) {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (predicate()) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('timeout: ' + label)); }, 4000);
        }), 5000);
    }
    async function stdinCommand(text, expected) {
        stdout = '';
        child.stdin.write(text + '\n');
        await waitFor(() => stdout.includes(expected), expected);
    }
    async function keyWorks(code) {
        stdout = ''; stderr = '';
        assert.equal(await command('key ' + code), 'ok');
        const logs = () => stdout + stderr;
        await waitFor(() => logs().includes('painted portal') ||
            logs().includes('key rc=' + code), 'key ' + code);
    }

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load nav_display'), 'ok nav_display');
        assert.equal(await command('start nav_display'), 'ok');
        assert.equal(await command('load ir'), 'ok ir');
        assert.equal(await command('start ir'), 'ok');
        assert.equal(await command('load ui_engine'), 'ok ui_engine');
        assert.equal(await command('start ui_engine'), 'ok');
        assert.equal(await command('load ui_vendor'), 'ok ui_vendor');
        assert.equal(await command('start ui_vendor'), 'ok');

        await stdinCommand('status ir', 'state   started');
        assert.equal(stdout.includes('input:v1'), true);
        await keyWorks(22);

        assert.equal(await command('stop ir'), 'ok');
        assert.equal(await command('key 22'), 'failed errno ' + os.constants.errno.ENODEV);
        assert.equal(await command('start ir'), 'ok');
        await stdinCommand('status ir', 'state   started');
        await keyWorks(22);

        assert.equal(await command('reload ir'), 'ok');
        await stdinCommand('status ir', 'state   started');
        assert.equal(stdout.includes('input:v1'), true);
        await keyWorks(22);

        assert.equal(await command('reload ui_vendor'), 'ok');
        assert.equal(await command('ui-state'), 'portal screen 0');
        await keyWorks(21);

        assert.equal(await command('stop ui_vendor'), 'ok');
        assert.equal(await command('start ui_vendor'), 'ok');
        await keyWorks(22);

        assert.equal(await command('stop ir'), 'ok');
        assert.equal(await command('unload ir'), 'ok');
        assert.equal(await command('load ir'), 'ok ir');
        assert.equal(await command('start ir'), 'ok');
        await stdinCommand('status ir', 'state   started');
        await keyWorks(22);

        child.kill('SIGTERM');
        const exit = await bounded(closed, 5000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual ir control-socket lifecycle: start/key/stop/reload/unload and ui_vendor reload');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 5000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
