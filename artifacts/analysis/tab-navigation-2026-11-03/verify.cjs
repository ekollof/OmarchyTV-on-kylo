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
const profile = path.join(root, 'artifacts/profiles/mb230_swprofile.bin');
assert.equal(crypto.createHash('md5').update(fs.readFileSync(elf)).digest('hex'),
             '64c252340d3af6c5d66072611b7bad7a');
assert.equal(crypto.createHash('md5').update(fs.readFileSync(profile)).digest('hex'),
             'a1d9e5e80a926a6e0abd812c98852894');
const result = spawnSync('qemu-arm-static', ['-L', libc, binary, elf, profile],
                         { encoding: 'utf8', timeout: 30000 });
process.stdout.write(result.stdout || '');
process.stderr.write(result.stderr || '');
assert.ifError(result.error);
assert.equal(result.status, 0, `reference failed (77 means address unavailable), signal=${result.signal}`);
assert.match(result.stdout, /884736 comparisons/);
assert.match(result.stdout, /group selection: 9216 availability\/description\/request cases/);
assert.match(result.stdout, /profile: 65281 signed-byte hashes, 27 saved-profile lookups\/defaults; connectivity=1 hotel=1/);
assert.match(result.stdout, /retained setters: 1792 signed32\/raw byte cases/);
assert.match(result.stdout, /hotel row: 345 integer store\/no-row cases/);
assert.match(result.stdout, /tab events: 1008 handler cases/);
assert.match(result.stdout, /pre-handler: 65664 valid-tree cases/);
assert.match(result.stdout, /tab dispatch: 65600 type\/id\/result routes, 16384 pre\/fallback joins/);
assert.match(result.stdout, /tab leaves: 238860 cases, six bodies and real signed division/);
assert.match(result.stdout, /settings map: 65600 type\/id\/result cases/);
assert.match(result.stdout, /settings input: 86400 cases, five bodies/);
