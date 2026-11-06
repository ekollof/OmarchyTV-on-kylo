/* Batch loading over the real control socket: the reply is immediate, the
 * loop drains the queue without further input, and clients stay answered. */
const assert = require('assert/strict');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

async function main() {
    const build = path.resolve(process.argv[2] || 'build');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-batch-'));
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.writeFileSync(config, 'autoload=\nui.fps=0\nui.ok_file=\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['ignore', 'pipe', 'pipe'] });
    let stderr = '';
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-8192); });
    const closed = new Promise(resolve => child.on('close', resolve));

    function request(text, lines = 1) {
        return new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '', got = 0;
            const finish = (error, value) => {
                s.destroy();
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text));
            s.on('error', finish);
            s.on('end', () => finish(Error('EOF before reply: ' + data)));
            s.on('data', d => {
                data += d;
                got = (data.match(/\n/g) || []).length;
                if (got >= lines) finish(null, data);
            });
            s.setTimeout(5000, () => finish(Error('socket timeout: ' + data)));
        });
    }
    function requestUntil(text, marker) {
        return new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '';
            const finish = (error, value) => {
                s.destroy();
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text));
            s.on('error', finish);
            s.on('end', () => finish(Error('EOF before reply: ' + data)));
            s.on('data', d => {
                data += d;
                if (data.includes(marker)) finish(null, data);
            });
            s.setTimeout(5000, () => finish(Error('socket timeout: ' + data)));
        });
    }
    async function command(text) {
        const data = await request(text + '\n');
        return data.slice(0, data.indexOf('\n'));
    }
    async function waitFor(predicate, label, ms = 8000) {
        const deadline = Date.now() + ms;
        while (Date.now() < deadline) {
            if (await predicate()) return;
            await new Promise(r => setTimeout(r, 25));
        }
        throw Error('timeout: ' + label);
    }
    async function waitState(name, want) {
        await waitFor(async () => {
            const data = await requestUntil('status ' + name + '\n',
                'cleanup_pending');
            return data.includes('state   ' + want);
        }, name + ' ' + want);
    }

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        /* Both commands travel in one write and are handled together: the
         * batch only queues, so the last module cannot be loaded yet. */
        const pipelined = await request(
            'load-batch ticker ui_clock ui_anim\nstatus ui_anim\n', 2);
        const [queued, early] = pipelined.split('\n');
        assert.equal(queued, 'queued 3');
        assert.equal(early, 'no such module: ui_anim');

        /* No further commands: the loop must keep pumping the queue itself. */
        await waitState('ticker', 'loaded');
        await waitState('ui_clock', 'loaded');
        await waitState('ui_anim', 'loaded');

        const started = await request(
            'start-batch ticker ui_clock ui_anim\nstatus ui_anim\n', 2);
        assert.equal(started.split('\n')[0], 'queued 3');
        await waitState('ticker', 'started');
        await waitState('ui_clock', 'started');
        await waitState('ui_anim', 'started');

        /* Re-queueing a loaded/started module is idempotent, and a missing
         * entry does not stop later entries. */
        assert.equal(await command('load-batch ticker'), 'queued 1');
        await waitState('ticker', 'started');
        assert.equal(await command('load-batch no_such_module_xyz power'),
            'queued 2');
        await waitState('power', 'loaded');

        const help = await requestUntil('help\n', '  quit');
        assert.match(help, /load-batch <names\.\.\.>/);
        assert.match(help, /start-batch <names\.\.\.>/);
        assert.equal(await command('load-batch'), 'usage: load-batch <name> [name...]');

        child.kill('SIGTERM');
        const code = await Promise.race([
            closed,
            new Promise((_, reject) =>
                setTimeout(() => reject(Error('host did not exit')), 5000)),
        ]);
        assert.equal(code, 0, stderr);
        console.log('ok batch load: socket queue drains one module per loop turn');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGKILL');
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
