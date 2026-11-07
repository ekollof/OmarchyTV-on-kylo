/* IR BLAST helper lifecycle: fake blocking libNDAL_EX, real oa-ir-blast and
 * mod_ir.
 *
 * Proves that BLAST_RX runs in the helper process (status replies while the
 * fake blocks), fixed records reach the module's loop (debug log), and quit
 * kills/reaps the helper without hanging.
 */
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
    const ir = path.join(build, 'modules', 'mod_ir.so');
    const plat = path.join(build, 'tests', 'mod_plat_stub.so');
    const fake = path.join(build, 'tests', 'fake_blast_lib.so');
    const helper = path.join(build, 'oa-ir-blast');
    for (const f of [ir, plat, fake, helper])
        assert.ok(fs.existsSync(f), `missing ${f}`);

    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-ir-blast-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    fs.copyFileSync(ir, path.join(modules, 'mod_ir.so'));
    fs.copyFileSync(plat, path.join(modules, 'mod_plat.so'));
    fs.copyFileSync(fake, path.join(dir, 'libNDAL_EX.so'));
    fs.writeFileSync(config,
        'autoload=\n' +
        'ir.blast=1\n' +
        `ir.blast_helper=${helper}\n` +
        'ir.blast_video=\n' +
        'ir.device=\n' +
        'ir.rc_protocol=4\n' +
        'ir.cleanup_delay_ms=0\n');

    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', modules, '--config', config, '--sock', socket,
         '--log', '3'],
        { stdio: ['ignore', 'pipe', 'pipe'],
          env: { ...process.env, LD_LIBRARY_PATH: dir +
                 (process.env.LD_LIBRARY_PATH ? ':' + process.env.LD_LIBRARY_PATH : '') } });
    let output = '';
    child.stdout.on('data', d => { output = (output + d).slice(-65536); });
    child.stderr.on('data', d => { output = (output + d).slice(-65536); });
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
        assert.equal(await command('load plat'), 'ok plat');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('load ir'), 'ok ir');
        assert.equal(await command('start ir'), 'ok');

        /* The fake BLAST_RX blocks for 150 ms. The control loop must still
         * answer while the worker is inside it. */
        const started = Date.now();
        const status = await command('status ir');
        const elapsed = Date.now() - started;
        assert.match(status, /^name\s+ir/m);
        assert.ok(elapsed < 1000, `status ir took ${elapsed}ms`);

        await new Promise(resolve => setTimeout(resolve, 700));
        assert.match(output, /BLAST helper pid/);
        assert.match(output, /BLAST code=00003056/);
        assert.match(output, /rc=22 Right/);

        const again = Date.now();
        assert.match(await command('status ir'), /^name\s+ir/);
        assert.ok(Date.now() - again < 1000, 'second status ir must not block');

        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 5000);
        assert.equal(exit.code, 0, output);
        console.log('ok ir BLAST worker: loop responsive, keys posted, clean quit');
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
