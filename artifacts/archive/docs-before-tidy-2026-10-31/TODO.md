# TODO — tvhack (TELEFUNKEN D65U546A / Vestel MB230)

Latest resumption guide: **HANDOFF.md** (2026-10-31).

State audited on 2026-10-31; see `artifacts/reports/TODO-AUDIT-2026-10-31.md`. TV is **rooted over ssh** and the hack lives entirely in
`apdat0` (`/conf`); the signed volumes are untouched.

**CURRENT: recovered and verified, 2026-10-31 20:02.** Owner successfully restored
the prepared image using the exact older `apply_hook.scr.txt` as `kylo_usb_update.scr`
(584 bytes, MD5 `de1591298d633cf6422a1ade4e8835a9`). Fresh SSH key login and root
telnet both work. Ethernet is `.87` / `70:54:b4:fe:70:c6`; routes, hosts block,
Samba OUTPUT REJECT and privacy settings are correct. One keepalive loop runs.
Stock UI is pinned with `/conf/oa_safe`; native/web boot disabled. No further TV
deployment or reboot is needed. Stability/respawn tests passed before recovery,
and all access/protection checks passed again after this physical recovery boot.
Recovery image MD5: `994da7ff76a0b2fbafb8f6fa0c682627`; raw mtd13 image remains older.
The upload failure and unsuccessful initial recovery attempts are documented in MEMORY.

**Owner constraint:** the TV is in use. Remote access and read-only diagnostics are
allowed. Do not interrupt operation: no deployment, service restarts, network changes,
remote keys, suspend, WoL or reboots until the owner authorizes changes again.

**Latest diagnostic authorization:** the owner explicitly requested real-TV
`--check` testing and allowed its CPU/memory/disk cost. The rebuilt core/modules
were staged under `/tmp`, separately hash-verified, checked successfully and
removed. Persistent install/boot flags/UI were unchanged. Owner prefers
`tools/tvssh.sh` for TV access, including its WoL handling when necessary.

**Openaurora scope:** vendor behaviour and quirks remain the target, with an
explicit owner-requested exception: **no telemetry or advertising**, including
ACR, usage/app-click reporting, tracking/profiling, advertising IDs, sponsored
placements or ad delivery. Do not port these as optional modules. This does not
descope other TV features or local diagnostics.

`[P]` = needs physical access to the TV.

**Earlier continuation, 2026-10-31 11:36 (before the error above):** vendor UI still running; no reboot or hardware
takeover this session. Independent `oa-supervise` recovery is implemented, host-tested
and ARM smoke-tested on the TV with a fake display and stub watchdog/fallback.
Core, supervisor, ui_fb, config and the opt-in hook are deployed with
`.pre-recovery-20261031` backups. `/conf/oa_boot` remains absent; web launcher is disabled.
See `openaurora/docs/RECOVERY.md`. The UBIFS/raw recovery images have **not** been rebuilt.

Offline DRM work recovered the 12-word record and corrected another init assumption:
`_hal_video_drm_kms_init` discovers IDs; it does not call SETPLANE. See
`artifacts/reports/DRM-PLANE-CONTRACT-2026-10-31.md`. Next: follow the vendor OSD
enable/buffer-binding path before another scanout experiment.

**Audit:** `artifacts/reports/TODO-AUDIT-2026-10-31.md` inventories the 72 checkmarks
present at audit start. Fourteen were reopened; research/experiment milestones were
narrowed to their actual evidence. Three reproduced registry bugs were fixed and
regression-tested on host and ARM/qemu. Final staged core MD5:
`a898384df076572cf377749f25427689` (earlier ABI 1 build). Queued-post ownership and
descriptor-size validation are now implemented and tested offline in ABI 2; see
`artifacts/reports/MODULE-SAFETY-2026-10-31.md`. Nothing was deployed to the TV.

---

## P0 — online (done 2026-10-30)

- [x] Network restored; consent flags `internet=1`, `advertising=0`, `personalized=0`,
      `marketing=0`, `3p_share=0`, `acr=0`, `logging=0`, DNT=1 (read back 2026-10-31).
- [x] `/etc/hosts` block active; verified no plog/orion/search traffic over a 90 s sample.

## P0a — WoL / standby (tested 2026-10-30)

- [x] Wake-on-LAN **works**: magic packet to `70:54:b4:fe:70:c6` resumed the TV from quick
      standby (uptime preserved, sshd/app back instantly). Tool: `tools/wol.py`.
- [x] `m_wake_on_settings = 1`; NIC driver keeps PHY power in STR_OFF suspend.
- [ ] Determine which standby mode (quick vs deep) powers the PHY down — test deep standby.
- [x] Quick-standby DIAL drops the connection and ports close (historical DIAL standby test);
      use WoL. Deep standby remains untested above.
- [x] Correction: the privacy "Internet connection" flag **does** gate the network — owner
      re-enabled all privacy settings in the UI and connectivity returned. Keep
      `m_privacy_internet_connection_en = 1` when re-applying ad/marketing/3p = 0.

## P0b — self-heal + on-device tooling (done 2026-10-30)

- [x] Static ARM `sqlite3` installed at `/conf/sqlite3` (1.35 MB; 2.0 MB free left).
- [x] Hook forces `m_privacy_internet_connection_en=1` before aurora starts + eth0 watchdog
      (15 s). Kill switch: `touch /conf/no_force_net`. Tested with flag=0 + reboot → restored.
- [x] Privacy re-applied: ads/personalized/marketing/3p = 0, internet = 1; persisted across reboot.
- [x] Samba TV blocked by IP (hook iptables rule); hosts block verified for plog/orion/search.
- [ ] Optional: rebuild `/conf` free space budget if more tools are added (2.0 MB left).

- [x] Nondestructive workaround: `tools/ensure_awake.py` + `tools/tvssh.sh`
      (ping/port probe → WoL → wait for sshd).
- [ ] Optional (destructive-ish): patch `NoSignalCapability` in the overlay so the TV never
      sleeps on no-signal. Only if the ~3 min suspend becomes a problem.
- [x] Cause identified: HDMI1 no-signal timeout (~3 min), not a misconfigured setting.
## P1 — hook robustness (the 2026-10-30 outage was self-inflicted, but exposed fragility)

- [x] DHCP retries added (`udhcpc -t 5 -T 3 -A 3`) in hook + watchdog.
- [x] Normal hook paths reach the vendor exec; native attempts use the tested supervisor.
      This does not guarantee recovery from a hung setup command or a failed supervisor exec.
- [x] **Dropbear respawn tested** historically; now maintained by keepalive alongside
      telnet (five-second idle cadence, with bounded SQL/DHCP work between passes).
- [x] **Access/privacy maintenance, 2026-10-31:** keepalive.sh checks SSH 22 and
      telnet 2323 listeners, key/home permissions, hosts and Samba block every 5 s;
      corrects privacy/internet/WoL DB values each minute only when different;
      brings Ethernet up and retries bounded DHCP. SSH/telnet listener restart tests
      and post-recovery boot checks passed. See `artifacts/reports/STABILITY-2026-10-31.md`.
