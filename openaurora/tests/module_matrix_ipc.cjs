/* Real Unix-socket lifecycle for the remaining hardware-free modules:
 * ticker -> ui_clock -> ui_anim, plus ui_navigation, ui_list, power and cec.
 * No display/HAL/device module is loaded. */
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
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-matrix-ipc-'));
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config, 'autoload=\nui.fps=0\nui.ok_file=\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['pipe', 'pipe', 'pipe'] });
    let stdout = '', stderr = '';
    child.stdout.on('data', d => { stdout = (stdout + d).slice(-16384); });
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
    async function waitFor(predicate, label) {
        await bounded(new Promise((resolve, reject) => {
            const timer = setInterval(() => {
                if (predicate()) { clearInterval(timer); resolve(); }
            }, 20);
            setTimeout(() => { clearInterval(timer); reject(Error('timeout: ' + label)); }, 4000);
        }), 5000);
    }
    async function retryCommand(text, expected = 'ok', tries = 20) {
        let last = '';
        for (let i = 0; i < tries; i++) {
            last = await command(text);
            if (last === expected) return last;
            await new Promise(r => setTimeout(r, 100));
        }
        return last;
    }
    async function statusStarted(name) {
        stdout = '';
        child.stdin.write(`status ${name}\n`);
        await waitFor(() => stdout.includes('name    ' + name), name + ' status');
        await waitFor(() => stdout.includes('state   started'), name + ' started');
    }
    const modules = ['ticker', 'ui_clock', 'ui_anim', 'ui_navigation',
        'ui_list', 'power', 'cec'];

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        for (const name of modules) {
            assert.equal(await command('load ' + name), 'ok ' + name);
        }
        for (const name of ['ticker', 'ui_clock', 'ui_anim', 'ui_navigation',
            'ui_list', 'power', 'cec']) {
            assert.equal(await command('start ' + name), 'ok');
        }
        for (const name of modules) await statusStarted(name);

        assert.equal(await command('nav-state'), 'tab 0 item 0 submenu 0 pending 0');
        assert.equal(await command('list-state'),
            'first 0 size 5 selected 0 total 9 handled -1 anim 0 scroll 0 partial 0 focus 0 group 0');

        assert.equal(await command('reload ui_navigation'), 'ok');
        assert.equal(await command('nav-state'), 'tab 0 item 0 submenu 0 pending 0');
        assert.equal(await command('reload ui_list'), 'ok');
        assert.equal(await command('list-state'),
            'first 0 size 5 selected 0 total 9 handled -1 anim 0 scroll 0 partial 0 focus 0 group 0');
        assert.equal(await command('reload ui_anim'), 'ok');

        /* Dependency-ordered clock reloads. */
        assert.equal(await command('stop ui_anim'), 'ok');
        assert.equal(await command('reload ui_clock'), 'ok');
        assert.equal(await command('start ui_anim'), 'ok');
        assert.equal(await command('stop ui_anim'), 'ok');
        assert.equal(await command('stop ui_clock'), 'ok');
        assert.equal(await command('reload ticker'), 'ok');
        assert.equal(await command('start ui_clock'), 'ok');
        assert.equal(await command('start ui_anim'), 'ok');
        for (const name of ['ticker', 'ui_clock', 'ui_anim']) await statusStarted(name);

        assert.equal(await command('reload power'), 'ok');
        assert.equal(await retryCommand('reload cec'), 'ok');
        assert.equal(await retryCommand('stop cec'), 'ok');
        assert.equal(await retryCommand('unload cec'), 'ok');
        assert.equal(await command('load cec'), 'ok cec');
        assert.equal(await command('start cec'), 'ok');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 5000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual hardware-free module matrix IPC: clock chain, navigation/list, power/cec reloads');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 5000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
