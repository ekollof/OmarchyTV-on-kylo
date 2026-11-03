/* Host preflight of exactly the source module inventory, excluding older
 * diagnostic DSOs left in build/modules. No module start or TV connection. */
const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '../../..');
const makefile = fs.readFileSync(path.join(root, 'openaurora/Makefile'), 'utf8');
const modules = makefile.match(/^MODULES\s*:=\s*(.+)$/m)[1].trim().split(/\s+/);
assert.equal(modules.length, 23);
assert.equal(new Set(modules).size, modules.length);
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'oa-profile-loader-'));
try {
    for (const name of modules)
        fs.symlinkSync(path.join(root, 'openaurora/build/modules', `mod_${name}.so`),
                       path.join(dir, `mod_${name}.so`));
    const result = spawnSync(path.join(root, 'openaurora/build/openaurora'),
                             ['--modules', dir, '--check'], { encoding: 'utf8', timeout: 30000 });
    process.stdout.write(result.stdout || '');
    process.stderr.write(result.stderr || '');
    assert.ifError(result.error);
    assert.equal(result.status, 0);
    assert.equal((result.stdout.match(/^OK .*\/mod_.*\.so$/gm) || []).length, 23);
} finally {
    fs.rmSync(dir, { recursive: true, force: true });
}