- [x] `/etc/hosts` bind guarded (skipped if `$S/hosts` missing).
- [x] Log cap: rotates `tvhack.log` → `.old` at boot above 200 KB.
- [ ] Decide on hardening: drop unauthenticated `telnetd:2323`, move ssh off 22, key-only
      (password auth is already unusable — unknown root hash).

## P2 — runtime `/vendor` overlay (practical "unlock" without breaking signatures)

- [x] **Aurora static analysis done** (Ghidra): `script.txt`/`IRsim*` are dead code in 0.51.52.3;
      the only protocol-fed shell injection (`Robotest copyDirectory`) is behind the broken
      12345 server. Details: `artifacts/reports/AURORA-INFLUENCE-2026-10-30.md`.

- [ ] Add overlay stage to the hook: on first boot `cp -a /vendor /buffer/vendor_rw`,
      patch, then `mount --bind /buffer/vendor_rw /vendor` before appmgr starts.
- [x] **DIAL restored** — `com.nvt.dialserver` runs standalone as root from the hook (no overlay
      needed), so `56789/56790`/`dd.xml` are back. Keys, `browserseturl` and `tvstate` all work.
      Report: `artifacts/reports/DIAL-KEYS-SCREEN-2026-10-30.md`.
- [x] **Screenshot over SSH** — root can read `/dev/fb0..3`; `tools/tvshot.py` dumps an OSD layer to
      PNG, which is the oracle for "did the key/command land" (no overlay, no screen watching).
- [x] Full **virtual RC keymap** pulled from the TV (`/vendor/vk_keymapping_db.xml` +
      `ice_keymapping_db.xml`, copies in `artifacts/protocol/`): arrows 1019–1022, OK 1053,
      EXIT 1010 / KeyExit2 1037, source 1056, plus the `Keyboard*` text-entry codes.
- [ ] Use it to neutralise app-level telemetry (plog, ACR, portal calls) rather than only
      blocking at DNS.
- [ ] Gate it on a flag file (`/conf/overlay_enable`) so it can be disabled without a reflash.
- [ ] **Custom UI** (analysis: `artifacts/reports/UI-STACK-2026-10-30.md`): aurora is DirectFB 1.7
      (`fbdev` + `tdga` gfxdriver + sawman) drawing onto the OSD framebuffers; the theme is the
      asset packs in `/vendor2/dynamic_ui`. Web prototyping and asset extraction are done
      as separate milestones below; fb2 painting was tested and was invisible. The active
      native ownership/scanout and parity work is P8/P9, not a new blind fb2 probe.
- [ ] Measure boot-time and space cost (154 MB copy, one-time; buffer has room).

## P3 — firmware signature / "unlock" investigation (blocked on physical access)

- [P] [ ] Run `artifacts/scripts/spi_dump.scr` (8 × 256 KB SPI reads, read-only) and store
      `spi0..7.bin` in `artifacts/dumps/`. We currently only have 256 B of the 2 MB SPI.
- [ ] Disassemble **xboot** and answer: does it verify **U-Boot** (vhash compare vs TEE call)?
- [ ] If U-Boot is *not* verified: assess patching U-Boot to skip kernel verification —
      note it is still insufficient alone (the kernel verifies `fs0/ap0/ap1` via TEE), so a full
      chain break would need U-Boot + kernel + custom rootfs. Weigh losing the USB recovery path.
- [ ] If U-Boot *is* verified: record the chain as closed (root of trust in xboot/OTP);
      do not pursue further without a TEE exploit.
- [ ] Document the TPS/VHASH layout (`nvt_calc_*_vhash`, `nand_tps_*`, `nvt_sign` volume) for the record.
- [ ] Keep the full 512 MB NAND dump (`dumps/tv_nand.bin`, md5 `36de1801478ce38c8042845374add41d`) current.

## P4 — telemetry verification and policy (after P0)

- [ ] Confirm `m_logging_enabled=0` really stops `plog.vstlsrv.com` (or whether only the
      `/etc/hosts` block is doing the work).
- [ ] Check what breaks with `orion.portaltv.tv` blocked (portal app list / notifications) and decide.
- [ ] Decide whether to keep OTA (`s3-eu-west-1.amazonaws.com`) allowed — currently yes.
- [ ] Optional: add the same blocklist at the router/DNS for defence in depth.
- [ ] Re-run the telemetry report after changes and update `artifacts/reports/TELEMETRY-2026-10-30.md`.

## P4b — OTA / Callisto playground (see `artifacts/reports/OTA-2026-10-30.md`)

- [ ] Trigger an OTA check from the TV menu (Settings → Software update) and capture the
      descriptor fetch; confirm it really is `https://s3-eu-west-1.amazonaws.com/vstlupdate/0009df/7706.dcf`
      and whether any token/query is added.
- [x] No `.dcf` is cached on disk (searched `/conf`, `/buffer`, whole rootfs) — only during an
      active update. Descriptor would have to be captured live.
- [x] Dumped the 9 × 256-byte key slots at `.rodata 0x12dd3e6`; fingerprints in the report.
      Identified the embedded PEM material as PolarSSL/mbedtls *test* keys (no vendor key).
- [ ] [P] Try redirecting `s3-eu-west-1.amazonaws.com` in `/conf/hosts` to a local HTTPS server
      to see the exact request (path/headers/SNI) and whether the client validates the cert
      (likely yes via `/vendor/ca-bundle.crt`) — capture only, do **not** attempt install.
- [ ] Inspect `ICE_CallistoSetCustomSWUpgrade` and the USB path
      (`CheckUsbToUpgrade` / `StartUpgradeSoftwareFromUSB`) — is a "custom" descriptor honoured
      for USB upgrades, and does it still require the RSA signatures?
- [ ] Decide whether to block `s3-eu-west-1.amazonaws.com` (currently allowed so updates work).
- [ ] Cross-check the descriptor identity against the mobile app / Vestel `InsertDB.aspx` /
      `FTPUpdateInfo.aspx` endpoints seen in the same module.


## P7 — custom launcher UI (plan: `artifacts/reports/LAUNCHER-PLAN-2026-10-30.md`)

- [x] **P0 capability probe DONE** (`artifacts/reports/P0-BROWSER-PROBE-2026-10-30.md`):
      browser = **Chromium 67** (OMI 4.13.5), viewport **1280x720 CSS** (DPR 1.5), 24/28 features;
      arrows/OK/digits arrive as DOM events (MENU/EXIT are eaten by the TV); same-origin CGI works,
      cross-origin POST to DIAL does **not** (CORS) → CGI proxy required; **EXIT closes the page and
      returns to stock TV**, `browserseturl` re-opens it; a live page keeps the TV from sleeping.
