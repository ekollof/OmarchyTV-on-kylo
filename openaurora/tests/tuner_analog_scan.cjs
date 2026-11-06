/* Analog scan stepping over the offline stub backend: one frequency per
 * dwell window, progress from the configured range, locked channels counted
 * once, and cancel still works. Real MxL661 tuning is hardware-gated. */
const assert = require('assert/strict');
const fs = require('fs');
const net = require('net');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

function parse(reply) {
    const m = reply.match(/^network (\d+) state (\d+) progress (\d+) lock (\d+) services (\d+) selected (\d+) error (-?\d+) strength (\d+) quality (\d+) ber ([\d.]+) snr ([\d.]+) unc (\d+)$/);
    assert.ok(m, `unexpected tuner-state: ${reply}`);
    return {
        network: +m[1], state: +m[2], progress: +m[3], locked: +m[4],
        services: +m[5], selected: +m[6], error: +m[7],
        strength: +m[8], quality: +m[9], ber: +m[10], snr: +m[11],
        unc: +m[12],
    };
}

async function main() {
    const build = path.resolve(process.argv[2] || 'build');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-atv-scan-'));
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    fs.writeFileSync(config,
        'autoload=tuner\n' +
        'tuner.analog_scan=1\n' +
        'tuner.analog_start_khz=100000\n' +
        'tuner.analog_stop_khz=104000\n' +
        'tuner.analog_step_khz=1000\n' +
        'tuner.analog_dwell=2\n' +
        'ui.fps=0\nui.ok_file=\n');
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['ignore', 'pipe', 'pipe'] });
    let stderr = '';
    child.stderr.on('data', d => { stderr = (stderr + d).slice(-8192); });
    const closed = new Promise(resolve => child.on('close', resolve));

    async function command(text) {
        return new Promise((resolve, reject) => {
            const s = net.createConnection(socket);
            let data = '', done = false;
            const finish = (error, value) => {
                if (done) return;
                done = true; s.destroy();
                error ? reject(error) : resolve(value);
            };
            s.on('connect', () => s.write(text + '\n'));
            s.on('error', finish);
            s.on('end', () => finish(Error('EOF before reply')));
            s.on('data', d => {
                data += d;
                if (data.includes('\n')) finish(null, data.slice(0, data.indexOf('\n')));
            });
            s.setTimeout(3000, () => finish(Error('socket timeout')));
        });
    }

    try {
        const deadline = Date.now() + 4000;
        while (!fs.existsSync(socket) && Date.now() < deadline)
            await new Promise(r => setTimeout(r, 20));
        assert.ok(fs.existsSync(socket), 'control socket');

        assert.equal(await command('tuner-scan 4'), 'ok');
        let st = parse(await command('tuner-state'));
        assert.equal(st.network, 4);
        assert.equal(st.state, 1);

        let seen = [st];
        for (let i = 0; i < 40 && st.state !== 2; i++) {
            st = parse(await command('tuner-state'));
            seen.push(st);
        }
        assert.equal(st.state, 2, `scan did not finish: ${JSON.stringify(seen)}`);
        assert.equal(st.progress, 100);
        assert.equal(st.locked, 1);
        assert.equal(st.services, 5, 'five 1 MHz steps over 100-104 MHz');
        assert.ok(seen.some(x => x.progress === 25));
        assert.ok(seen.some(x => x.progress === 50));
        assert.ok(seen.some(x => x.progress === 75));

        assert.equal(await command('tuner-scan 4'), 'ok');
        parse(await command('tuner-state'));
        assert.equal(await command('tuner-cancel'), 'ok');
        st = parse(await command('tuner-state'));
        assert.equal(st.state, 3);

        assert.equal(await command('quit'), 'bye');
        assert.equal(await closed, 0, stderr);
        console.log('ok tuner analog scan: 5 stepped channels, progress 25/50/75/100, cancel');
    } finally {
        if (child.exitCode === null && !child.signalCode) child.kill('SIGKILL');
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
