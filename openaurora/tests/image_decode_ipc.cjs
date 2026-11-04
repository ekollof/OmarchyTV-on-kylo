/* Real Unix-socket and stdin control-plane lifecycle for image.decode.
 * Starts the host with no autoloaded modules, then loads/starts/uses/stops/
 * reloads/unloads the provider and verifies the decoded bytes through the CLI.
 * No display, HAL or TV module is loaded. */
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
    const root = path.resolve(__dirname, '..', '..');
    const png = path.join(root, 'artifacts/workbench/thumbs/launcher_app_thumb61');
    const jpeg = path.join(root, 'artifacts/dumps/apdat_files/dlna-dmr/xml/icon/DMR-small.jpg');
    if (!fs.existsSync(png) || !fs.existsSync(jpeg)) {
        console.error('skipping: image corpus unavailable');
        process.exit(77);
    }
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-image-ipc-'));
    const config = path.join(dir, 'config'), socket = path.join(dir, 'control');
    fs.writeFileSync(config, 'autoload=\n');
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
    async function stdinCommand(text, expected) {
        stdout = '';
        child.stdin.write(text + '\n');
        await waitFor(() => stdout.includes(expected), expected);
    }

    try {
        await waitFor(() => fs.existsSync(socket), 'socket');
        assert.equal(await command('image-decode ' + png), 'failed errno ' + os.constants.errno.ENODEV);
        assert.equal(await command('load image_decode'), 'ok image_decode');
        assert.equal(await command('start image_decode'), 'ok');
        assert.equal(await command('image-decode ' + png), 'image 400x225 fnv 12815d59be310b91');
        assert.equal(await command('image-decode ' + jpeg), 'image 48x48 fnv 954e7afda49f66c6');
        assert.equal(await command('image-decode ' + config), 'failed errno ' + os.constants.errno.EINVAL);
        assert.equal(await command('image-decode'), 'failed errno ' + os.constants.errno.EINVAL);
        assert.equal(await command('image-decode ' + png + ' extra'), 'failed errno ' + os.constants.errno.EINVAL);

        await stdinCommand('status image_decode', 'name    image_decode');
        await waitFor(() => stdout.includes('state   started'), 'started status');
        await stdinCommand('image-decode ' + png, 'image 400x225 fnv 12815d59be310b91');

        assert.equal(await command('suspend'), 'ok');
        assert.equal(await command('image-decode ' + png), 'failed errno ' + os.constants.errno.EAGAIN);
        assert.equal(await command('resume'), 'ok');
        assert.equal(await command('image-decode ' + png), 'image 400x225 fnv 12815d59be310b91');

        assert.equal(await command('stop image_decode'), 'ok');
        assert.equal(await command('image-decode ' + png), 'failed errno ' + os.constants.errno.ENODEV);
        assert.equal(await command('start image_decode'), 'ok');
        assert.equal(await command('image-decode ' + png), 'image 400x225 fnv 12815d59be310b91');

        assert.equal(await command('reload image_decode'), 'ok');
        assert.equal(await command('image-decode ' + jpeg), 'image 48x48 fnv 954e7afda49f66c6');

        assert.equal(await command('stop image_decode'), 'ok');
        assert.equal(await command('unload image_decode'), 'ok');
        assert.equal(await command('image-decode ' + png), 'failed errno ' + os.constants.errno.ENODEV);
        assert.equal(await command('load image_decode'), 'ok image_decode');
        assert.equal(await command('start image_decode'), 'ok');
        assert.equal(await command('image-decode ' + png), 'image 400x225 fnv 12815d59be310b91');

        child.kill('SIGTERM');
        const exit = await bounded(closed, 5000);
        assert.equal(exit.code, 0, stderr);
        assert(!fs.existsSync(socket), 'socket not removed');
        console.log('ok actual image.decode Unix IPC: load/start/CLI use/stop/start/reload/unload/reload lifecycle');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGTERM');
        try { await bounded(closed, 5000); } catch { child.kill('SIGKILL'); await closed; }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}
main().catch(e => { console.error(e); process.exitCode = 1; });