- [x] **P1 mock launcher DONE** (`artifacts/reports/P1-MOCK-LAUNCHER-2026-10-30.md`): `launcher/`
      (index.html + cgi-bin/act) served from `/buffer/launcher` by busybox httpd on 8181; tiles
      TV(1037) / Sources(1037+1056) / Apps(1037+1048) all verified on the real screen. Learnings:
      the browser eats TV-control keys (EXIT first, then the key), the Bronlijst times out in ~2 s,
      fb0 and fb1 are BGRA (tvshot fixed 2026-10-31), CGI needs Content-Type + blank line.
- [x] **Input/source switching found**: `KeyAux` (1056) opens the native **Bronlijst**
      (TV / BACK AV / HDMI1-3 / YPbPr / VGA-PC / Wireless Display), opens on the current source and
      **wraps**; `1056, DOWN x3, OK` = HDMI2, verified by the `HDMI2 | HDMI` OSD banner. Send keys as a
      tight burst (~0.25 s) — the list is transient. A "Sources" tile can either just press 1056 and let
      the user navigate, or compute `(target - current + 9) mod 9` moves.
- [x] **P2 boot integration DONE** (`artifacts/reports/P2-BOOT-INTEGRATION-2026-10-30.md`): hook starts
      busybox httpd on 8181, an opener loop flips the screen to the launcher once the portal is up
      (attempt 6 in practice), the page writes `/conf/launcher_ok` through the CGI, and the watchdog
      removes `/conf/launcher_enable` after 3 failed boots (tested with a broken page: stock UI intact,
      httpd stopped). `httpd` respawn verified. Host control: `tools/launcher.sh open|close|status`.
- [x] **Re-entry solved** (`artifacts/reports/P2b-PORTAL-TILE-2026-10-30.md`): the stock portal's tiles
      are rows in `settingsDB.db` → `LauncherItem` (`m_item_type=2`, `m_parent_id` 1=Home/0=Apps,
      `m_application_url`, `m_application_thumb_url`, `m_order_index`). Two rows with
      `m_original_id=900000` put an **OmarchyTV tile first in the portal**; its icon is downloaded from
      our httpd; **OK on the tile opens the launcher**. Full loop: boot→launcher, EXIT→stock TV,
      MENU→portal, OK→launcher. Tool: `tools/portal_tile.sh add|remove|status`. Gotchas: id 900 collides
      (wrong caption), reboot needed, the app rewrites some row fields.
- [x] **P3 DONE** (`artifacts/reports/P3-OMARCHYTV-PORT-2026-10-30.md`): upstream `webos/app` ported to
      `launcher/omarchy/` (≈1.3 MB) with a `platform.js` shim (`OMARCHY_PLATFORM="vestel"`, app/input
      lists, `OMARCHY_LAUNCH` → `/cgi-bin/act`), two 20-line hooks in `app.js` and a `width=device-width`
      viewport. Opt-in boot was verified historically (currently disabled); workspaces 1-5, TV tile (EXIT), Sources tile (Bronlijst) and the
      stock-portal tile all verified. Gotcha: the app scales its own 1920x1080 stage — don't add a second
      transform; `width=1920` in the meta breaks it on our 1280x720 CSS viewport.
- [x] **Offline test rig** (`artifacts/reports/TVSIM-2026-10-30.md`): `tools/tvsim.py` emulates the control
      plane (static host on 8181, CGI shims, fake DIAL 56789/56790, state WS 7681) and drives headless
      Chromium over CDP at 1280x720 CSS / DPR 1.5. `tools/tvsimctl.sh key|load|shot|state|dial`. Verified:
      page renders 1920x1080 like the TV, RIGHT moves focus, Sources tile closes the page (state SOURCE),
      `browserseturl` returns (state PORTAL), boot/keepalive handshakes. No TV needed for UI work.
- [x] **Native splash artifact built**: `launcher/bootsplash/splash.c` renders after OSD init.
      The original pre-aurora negative result was superseded by a later visible quarter-screen
      boot paint (`OSD-INIT-RE-2026-10-30.md`). Full-screen cold startup remains open in P9;
      neither experiment proves a complete independent display init.
- [x] **DirectFB ioctl vocabulary recovered and corrected**: kernel decompilation proves
      131/132 are palette set/get `{start,count,pointer}`, not frame-buffer acquisition.
      Region 107/108 and FBIOPAN_DISPLAY remain valid. The pan handler binds buffers and
      commits via pfUpdateEnable, including without rotation. See OSD-BUFFER-CONTRACT-2026-10-31.md.
      Independent cold setup and on-panel acceptance remain open.
- [x] **DRM driver identified**: `ntkndpdrm.ko` exports DRM interfaces and imports the
      vendor fb handler `_ker_osd_drm_fb_Ioctl`. Standard KMS equivalence is unproven: saved
      measurements include unusable dumb-buffer mmap and SET_MASTER. Do not assume DRM
      discovery or a dumb framebuffer replaces the vendor OSD setup.
- [ ] **Remote protocol partially recovered** (`IR-UART-2026-10-30.md`): vendor driver
      uses `/dev/ttyS2`, 115200 8N1, 9-byte frames and MCU/vendor mapping tables. `irkey`
      and `mod_ir` exist. Live frame-byte meanings, repeat/release behaviour, chassis key
      normalisation and native UI navigation are not confirmed; do not call this solved.
- [x] **CEC spec recovered** (`artifacts/reports/CEC-SPEC-2026-10-30.md`): the normative text is the public
      **HDMI 1.3a Supplement 1 (CEC)**, 97 pages. aurora is a *faithful* implementation - 0x46/0x47 are
      directly-addressed only (so rejecting a broadcast Set OSD Name is correct), [OSD Name] is 1..14 ASCII,
      the follower "shall store the name and use it in any menus associated with that device", and a TV "may
      send <Give OSD Name> whenever it discovers a new device". The LA-4 workaround is a legal use of
      <Request Active Source>. No spec deviations found.
      Consequence for the Steam Deck: it **shall** answer 0x46 *if it implements the feature*; if it does not,
      the name can never be learned - and there is no EDID fallback on this chassis (HDMI is `ntkndphdmi.ko`,
      `/sys/class/drm` only exposes LVDS).
- [x] **Spec-vs-aurora CEC comparison** (`artifacts/reports/CEC-SPEC-VS-AURORA-2026-10-30.md`): the CEC code is
      two layers (ICE `ice_cec.c` transport/replies + `NovoCECController` policy). TV-relevant 1.3a coverage is
      essentially complete (One Touch Play, Routing, OSD Name, Power Status, SAM/ARC, RCP, vendor IDs); only
      optional Device Menu Control (0x8D/0x8E) and `<Set OSD String>` (0x64) are absent. It is *stricter* than
      the spec (operand-length checks on every branch, no reply to broadcasts, proper Feature Abort with reason).
      The notable non-spec parts are three workarounds: the LA-4 -> Request Active Source machine, the
      `==WA==` destination rewrite in `ICE_CECSendData`, and `ForceAMPInitial`.
