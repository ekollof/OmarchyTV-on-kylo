/* Offline only: source identity before executing the optional ARM reference. */
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const root = path.resolve(__dirname, '../../..');
const elf = path.join(root, 'artifacts/workbench/rootfs/vendor/com.nvt.idtv/aurora.elf');
const binary = path.join(root, 'openaurora/build-tv-v3/tab-policy/tests/tab_policy_firmware');
const libc = path.join(root, 'artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/arm-none-linux-gnueabihf/libc');
assert.equal(crypto.createHash('md5').update(fs.readFileSync(elf)).digest('hex'),
             '64c252340d3af6c5d66072611b7bad7a');
const result = spawnSync('qemu-arm-static', ['-L', libc, binary, elf],
                         { encoding: 'utf8', timeout: 30000 });
process.stdout.write(result.stdout || '');
process.stderr.write(result.stderr || '');
assert.ifError(result.error);
assert.equal(result.status, 0, `reference failed (77 means address unavailable), signal=${result.signal}`);
assert.match(result.stdout, /884736 comparisons/);
