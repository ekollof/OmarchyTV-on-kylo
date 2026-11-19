# Staged end-to-end takeover bundle ready — 2026-11-19

Status: current (build/hash evidence only; **not run on the TV**)
Last verified: 2026-11-19
Evidence class: host build + hash verification; no TV contact, no ownership.

## Bundle

`openaurora/build-tv-v3/full-replacement/` — the vendor-skin parity stack
(`ui_vendor`, real `plat` with `plat.drm_init=0`), built for the TV with the
explicit `TARGET=tv BUILD=build-tv-v3` ARM toolchain command from
`openaurora/docs/ARCHITECTURE.md`.

- `sha256sum -c manifest.sha256 --quiet`: **66 entries verified**.
- 35 ARM EABI5 modules; core `openaurora` SHA256
  `f481e8faab75bf1b96672c473f0fe080915cd00db3ad7d9d28303f12b8ca989a`.
- Key modules: `mod_plat.so` `bc56d9c0…`, `mod_source.so` `8cdd6f5e…`,
  `mod_ui_vendor.so` `251ebb49…` (full hashes in `manifest.sha256`).
- Config `openaurora.conf` SHA256
  `a123dcb18364a127a0dfc790f51e276bb1f8cf2b7bf933fb819a702b28109590`;
  `takeover-full.sh` `e03464c1…`.
- Risky hardware gates are **off** in the staged config: `plat.drm_init=0`,
  `source.enable=0`, `cec.enable=0`, `audio_amp.enable=0`. The autoload list
  includes all 31 runtime modules; feature gates are enabled deliberately
  inside an authorized window, not in the bundle.
- `make TARGET=tv BUILD=build-tv-v3 check-staged-bundle` re-verifies hashes,
  module count, ARM ELF type, required helpers and gate values offline.

No staging was copied to the TV. `native-replacement-bundle` is a separate
profile (ui_omarchy skin); the owner's priority is platform/stock-UI parity,
so this vendor-skin stack is the correct first end-to-end candidate.

## Runbook for the owner-authorized window (not yet executed)

1. **Preflight (view-only):** record uptime, appmgr/Aurora PIDs and their
   `/proc/<pid>/fd` targets, fb1 var/pan, `/conf` flags, hw/sw profile MD5s.
   Re-verify the USB recovery media/script — the hashed recovery-stability pair
   is documented in `artifacts/README.md` and was last verified on 2026-11-09.
   `/conf/coredumpnoreset` was present at the last check.
2. **Stage:** copy the bundle under `/buffer` (owner-requested staging area);
   verify the manifest with `sha256sum -c` on the TV **in a separate successful
   connection** before executing anything (AGENTS.md rule). Never combine an
   stdin-streamed upload with asynchronous work.
3. **Takeover:** use `tools/boot/takeover-full.sh` with the external
   `oa-wdtfeed`; stop stock Aurora only for the window; never leave the
   watchdog disabled; no boot-hook or persistent install.
4. **Acceptance checks:** compare live fb1 frames against the read-only stock
   references using `openaurora/tools/capture_compare.py` and the region
   manifests — portal, channel list (`p0/04`), GotoServiceDialog
   (`goto-dialog-live.png`), filter popup (`channel-filter-popup.png`), edit
   pane and sources screen. Recorder candidates are **not** scanout evidence:
   only on-TV framebuffer reads count, and they still need independent visual
   confirmation for real panel appearance.
5. **Restore:** stop the core/helper, remove staging, `wdtctl set 10` reboot,
   then verify stock PIDs/fds/fb1/profiles and that no helper or disabled
   watchdog remains. Settings `m_volume`/`m_current_source`/`m_cec_enabled`
   must be restored before quit (hidden DSP/route state may still need a
   reboot — TAKEOVER-ACCEPT).

## Open

Owner authorization and a verified USB recovery path are required before
step 2. Record 151 (EPG/PVR options) has no stock capture (EIT/RF blocked), so
option-dialog parity cannot be compared in this window. Full platform gates
(cold boot, CEC bus ownership/TX, NIM RF, audible amp) remain separate.