- [ ] **Confirm the IR frames + button map on the set** (needs the TV awake, ~5 min): run `irkey`, check
      `B7`/`B8` and whether `0x5E` is release, then press every button once and label `cmd` -> silkscreen key.
      `irkey` steals bytes from the stock UI while running, so do it with the set idle.
- [x] **DRM probe performed historically**: ndp driver/planes enumerated; dumb mmap and
      SET_MASTER failed in recorded measurements (`OSD-INIT-RE-2026-10-30.md`). This closes
      the measurement task, not the independent native OSD/scanout gate.
- [x] **Reversed aurora's own initialisation** (`artifacts/reports/AURORA-INIT-2026-10-30.md`):
      `NovoSystemInitialiser::initialise()` = 388 calls building every manager (config/NVRAM, tuner + operator
      scan profiles, DVB components, EIT/EPG, picture/sound/video, DVR/PVR, UI/OSD, portal/apps, WiFi/IoT/
      Alexa, logging/diagnostics/hotel/burn-in, Samba ACR + Kochava); `initialiseNovoSystem()` = 99 calls
      (NovoLauncherManager + VestelMarketClient + Gracenote + search, first-run settings, DVR, then
      **initialiseBrowser** — which is why `browserseturl` needs the boot to settle). `DialController` and the
      7681 WS are constructed there too. Conclusion: the launcher-on-aurora design is right; a full
      replacement means re-implementing a television.
- [x] **Offline file extraction** (`artifacts/scripts/extract_from_dump.sh`): carve mtd12 (`fs0`) out of
      `dumps/tv_nand.bin`, extract the UBI volume (`root`/`vendor`/`vendor2`), unsquashfs, copy any file.
      Verified: the dump's `libdirectfb_tdga.so` is byte-identical (`a770a7e5…`) to the copy pulled over ssh —
      so libraries/assets no longer need the TV awake.
- [x] **Mapped source handling + CEC** (`artifacts/reports/SOURCES-CEC-2026-10-30.md`): the list is owned by the
      ICE layer (`ICE_ExternalNumberOfInterfaces/GetName(i)`, 13 descriptors × 0x38 B, name at +0x2c) and
      surfaced by `ExternalAVSourceManager` → `EASITVControlInterface` → `NovoExternalAVSourceNavigator` (the
      Bronlijst). Switching = `selectExtSource(idx, secondary)`: per-window current/previous index, video and
      audio routed separately (plus an audio-description path), ARC and SCART mute as separate switches.
      CEC = thin transport (`CECInterface` FROST thread + `ICE_CECInit/Open/SendData`, `read()` →
      `CECMessage{init,dest,opcode,payload}` → `CECMessagePublisher`) with policy in `NovoCECController`
      (logical address/device type, audio status, RCP passthrough, enabled), MCU flags
      (`_HW_MCU_SetCEC*`) and a 50-entry `CECKeyMapper` table (ICE key → CEC user-control code). A CEC device
      appears as its HDMI port — not as a separate source entry.
- [x] **Answered: CEC does rename inputs** (`artifacts/reports/CEC-INPUT-RENAME-2026-10-30.md`).
      Hotplug `Report Physical Address (0x84)` → `getHDMIName()` with no stored name → TV sends
      `Get OSD Name (0x46)`; the device's `Set OSD Name (0x47)` is truncated to 14 bytes and stored per
      CEC logical address (`NovoCECController` stride 0x128, +0x13c) and mirrored onto the peer LA on the
      same physical address. `InputManager::getExternalInterfaceName()` prefers that name over the
      user-set `m_source_names` entry and over the factory name, but only for connector type 7 (HDMI).
      A broadcast 0x47 is rejected by `isMessageValid` — only a directed reply counts. The name is RAM-only
      (`clearDevice` drops it and restores the default label), so it is re-learned after each boot/hotplug.
      `CIDSetInputName` is a separate side effect feeding the Samba/CID layer, not the OSD label.
      The LA-4 `Set OSD Name` → `Request Active Source (0x85)` state machine in `_ice_cec_Handler` is a
      dormant test-tool injector, unrelated to renaming.
- [ ] P3 follow-ups: streaming apps remain disabled by debug.ini; unsupported upstream features
      stay hidden. Weather, TV stats, localisation and screensaver work are recorded in MEMORY;
      do not treat the older "not wired" list as current. Only two wallpapers are bundled.
- [x] Stock re-entry route researched and tested historically: portal tile plus optional web boot
      opener (P2/P3 above). Native replacement is now the active P8/P9 direction.

## P5 — payload / stick maintenance

- [x] Rebuilt `payload/apdat0_full.ubifs` from a live `/conf` snapshot — md5
      `76b1464a4b70566b014a58ac39ef795b`, round-trip verified (`ubireader` extract = staging tree).
      Historical hook `2b1d4b7dc7acad8ca0e9b5ccaff03da7`; owners preserved (101 = idtv, 0 = root). Repeatable:
      `artifacts/scripts/rebuild_payload.sh` (`--dry-run` builds + verifies only).
- [x] Image rebuilt again after P2 (launcher in the hook): md5
      `76b1464a4b70566b014a58ac39ef795b`, hook `2b1d4b7dc7acad8ca0e9b5ccaff03da7`; runtime launcher
      flags excluded from the image.
- [x] Latest recovery image copied, verified and successfully used on the 8 GB
      TVHACK stick. Use `artifacts/recovery-stability/kylo_usb_update.scr` (exact
      old 584-byte script); image MD5 `994da7ff76a0b2fbafb8f6fa0c682627`.
- [ ] Re-create `n00..n63.bin` on the stick with `dumps/split_for_usb.sh` if the full-restore
      path is needed.
- [ ] Re-run the no-op write test (`noop_write_apdat0.scr`) after any image change.
- [P] [ ] **Live dd flash test** (needs physical access): `artifacts/scripts/flash_image.sh tv
      artifacts/payload/apdat0_mtd13.img --yes` — copies, `ubidetach 13`, `dd` to `/dev/mtd13`, reboots.
      Success = `/conf/tvhack.log` has a fresh `launcher: OPENED` and ssh comes back. Rollback = the
      stick (`make_stick.sh`) or `make_nand_image.sh` with a programmer.
- [ ] Store the full SPI dump in `artifacts/dumps/` once P3 runs.

## P6 — docs

- [x] Update current-state notes and audit TODO against code/live evidence (2026-10-31).
      `TODO-AUDIT-2026-10-31.md` distinguishes current observations from historical tests.
- [ ] Update `MEMORY.md` with the final privacy state and the outage lesson
      (`m_privacy_internet_connection_en` is functional, not cosmetic).
- [ ] Add a short **runbook**: how to recover (USB restore), how to re-enable network,
      how to re-apply privacy, how to re-flash the payload.

