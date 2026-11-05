/* The control client must not wedge on a host that accepts but never replies.
 * Also checks the fire-and-forget mode used for a slow reload. */
const assert = require('assert/strict');
const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');

async function once(emitter, event) {
    return new Promise((resolve, reject) => {
        emitter.once(event, resolve);
        emitter.once('error', reject);
    });
}

async function main() {
    const client = path.resolve(process.argv[2] || 'build/oa-client');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-client-'));
    const sock = path.join(dir, 'control.sock');
    const sockets = new Set();
    const server = net.createServer(s => {
        sockets.add(s);
        s.on('close', () => sockets.delete(s));
        s.on('error', () => {});
    });
    server.listen(sock);
    await once(server, 'listening');
    try {
        const started = Date.now();
        const bounded = spawn(client, ['--timeout', '1', sock, 'status'], {
            stdio: ['ignore', 'pipe', 'pipe'],
        });
        let stderr = '';
        bounded.stderr.on('data', d => { stderr += d; });
        const code = await new Promise(resolve => bounded.on('close', resolve));
        const elapsed = Date.now() - started;
        assert.equal(code, 1, 'bounded client must fail on timeout');
        assert.match(stderr, /timeout after 1s waiting for a reply/);
        assert.ok(elapsed < 3000, `bounded client took ${elapsed}ms`);

        const immediate = Date.now();
        const fire = spawn(client, ['--no-wait', sock, 'reload', 'ir'], {
            stdio: ['ignore', 'pipe', 'pipe'],
        });
        const fireCode = await new Promise(resolve => fire.on('close', resolve));
        const fireElapsed = Date.now() - immediate;
        assert.equal(fireCode, 0, 'no-wait client must not wait');
        assert.ok(fireElapsed < 1000, `no-wait client took ${fireElapsed}ms`);
        console.log('ok oa-client: bounded timeout and no-wait reload');
    } finally {
        for (const s of sockets) s.destroy();
        server.close();
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
