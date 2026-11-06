/* plat/power have no safely deferable cleanup: prove their real modules stop
 * and reload synchronously, unpublish their service, and republish on start.
 * Host build only; plat uses the OA_NO_VENDOR stubs, power is disabled. */
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
    const core = path.join(build, 'openaurora');
    const modulesSrc = path.join(build, 'modules');
    for (const name of ['mod_plat.so', 'mod_power.so'])
        assert.ok(fs.existsSync(path.join(modulesSrc, name)), `missing ${name}`);
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-plat-power-stop-'));
    const modules = path.join(dir, 'modules');
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.mkdirSync(modules);
    for (const name of ['mod_plat.so', 'mod_power.so'])
        fs.copyFileSync(path.join(modulesSrc, name), path.join(modules, name));
    fs.writeFileSync(config, 'autoload=\nplat.enable=1\npower.enable=0\n');

    const child = spawn(core,
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

    async function command_all(text) {
        return bounded(new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '', done = false, idle = null;
            const finish = (error, value) => {
                if (done) return;
                done = true; s.destroy();
                if (idle) clearTimeout(idle);
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text + '\n'));
            s.on('error', e => finish(e));
            s.on('end', () => finish(null, data));
            s.on('data', d => {
                data += d;
                if (idle) clearTimeout(idle);
                idle = setTimeout(() => finish(null, data), 150);
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        }), 4000);
    }

    async function timed(text) {
        const started = Date.now();
        const reply = await command(text);
        return { reply, elapsed: Date.now() - started };
    }

    async function started_with(name, service) {
        const out = await command_all(`status ${name}`);
        assert.match(out, /state\s+started/, `${name}: ${out}`);
        assert.ok(out.includes(service), `${name} missing ${service}: ${out}`);
    }

    async function stopped_without(name, service) {
        const out = await command_all(`status ${name}`);
        assert.match(out, /state\s+loaded/, `${name}: ${out}`);
        assert.ok(!out.includes(service), `${name} still offers ${service}: ${out}`);
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
        await started_with('plat', 'plat:v1');
        const platReload = await timed('reload plat');
        assert.equal(platReload.reply, 'ok');
        assert.ok(platReload.elapsed < 1000, `plat reload took ${platReload.elapsed}ms`);
        await started_with('plat', 'plat:v1');
        const platStop = await timed('stop plat');
        assert.equal(platStop.reply, 'ok');
        assert.ok(platStop.elapsed < 1000, `plat stop took ${platStop.elapsed}ms`);
        await stopped_without('plat', 'plat:v1');
        assert.equal(await command('start plat'), 'ok');
        assert.equal(await command('unload plat'), 'ok');

        assert.equal(await command('load power'), 'ok power');
        assert.equal(await command('start power'), 'ok');
        await started_with('power', 'power:v1');
        const powerReload = await timed('reload power');
        assert.equal(powerReload.reply, 'ok');
        assert.ok(powerReload.elapsed < 1000, `power reload took ${powerReload.elapsed}ms`);
        assert.match(stderr, /power: restored 4 bytes of state/);
        await started_with('power', 'power:v1');
        const powerStop = await timed('stop power');
        assert.equal(powerStop.reply, 'ok');
        assert.ok(powerStop.elapsed < 1000, `power stop took ${powerStop.elapsed}ms`);
        await stopped_without('power', 'power:v1');
        assert.equal(await command('start power'), 'ok');
        assert.equal(await command('unload power'), 'ok');

        assert.equal(await command('quit'), 'bye');
        const exit = await bounded(closed, 8000);
        assert.equal(exit.code, 0, stderr);
        console.log(`ok plat/power sync stop: plat reload ${platReload.elapsed}ms stop ${platStop.elapsed}ms, ` +
                    `power reload ${powerReload.elapsed}ms stop ${powerStop.elapsed}ms, services unpublished`);
    } finally {
        cleanup();
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