## P8 — openaurora: a perfect `aurora.elf` replacement (design: `openaurora/docs/ARCHITECTURE.md`)

Target, in this order: (1) bug-for-bug compatibility with the stock binary, excluding
telemetry and advertising as specified above, module by module; (2) the
OmarchyTV home screen implemented natively on top (P9). Modules are C, `dlopen()`ed, hot
reloadable, and only ever entered from a single-threaded event loop — which is what makes
callback dispatch serial. Workers must stop/join before module cleanup returns.

**Core (implemented; lifecycle/ownership gaps remain after the 2026-10-31 audit)**
- [ ] host: event loop, services, events, timers, fd watches, config and `post()` exist.
      Host-loop heartbeat, signal shutdown, hold-capacity rejection and owned thread posts
      are tested. Broader lifecycle acceptance, including suspend/reload semantics, remains open.
- [x] Worker posts carry an instance token; stop/unload/failed init/start cancel queued
      callbacks and reject late posts. Dispatch restores owner attribution and pops one at a
      time so earlier callbacks can cancel later work. Real worker fixtures pass on host,
      under ASan/UBSan and ARM/qemu. Module authors must join workers and reclaim userdata.
- [x] module ABI 2 (`include/openaurora/module.h`): magic/version/descriptor-size validation,
      requires[], lifecycle and save/restore hooks. Guard-page fixtures prove old ABI and
      short descriptors reject before unavailable fields; larger compatible prefixes load.
      Core and modules must be rebuilt together; installed TV/recovery builds remain ABI 1.
- [ ] loader: path resolution, required callbacks, magic/version checks and service-hold
      checks exist. Audit reproduced and fixed full-hold-table leakage, stop→unload bypass
      and failed-init registration leakage; regression tests cover each. Post ownership and
      descriptor-size checks are fixed above. **Still open:** restore() errors are ignored
      and reload always starts the replacement, including a previously stopped module.
      Define/test those semantics before claiming complete lifecycle compatibility.
- [x] Corrected PWM declarations against decompilation **and return instructions**:
      one maintained header has six-/four-word layouts and scalar returns for
      Init/Open/Set/Terminate; Close takes no argument. Panel and fetch script use it,
      and copied vendor header matches. Earlier void-return claim was incorrect.
      Strict host/ARM builds pass. Panel hardware acceptance remains open below;
      nonzero return alone does not prove an ioctl/write. See PWM-CONTRACT-RECHECK-2026-10-31.md.
- [x] control plane: unix socket + stdin — `list`, `status`, `load`, `start`, `stop`,
      `unload [--force]`, `reload [--force]`, `suspend`, `resume`, `loglevel`, `quit`.
- [x] **`--check` runtime preflight:** file-only module ABI note/entry validation;
      own ELF interpreter traces library lookup, symbol versions and all relocations.
      Constructor/init/start/socket/hardware calls are bypassed. Diagnostics catch
      unresolved symbols even when glibc exits 0. Six host regression groups pass;
      full ARM mode and failure fixtures pass with TV libc under qemu. Rebuilt
      core plus all eleven modules **passed on the actual TV**, exit 0, after
      separate size/hash verification. Temporary staging removed; vendor PIDs,
      boot ID and persistent binary unchanged. See RUNTIME-CHECK-2026-10-31.md.
- [ ] At a future authorized deployment, replace the older installed audio/panel/source
      artifacts with the complete matching build: their standalone traces currently
      report unresolved v4l2_open/v4l2_ioctl. Current rebuilt artifacts pass strict
      real-TV checks. Do not mix the installed ABI 1 core with ABI 2 modules.
- [x] `make` / `make check` pass: existing lifecycle smoke checks, registry regressions
      and 11 recovery integration cases, plus six module-safety groups. Host and ARM builds
      compile with `-Werror -pedantic`; safety checks also pass under ASan/UBSan and ARM/qemu.
      These tests do not establish full module safety,
      real IR handling, hardware parity or non-default UI selection survival.
- [ ] `mod_ir`: UART reader, resynchronisation, complement check and key events exist.
      The live RC-code byte is still unknown; f[3] is heuristically treated as a code,
      while consumers also expect NEC bytes. Repeat is never emitted and 0x5e is assumed
      to mean release. Finish and validate the protocol before marking it complete.
- [x] `mod_ticker` (clock service, counter survives reload) and `mod_ui_console` (text UI proving the
      UI lifecycle; selection survives reload).
- [x] **Vendor HAL strategy** (`artifacts/reports/OPENAURORA-VENDOR-HAL-2026-10-30.md`): the image ships
      reusable vendor libs; dynamic-symbol recount finds **321 distinct defined Hal_* functions**
      in libNDAL3 and **80 NDAL_* functions** in libNDAL_EX. Older 121/643/160 counts were
      inconsistent. TV modules link the HAL libraries; the host dlopens the modules with RTLD_LAZY.
      This establishes a reuse strategy, not complete amp or display bring-up.
- [x] `mod_panel`: backlight via `Hal_Pwm_Init/Open/Set_ByChannel`, channel+freq from config. `make tv`
      links libNDAL3.so (+NDAL_EX/asound/mhash/nteec closure) with `-Wl,-rpath,/vendor/lib:/vendor/lib/libs`;
      host builds get `-DOA_NO_VENDOR` stubs. **Validated under qemu against the TV's glibc 2.23**: the
      module loads, reaches the HAL, and the only failure is the missing `/dev/nvt/ntkled` node off-TV.
      Modules dlopen with `RTLD_LAZY` because the vendor HALs import symbols the vendor app supplies.

**Parity scope (Ghidra census: `artifacts/reports/OPENAURORA-PARITY-2026-10-30.md`)**
The stock binary has 84,547 functions and is dominated by the broadcast stack (tuner/EPG/PVR/
teletext/CI+), not the UI. Parity is therefore tiered, and T2 is chosen per household.

**T1 — a working display (must all exist; plans: `artifacts/reports/OPENAURORA-T1-PLANS-2026-10-30.md`)**Reusable vendor surface includes `Hal_*` (libNDAL3, 321 distinct defined dynamic functions) + `NDAL_*` (libNDAL_EX, 80); `PLAT_*`/
`DEV_*`/`ICE_*` are statically inside aurora and cannot be called. That means `source` uses
**`Hal_Vin_*`** (22 fns) and `display` can use **`Hal_Video_Libdrm_*`** — neither has to be written from
scratch. Recommended order: power → audio → source → display → settings → services.
- [ ] `display` — fbdev renderer implemented: mmap, BGRA drawing, blits, region RMW,
      mirror/pan, display service. Earlier on-panel paint with vendor-initialised OSD is
      recorded. **Open:** independent cold bring-up, scanout/buffer binding, enable state
      and suspend/resume restoration. Runtime paint is not completion of the display module.
