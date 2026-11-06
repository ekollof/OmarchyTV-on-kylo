/* A stage-sized autoload list must not be truncated. The old code stopped at
 * 16 names and truncated the config value at 192 bytes. The names are
 * intentionally absent: this checks the parser/count contract, not loading. */
const assert = require('assert/strict');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawn } = require('child_process');

async function main() {
    const build = path.resolve(process.argv[2] || 'build');
    const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-autoload-'));
    const config = path.join(dir, 'config');
    const socket = path.join(dir, 'control');
    const names = Array.from({ length: 40 }, (_, i) => `missing_${i}`);
    fs.writeFileSync(config, `autoload=${names.join(',')}\nui.fps=0\nui.ok_file=\n`);
    const child = spawn(path.join(build, 'openaurora'),
        ['--modules', path.join(build, 'modules'), '--config', config,
         '--sock', socket, '--log', '0'], { stdio: ['ignore', 'pipe', 'pipe'] });
    let out = '';
    child.stdout.on('data', d => { out += d; });
    child.stderr.on('data', () => {});
    const closed = new Promise(resolve => child.on('close', resolve));

    try {
        const deadline = Date.now() + 5000;
        while (!out.includes('module(s) loaded') && Date.now() < deadline)
            await new Promise(r => setTimeout(r, 20));
        assert.match(out, /openaurora: 40 module\(s\) loaded/,
            `startup output: ${out}`);
        console.log('ok autoload: stage-sized lists are not truncated');
    } finally {
        if (child.exitCode === null && !child.signalCode)
            child.kill('SIGTERM');
        try {
            await Promise.race([
                closed,
                new Promise((_, reject) =>
                    setTimeout(() => reject(Error('host did not exit')), 3000)),
            ]);
        } catch {
            child.kill('SIGKILL');
            await closed;
        }
        fs.rmSync(dir, { recursive: true, force: true });
    }
}

main().catch(e => { console.error(e); process.exitCode = 1; });