- [ ] `ir` — device profile map is extracted (`IR-KEYMAP-2026-10-30.md` and CSV), but
      not integrated as a confirmed canonical input mapping. Identify the UART code field,
      translate it consistently, confirm repeat/release and exercise actual UI navigation.
- [ ] `power` — sysfs suspend + WoL was tested historically; the module publishes a
      service and emits power events. **Open:** `standby()` does not run the other modules'
      suspend/resume hooks; host `suspend` leaves modules marked suspended after wake until
      a separate `resume`. No native end-to-end power lifecycle test is recorded.
- [ ] `panel` — skeleton done (`mod_panel`); **runs on the TV and reaches the driver** (first openaurora
      run on the set: `Open /dev/ntkled successfully`, all 13 channels accepted). A channel sweep produced
      only a **brief flicker**, i.e. the level is applied and then overridden while aurora owns the panel.
      Contract checked against decompilation and instructions: Set takes
      `{freq,duty,shift,mode}`, `mode<9`; Open takes 6 words. Init/Open/Set/Terminate
      return scalars, Close takes no argument; earlier void-return claims were wrong.
      Nonzero results do not establish a hardware write. Board constants read
      from .data (`gu32PanelFreqInHz=15000`, `genPwmdimMode=1`). Remaining: find what re-applies the cached
      `gstBacklightPara`, or verify in the native shell with aurora stopped. Report:
      `artifacts/reports/PANEL-BACKLIGHT-2026-10-30.md` and
      `artifacts/reports/PWM-CONTRACT-RECHECK-2026-10-31.md`.
- [ ] `audio` — volume/mute glue and vendor init calls exist; earlier volume calls ran
      on the set, but output/mute/routing were not verified audibly. It repeats the audio
      prelude after `plat` and declares Protect_Init(void) where plat passes an int.
      Resolve the ABI/init ownership and validate soundbar/speaker behaviour before completion.
- [ ] `source` — VIN open/select/start glue and id table exist; open was observed on
      the set. Switching stays disabled by default and has not been verified. The module
      omits the companion VPLT command, `Hal_Video_Set_V4l2Info`, audio routing and human
      source enumeration from the recovered vendor path. This is a partial backend.
- [ ] `settings` — **generic integer service implemented**, read path observed on the set
      (`openaurora/modules/settings`). Typed access, BLOB/string handling and NVRAM are incomplete.
      The `Settings`
      table is a single row of named INTEGER columns, so the module exposes `get_int(col)` / `set_int(col,v)`
      / `is_writable()`; column names are validated against `[A-Za-z0-9_]` because SQLite cannot parameterise
      an identifier. **Read-only by default** (`settings.readonly=1`) — aurora caches these values. Links the
      firmware's `libsqlite3.so.0` on the TV and the system sqlite on the host, with a vendored
      `third_party/sqlite3.h` so both builds agree. Verified on the set: opened
      `/conf/sqlite_db/settingsDB.db` read-only, `m_cec_enabled=1`.
      Still to do: typed accessors for the columns the UI needs, and the NVRAM (MCU blob) side.
- [ ] `services` — DIAL 56789/56790, state WS 7681, DLNA 2870 (mostly proven standalone).

**T2 — only if actually used (decide and descope the rest)**
- [ ] `cec` — the dispatcher we mapped (`CEC-SPEC-VS-AURORA-2026-10-30.md`).
- [ ] `apps` — run the vendor OMI/`OperaEngine` browser as an `apps` backend (vs replacing it).
- [ ] `pq` — `PLAT_PQ`/`PLAT_FRC`/`NovoPictureSettings`.
- [ ] `network`, `bt` (`BluetoothController`).
- [ ] `teletext`/`mheg` — `TeletextIF`, `MHEGClient`, `fvp::FreeviewPlay` (was missing from the plan).
- [ ] `tuner` + `epg` — `TunerManager`, `ServiceFinder`, `DEV_NIM` (76 fns), DVB-T **and** DVB-S.
- [ ] `pvr` — `DvrController`, `NebulaPvrRecordingScheduler`, thumbnail/playback.
- [ ] `ci` — `CHIL`, `INCA`, `CAStackServer`, `CIStackProxy` (~850 fns).

**T3 — cloud/optional (probably never): `portal`/NEBULA, `voice`/AVS, `hotel`, global search, OTA.**

- [ ] **decide T2 explicitly** — if the set stays an HDMI monitor, descope tuner/EPG/PVR/teletext/CI+.
- [x] **TV toolchain solved**: the set is glibc 2.23, so the TV build uses the *Arm GNU Toolchain
      10.2-2020.11* (`arm-none-linux-gnueabihf`, glibc 2.31). Verified offline by running the ARM build
      against the TV's own libc under qemu (`qemu-arm-static -L <rootfs>`), and by keeping every binary's
      max symbol version at `GLIBC_2.17` or lower (`fcntl` is versioned 2.28, so the core uses `pipe2`/
      `SOCK_NONBLOCK`). `make tv TV_CROSS=...` is the TV build; `make`/`make arm` stay stub-built.
- [ ] **acceptance**: boot goes straight to openaurora with `aurora.elf` never started, and every
      feature in the agreed scope still works.

## P9 — OmarchyTV as a native openaurora UI module (`mod_ui_omarchy`)

Needs `display` + `ir` + `settings` from P8. Keeps the current web launcher as the reference for
look and behaviour, and DIAL as a fallback control surface.

**Parity assets extracted** (`artifacts/reports/UI-ASSETS-2026-10-30.md`): `/vendor2/dynamic_ui` holds the
vendor theme — `telefunken18_img.txt/.bin` decoded and **964 PNGs extracted to `artifacts/ui-assets/`**
(index = `type name hash w h size flag`, data concatenated in order), and `telefunken18_svg.bin` is not
vector art but the stock **layout description** (`background`, `rowItems`, `title_gRowL`, `#FFFFFF`,
`matrix(…)`, `/png/picture.png`). Manifest + layout + indexes copied to `artifacts/ui-assets/_layout/`.

- [x] **First native render on the set**: `mod_ui_fb` + `mod_display` painted a home screen on the OSD
      layer (dark background, top/bottom bars, 6 tiles) using the **vendor's own icons** — confirmed
      visually. Icons come from `artifacts/ui-assets/` (theme pack) converted to `OAI1` blobs by
      `openaurora/tools/icons-to-raw.py`; `mod_display` gained a `blit()` (alpha-blended) for it.
- [ ] turn the canvas test into the parity portal: **geometry and content are now the vendor's**
      (`artifacts/reports/UI-PARITY-2026-10-31.md`: 1.62/255 mean pixel error vs the stock portal).
      Still to do: tile hit-testing + `ui/action` events, and the remaining labels.
- [x] **Colour order fixed**: `tvshot.py` decoded fb1 as RGBA (it is BGRA), so every reference capture
      had red/blue swapped and the palette sampled from it was wrong. Tool + `shot-launcher.png` +
      `ui_fb` corrected; the rail is orange→maroon (`#f7941e … #481730`), and `#BE1E2D` matches a
      literal in the vendor style pack.
- [x] **Real content pipeline**: `openaurora/tools/mk-ui.py` builds the `OAI1` blobs from the device's
      own data — rail glyphs cut from the portal framebuffer, the nine cards from
      `/buffer/downloaded_images/launcher_app_thumb<N>` (the `LauncherItem` rows), the title from
      `/vendor/fonts/DefaultProportional.ttf`.
- [ ] **BLOCKER — own the OSD layer**: both fb0 and fb1 belong to vendor aurora and fb1 is the upper
      one; aurora updates by *region* and its no-signal banner repaints ~1 Hz, so a second writer of
      whole frames interleaves as strips. Nothing can be "held" alongside it. Requires replacing
      aurora as the OSD owner.
- [x] **MCU watchdog solved** (`artifacts/reports/PANEL-TAKEOVER-2026-10-31.md`): `pcbconfig`'s 6-byte
      packets on `/proc/nvtstbcipc` (cmd `0xae`) disable it — `openaurora/tools/mcu/wdtctl.c`.
      Verified: vendor aurora can be SIGSTOPped and the set stays up. Going through libNDAL_EX's
      `STBC_*` directly resets the set (needs the full MCU state machine) — do not retry that.
- [x] **Crash-reset path found** (the "bricking"): the kernel core pattern pipes every crash into
      `/usr/sbin/core_helper.sh`, which — unless `/conf/coredumpnoreset` exists — detaches `/mtd_rwarea`,
      `/conf` and `/buffer` and arms the MCU watchdog at **1 s** (`pcbconfig -i -w 1`), resetting the
      SoC. That is what happened on each of our crashed probes (`always_logging_reset_count` = 2).
      `/conf/coredumpnoreset` is the documented off-switch, is now set, and was **verified**: a
      deliberate SIGSEGV left the set running (uptime advanced, counter 2→3). The other path is
      `/vendor/run.sh`'s `trigger_reboot &`, which waits for `/tmp/trigger_reboot` and then kills
      `aurora.elf`, arms 10 s and detaches the same partitions. Neither is a process supervisor.
- [x] **Watchdog paths researched** (`PANEL-TAKEOVER-2026-10-31.md`): aurora has an
      internal kick thread; appmgr did not respawn it in the recorded experiment. A single
      FreshWDT call with the watchdog disabled returned 1; this does **not** establish that
      openaurora cannot feed it. The native attempt uses disable + userspace supervision.
- [ ] **Region write partially understood**: display reads 8 words, sets size words 2/3
      and zeroes 4/5, then writes back; the config whitespace bug is fixed. Full semantics,
      cold scanout and the effect of this write on visibility are not yet verified.
      A successful ioctl/readback alone does not settle the display gate.
- [x] **Supervised takeover harness implemented**: the deployed hook uses `oa-supervise`, a separate process
      with a host-loop heartbeat plus a first-present receipt, both on tmpfs. Startup 75 s,
      heartbeat timeout 5 s, kill/reap → `wdtctl set 10` → vendor exec; three failures remove
      `oa_boot`. SIGTERM shutdown fixed. Host recovery tests and fake-hardware ARM smoke pass;
      a real native takeover remains unverified. See `openaurora/docs/RECOVERY.md`.
- [ ] **Vendor platform bring-up partially reproduced** (`modules/plat`): audio prelude,
      common init, GPIO/I2C and video open are called in recovered order; requires[] gates
      module startup. Most calls return void. The copied VIN arrays are private unused
      state, not a functional substitute for vendor routing. Hardware success is unproven;
      audio also repeats its prelude when loaded. Finish init ownership and verify effects.
- [ ] **Choose the independent OSD setup from evidence**: filtered trace absence does
      not rule out DirectFB ioctls, and `Hal_Video_DrmInit(1)` does not itself SETPLANE.
      The explicit vendor OSD commit is in `plat_Gfx_DRM_osd_drv::Update_Enable` (0x944860),
      reached through Window::Update. Decode that state/buffer path before choosing a backend.
- [x] **Offline OSD buffer contract recovered**: private create ioctl 0xc0106440, dedicated
      pool flags, MAP_DUMB/mmap64, GEM name/handle resolution, 68-byte OSD plane records,
      ADDFB2/SETPLANE and 12-byte sync-on/off payload decoded. CREATE_DUMB is confirmed to
      allocate a fixed 4 KiB; its mmap failures do not rule out DRM. Raw decompilation logs
      and addresses are saved with OSD-BUFFER-CONTRACT-2026-10-31.md. No TV experiment.
- [x] **Missing fbdev commit fixed offline**: display.pan=0 now disables rotation only;
      every present calls FBIOPAN_DISPLAY, including one-frame configurations. Failed
      sync/commit returns failure and retries the same frame. Current-frame resume fixed.
      Actual-module tests with fake fbdev pass on host, ASan/UBSan and ARM/qemu; full strict
      host suite passes. No deployment or physical scanout verification.
- [x] **The real reason `open()` failed**: the config parser kept the value's leading whitespace, so
      `display.fb = /dev/fb1` became `" /dev/fb1"`. Fixed in `core/host.c`; `tests/run.sh` now uses the
      spaced form so `make check` catches a regression.
- [x] **Historical dry run recorded** (`PANEL-TAKEOVER-2026-10-31.md`): platform calls
      completed, fb1 opened, region ioctl returned success and pixels matched the reference.
      This proves neither scanout nor cold startup. The missing v4l2/v4lconvert link
      dependencies are now in Makefile/fetch-vendor.sh. Logger accurately says void stages
      are unverifiable, rather than claiming bring-up succeeded.
- [x] **Prior review performed; fixes audited again**: window_order overflow code is
      removed, ui_fb releases its display hold on suspend, stop/start cleanup and the
      requires test exist. The claimed hold-capacity fix was incomplete; this audit fixed
      it and also fixed held-provider stop/unload and failed-init cleanup, with regressions.
      Thread-post ownership and ABI-size checks were subsequently fixed/tested offline under P8;
      other lifecycle and hardware parity items remain open.
- [ ] **BIGGEST REMAINING ITEM — is our OSD scanned out at all?**: our fb1 content is correct but
      was not visible in the recorded source-screen test. Buffer binding, enable state,
      region and stacking remain hypotheses. A vendor stacking function exists:
      `plat_drm_set_windoworder` → `Hal_Video_Drm_Set_Property(dev, 0xcccccccc, info[1], "dp_order", v, 0)`
      (mapping 0..5 → 0,2,1,3,4,5). Its previous invocation **reset the set** and does not
      establish stacking as the cause. **Decoded offline:** `0xcccccccc` is CRTC type;
      the 12-word record has words 0/1/2 = plane/CRTC/fb ID; 4..7 = CRTC xywh;
      8/9/10/11 = source x/y/height/width. Getter/setter return int (checked in assembly).
      `DrmInit(1)` and `FlushPlaneInfo` do discovery, not SETPLANE. Follow the separate
      OSD enable path; do not retry `dp_order`. Report: `DRM-PLANE-CONTRACT-2026-10-31.md`.
- [x] **`libNDAL3.so` cannot be `dlopen`ed standalone** — it needs globals from `libNDAL_EX.so`
      (`gs32Ndp_PQ_fd`), so load `libNDAL_EX` with `RTLD_GLOBAL` first, or link both.
- [x] Vendor module facts (from `/lib/modules/install_fpga.sh`): `ntkdriver_nondovi.ko` takes
      `gau32PlaneWinSize/gau32PlaneOrder/gau32NumOfBuffer/gau32SeperateBuffer/gu32FastLogoPlaneID`, then
      writes `/tmp/ntkdriver_completed` (the flag aurora's `ICE_HardwareInit` waits on).
- [ ] **Plane + region setup**: region read-modify-write is implemented. **Correction 2026-10-31:**
      `Hal_Video_DrmInit(1)` creates a DRM framebuffer and discovers IDs, but its `_kms_init`
      does not call SETPLANE. Independent OSD binding/enabling remains open; see the new
      `DRM-PLANE-CONTRACT-2026-10-31.md` report.
- [x] **Supervised test-mode implementation**: `/conf/oa_boot` selects oa-supervise;
      `/conf/oa_safe` forces fallback. Host and stub-only ARM tests pass. No real cold
      takeover has been validated with this supervisor; that acceptance remains open.
- [ ] decode the layout record format (property ids → meaning); it gives the stock portal's geometry
- [x] pre-convert the initial icon subset to OAI1 RGBA (`icons-to-raw.py`, `mk-ui.py`);
      964 source PNGs exist. Additional screens need additional assets.
- [ ] assemble the parity UI on the `display` + `ir` services using the vendor's own assets
- [x] **String reference inventory extracted**: `lang-strings.txt` contains **56,715 text lines**
      (sorted/filtered inventory from the EFSL langprofile). Earlier raw-field/unique counts
      do not describe this text file. It does not preserve string IDs, language grouping or
      a proven Dutch slot. Decode the language enum and original record alignment separately.

- [ ] port the OmarchyTV layout/theme to the native renderer (the web app's `#stage` scaling and
      theme `last-horizon` are the visual target).
- [ ] tiles → `ui/action` events instead of CGI → DIAL key posts.
- [ ] retire the browser launcher once parity is reached.

Defects of the current web launcher that P9 must not reproduce (measured 2026-10-30):
- [ ] page freezes silently — heartbeat 699 s stale while `omi_worker` sat idle; keys then do nothing.
- [ ] boot to launcher takes ~46 s (6 opener attempts); wake takes ~30 s (3-4 attempts).
- [ ] HOME does not return to the launcher — no watcher exists, and the key never reaches the page.
- [ ] the browser eats TV-control keys, so every action needs an EXIT-first dance.

- [x] Offline kernel OSD init traced: KER_OSD_DrvInit registers five framebuffers;
      SetPar configures alpha/format/stride/rectangles; physical binding accounts for
      frame order and source origin. See OSD-KERNEL-INIT-2026-10-31.md. This corrects
      the claim that Aurora itself creates the fb devices, not the open scanout gate.
- [x] OSD callback table resolved from ELF relocations: static kernel callbacks,
      with Set_FbInfoPointer storing kernel fb_info. Recovered stateful frame-order
      transitions for pan yoffset 0/yres/2*yres. See OSD-KERNEL-INIT-2026-10-31.md;
      this is static evidence, not a physical scanout test.
- [ ] Finish separate-buffer/alternate-MAU selection and zero-buffer handling; reconcile
      physical frame order with userspace GEM sizing before additional cold setup.
- [x] Offline GPU flush and DRM framebuffer classification traced: VsyncFlush
      delegates to Copy; GetFlushCopyMode returns a cached byte; GEM framebuffer
      kind gates creation callbacks. See OSD-BUFFER-CONTRACT-2026-10-31.md.
- [x] Private GEM flag provenance and size propagation traced: main/sub flags
      set framebuffer-mode +0xa9=1; page-aligned requested size reaches plane-pool
      AllocMem. See OSD-BUFFER-CONTRACT-2026-10-31.md and saved instructions.
- [x] OSD pool/AllocMem and initial front/back requests traced. Front requests S;
      Sync advertises 2*align4096(S), but OSD commit binds GEM-derived address/size
      rather than calculating backing from framebuffer dimensions. Evidence and
      remaining core-initializer check: OSD-BUFFER-CONTRACT-2026-10-31.md.
- [ ] Confirm generic GEM +0x70 size initialization and complete OSD cold
      format/stride/region/alpha/enable/update sequence before choosing a backend.
      Later surface switching/resizing and on-panel acceptance remain open.
- [x] MAU selection query recovered and helper extended with GET-only diagnostics.
      Host/ARM tests pass. Owner-authorized real-TV query: fb1 current MAU=0,
      primary 0x35bbb000, length 16588800, stride 7680, yoffset=1080;
      region 0,0,1920,1080,0,0,3840,2160. No current bank mismatch shown.
      Temporary helper removed; UI PIDs/boot unchanged. See OSD-MAU-SELECTION-2026-10-31.md.

## Older threads (low priority, no access needed)

- [ ] Panel `VESxxx` code: service-menu Product Info or panel sticker (needed for a firmware request).
- [ ] Request exact `0.51.52.3` image from Vestel Germany / Telefunken with
      `D65U546A / 2199818 / SN 8368599 900143 / MB230_VESTEL / VESxxx`.
- [ ] OTA descriptor `0009df/7706.dcf` on `s3-eu-west-1.amazonaws.com/vstlupdate` — still 403
      anonymously; check whether the TV uses a signed URL and whether it still updates
      with logging disabled.
- [ ] Confirm whether the launcher's app list really comes from `nvt.app.ini` or elsewhere
      (Netflix still shows despite `debug.ini` marking it `n`).

---

### Quick reference

| thing | value |
|---|---|
| ssh | `ssh -i ~/.ssh/id_ed25519 root@192.168.178.87` |
| fallback shell | `telnet 192.168.178.87 2323` |
| host keys on TV | `/conf/remotelf/hostkey_{rsa,ed25519}`, keys in `authorized_keys` |
| hook | `/conf/remotelf/aurora.elf`, log `/conf/tvhack.log` |
| restore `/conf` | stick: `restore_apdat0.scr` → `kylo_usb_update.scr`, OK-hold |
| full NAND restore | `restore_full_nand.scr` + `n00..n63.bin` |
| payload image | `artifacts/payload/apdat0_full.ubifs` (md5 `76b1464a4b70566b014a58ac39ef795b`; older hook) |
| pristine NAND | `artifacts/dumps/tv_nand.bin` (md5 `36de1801…`) |
