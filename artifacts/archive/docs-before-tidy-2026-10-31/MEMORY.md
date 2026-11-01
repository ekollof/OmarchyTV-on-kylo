# MEMORY.md — tvhack state (2026-10-29)

## Latest handoff — 2026-10-31

Latest continuation traced OSD reserved-pool creation, bitmap run allocation,
initial front S/back S or2S/shared allocation, and doubled advertised metadata.
Kernel OSD commit uses GEM-derived address and +0x70 size for physical binding,
not advertised framebuffer dimensions. Generic drm_gem_object_init receives
aligned request; inspect its +0x70 store next, then finish the cold OSD state
sequence. Evidence saved and buffer-contract report/TODO/HANDOFF updated. No TV
access or code changes. This supersedes the earlier allocator next-step entries.

Post-handoff offline continuation: private GEM main/sub requests set object
framebuffer-mode byte +0xa9=1 through their descriptor; requested low size word
is page-aligned and reaches the OSD-plane pool AllocMem call. Saved decompile
and instructions in oa-osd-private-gem-{create,allocation,plane}.txt. Report and
TODO updated. Next inspect _create_osd_memory_pool and ndp_drm_gem_mem_AllocMem,
then MCU table size versus doubled ADDFB2 metadata. No TV access/code changes.

See HANDOFF.md for consolidated state, validation, safety constraints and exact
resumption pointers. Last offline RE established VsyncFlush delegates to Copy,
GetFlushCopyMode returns a cached byte despite incorrect void metadata, and DRM
framebuffer creation gates on GEM object's framebuffer-mode byte +0xa9. Raw
logs are saved under artifacts/analysis/osd-2026-10-31; details appended to
OSD-BUFFER-CONTRACT-2026-10-31.md. Next trace private GEM allocation extent and
flag initialization against the doubled surface-size declaration. No TV access,
code changes or new test runs occurred during handoff preparation.

## Settled facts
- Target = Telefunken D65U546A (65" UHD, Art 2199818, SN 8368599 900143) = Vestel MB230 chassis, MB230_VESTEL, FW 0.51.52.3, brand TELEFUNKEN, country nld (portal URL), MAC 70:54:B4:FE:70:C6 / wifi 70:54:b4:fd:64:1c, UDN uuid:d3f5ce9e-....
- Platform = Vestel Linux Smart TV (Opera OPR/46, SmartTvA/3.0.0, HbbTV 1.5.1, NT72671_2019, NetFront DLNA, DIAL SmartCenter, WS 7681, portal orion.portaltv.tv). Not Android/Vidaa/TiVo.
- SSH/telnet: closed (22/23/1986/4725/9222). No network RCE. SmartCenter is not command injection and cannot start a reverse shell. `browserseturl` runs JavaScript in the TV browser and can reach loopback port 8080, which answers 404. No file read and no shell. CVE-2026-15570 is that SSRF class on MB181/TiVo, not OS command execution on this MB230 set.
- Service menu NOT confirmed open (sent MENU,4,7,2,5 via keys, all 201, no new ports 5001/56183; likely needs HDMI+hold-0+Home sequence on physical remote).
- Exact Telefunken image 0.51.52.3 still not public. A same-chassis Novatek Kylo full image is in hand for RE only: Medion MD 30010 `kylo_usb_update.bin` (243,007,488 bytes) inside `artifacts/firmware/MD30010_Firmware_Full_Update_DE.zip` (218,151,540 bytes, last-modified 2023-12-21). Do not flash it: the script runs `nand erase.chip`, burns Medion PlayReady/Marlin material, and the panel/brand will not match D65U546A. VESxxx panel code still required before any Vestel Germany request for the Telefunken package.
- This set's `mb230_swprofile.bin` is not on disk and is not served by DIAL, SmartCenter, or the state WebSocket. A save-time re-fetch of `dd.xml` on 2026-10-29 failed to connect (TV off or asleep). The earlier live document had model, `0.51.52.3`, both MACs, and the UDN, and no `.dcf` or S3 host. The Medion profile is carved out: `artifacts/firmware/medion-md30010/medion-mb230_swprofile.efsl` (EFSL at file `0xcf3df39`, 91825 bytes, `772e.dcf`, id `0009df`, host `s3-eu-w` + `*-1.amazonaws.com/vstlupdate`). That id is Medion's, not Telefunken. DCF tokens are not a hash of model, article, serial, MAC, or version. Anonymous S3 GET of real and nonsense keys both returns AccessDenied, so a guessed path cannot be checked.
- Getting this set's profile still requires the physical service-menu USB export: FAT32 stick, empty `DownloadProfile.mb230`, USB Operations. Network key injection did not open that menu.

## Firmware RE findings (2026-10-29, Medion MD30010 Kylo image)
- UBI at `0x1280020`, volumes `fs0`(5.6MB, 556 inodes)=system root, `ap0`(154MB, 7429)=Vestel app+vendor, `ap1`(27MB, 298)=modules/html. `ubi_reader` + `unsquashfs` work; raw offset carves do not (UBI headers interleave).
- System root `fs0`: busybox 1.x with `telnetd/ftpd/httpd/tftpd` (NO sshd), `/etc/passwd` root has MD5-shadow hash, `inittab` = login on console, `/etc/rc.local` → `/usr/script/startup.sh` → `/vendor/run.sh`, `/oad/oadLoader`.
- `ap0/run.sh`: starts `telnetd` only if `$PWD/run_nfs.sh` exists AND `/conf/remote_nfs.txt`+`/conf/mac.txt` (run_nfs.sh NOT in Medion image). `run_aurora.sh`: if `/conf/remote.txt` = `isgdb:ip:user:pass` → fixed MAC, DHCP, CIFS `//ip/mb230cifs`, then `gdbserver <local_ip>:2345` = root RCE. `/conf/remote.txt` gate is the prime target.
- FROST TestTool TCP console in `aurora.elf`: binds **TCP 1986** (`movw r0,#0x7c2`), no auth, prompt `testtool > `, commands incl. `print_profile` (dumps SW+HW profile), `usboperations`, `ntkmenu`, `screen_capture_usb`, `debugout`, `list_all_thread`, `set_IP_address`, `SETDNS1`, `SETRDSERVERURL`, hundreds more. Gated by `is_testtool_socket_enabled`; 1986 refused on live set → needs Test Tool Mode enabled (service menu).
- Remote diagnostic command set in `app/remotediagnosticcommandhandler`: PRINTALLPROFILE, COPYCONFPART, GETLOGGINGFILE, GETSPIDATA, GETSERVICEMENU, INSTALLSERVICES, SETSDXDATA, UPDPANELFILES, SNTP/GTFTP/SSIP… served over `https://remotediag.vestek.com.tr/Diagnosticadmin/rdiagws.ashx?uuid=` (outbound, URL+port settable).
- `ICE_DiagnosticsEnableTelnet` / `EnableUartRx` / `EnableHttpTestServer` / `EnableUSBLogging` exist as diagnostics; strings `UART_RX_SETTING`, `Test Tool Mode : %s`, `Service Menu passcode`.
- Profiles: aurora handles `/<mnt>/<prefix>{hw,sw,dev,lang}profile.bin` (USB→flash "Update Profile") and `/DownloadProfile.mb230` (trigger file for USB export "Download Profile"). No network endpoint serves them.
- Live re-check 23:0x: 22/23/1986/8080 closed; 2870/7681/12345/26442/33907/56789/56790 open; 12345/26442/33907 accept TCP but never answer (tarpit/handshake unknown); SmartCenter POST of GETBOARDNAME/PRINTALLPROFILE/GETCAPS = 201 with empty body, no WS reply.

## Live profile export 2026-10-29 23:33 (service menu, USB Operations)
- `artifacts/profiles/`: `mb230_hwprofile.bin` (24514 B), `mb230_swprofile.bin` (60075 B), `mb230_langprofile.bin` (5.17 MB), `usb_trace_1029222624.log` (151 KB app log), plus 5 video-plane grabs.
- swprofile plaintext block: VESTEL TV / CEC / TELEFUNKEN / UAT / VESTEL / Movie Sense / Vestel Pixellence / `7706.dcf` / `0009df` / `s3-eu-west-1.amazonaws.com/vstlupdate` / `SERVER_HTTP_SECURE_TYPE` / `17MB230` / `vstl.1973`.
- So this set's OTA descriptor = `0009df/7706.dcf` (Medion was `772e.dcf`, same project id). Anonymous S3 GET of `0009df/7706.dcf` = 403 AccessDenied (both http/https, also bucket root) → still needs signed URL.
- hwprofile: MB230/MB200, 12Watt, Surround Sound, TIGroup7, `default.aqs.bin`, SingleTact, `RCA_43XX_RC5`, Si2157, VRc48XX. No VESxxx panel string in any exported profile → panel code still unknown.
- Trace log shows OTA check `http://s3-eu-west-1.amazonaws.com/vstlupdate/Essentials/2kb.txt` and `usb upd cnt=255, http upd cnt=2`; DVB tuning errors on HDMI1.
- Stick also got `N_GetScreen_ice_8.bmp` (1280x720x32, video plane) from the USB operation; not useful.

## Robotest / test tool status (live)
- `global_robotester_port = 12345`, `robotester_running = 1` → port 12345 = Robotest ("Observatory") server: plain line protocol, commands `version '1'`, `help 'protocol'`, `grab '<file>' '<plane>'`, `key '<name>'`, `listkeys`, `svnrevision`, `copyDirectory`, `pid`, `restartstreams`.
- BUT 12345 accepts then closes immediately, no banner, no reply to any line. Code at `RobotestServer::Connection::threadMain` (0xc018f8) builds an fd set by storing raw fd values and calls select(0x76ee8c); on failure it clears `robotester_running` and exits → connection closed. Effectively broken on this firmware; not a usable primitive.
- Service menu toggles confirmed present and enabled by owner: **Test Tool**, **HTTP test service**. Test Tool did NOT open 1986 (gate = profile `PROFILE_GetTesttoolSocketSupportEnabled` / `is_testtool_socket_enabled`, read at app start; also 1986 socket thread is the FROST console).
- 26442 / 33907: connect, then nothing (curl 000, raw line silence) = tarpits, not HTTP despite earlier notes.
- ICE HTTP test server (`ICE_DiagnosticsEnableHttpTestServer`) is not listening on any LAN port → loopback-only, only reachable through the TV browser SSRF if its paths were known.

## Bootloader / USB recovery channel (proven 2026-10-30)
- Holding OK on the remote while reconnecting power runs `kylo_usb_update.scr` from the FAT32 stick. U-Boot is 2016.07 for nt72, has `nvtload`, `nand read/write`, `sf read/write`, `ubi` commands, `start_progress_bar`/`stopprogressbar` + `update_img_idx`/`update_img_num` (on-screen progress = feedback channel), `fatwrite usb 0:1 <addr> <file> <size>` (standard order works; `usb 0:auto` also worked; vendor "file addr size" order failed and aborted the script).
- Confirmed working: `sf read 0x3300000 0x0 0x100` → 256 B dumped. Content is this TV's own xboot header: `4ccfBOOT`, `Jan 05 2021 11:10:00`, `v001`, `Rev.A`, `NT72673` (Medion image was 2019 → different build).
- Script must be <8 KB and end with `%` (U-Boot: "incorrect script format! place %% at the end of auto script").
- Full 512 MB NAND dump script (64 × 8 MB chunks) written to stick; read-only.

## Kernel facts (Medion 4.9.118 sibling kernel, decompressed to /tmp/opencode/ker0.img)
- uImage at file 0x540020, load/ep 0x2808000, gzip, 10.9 MB decompressed.
- **No initramfs/cpio support** (no "Unpacking initramfs", no cpio strings) and **no IP-Config/IP_PNP** → cannot boot a custom userspace from RAM.
- USB storage (`usb-storage.ko`, `usbcore.ko`, `xhci-hcd.ko`) and Ethernet (`ntkethmac.ko`) are **modules in /vendor/modules**, not built-in → a USB rootfs or network-before-userspace boot is impossible.
- Rootfs is glibc 2.23, ARM EABI5 hard-float (`/lib/ld-linux-armhf.so.3`) → static musl ARM binaries run fine.
- Toolchain ready: `/tmp/opencode/arm-linux-musleabihf-cross` (GCC 11.2.1), dropbear 2024.86 source at `/tmp/opencode/dropbear.tar.bz2`. mtd-utils (ubinize/mkfs.ubifs/ubiformat) installed on host.

## USB stick prepared
- `/dev/sdb` (8GB Generic Flash Disk) wiped → MBR + FAT32, label `TVHACK`, mounted `/mnt/tvusb`, contains empty `DownloadProfile.mb230` (export trigger) + README.

## Work log
1. Ping/ARP ok, nmap full scan → 7 ports + Vestel OUI + TELEFUNKEN-TV hostname.
2. HTTP banner grab → NFLC/3.0, 1989-epoch, 501, silent 33907, DIAL 404s.
3. SSDP/mDNS/UDP1900 negative; mDNS hostname resolves, no services.
4. Found DIAL docs (hass-vestel-tv) → fetched dd.xml (MB230/0.51.52.3) + SmartCenter 200 + WS 101 + WS tv_state + portal getportalurl (orion.portaltv.tv).
5. UART/service-menu research (4725, Home+4725, 0DRG WiDi injection, CVE-2026-15570 SSRF-only on MB181).
6. SSRF probe, corrected syntax → TV fetched attacker server, full UA captured. Loopback scan via JS → 8080 hidden; 33-path enum all 404. SmartTvA_API/oipf enumerated (opaque).
7. Sent remote keys MENU+4725 → 201s, no new ports. Cleaned with EXIT.
8. Rear-label photo → D65U546A confirmed. Firmware websearch → no public image. Downloaded MB230 service manual PDF + txt (SoC/flash/update confirmed). MITM capture empty. Exfil servers left on 8001-8006 (`/tmp/opencode/`).
9. OTA path check: Medion swprofile tokens are stored data, not derived from retail identifiers. S3 bucket `vstlupdate` is `eu-west-1` and anonymous GET is 403 for both real-looking and nonsense keys.
10. Token-from-TV check: description, `tvstate` (HTTP 201, empty body), and a 3s WebSocket read returned no profile. `mb230_swprofile.bin` for this set was not obtained.
11. SmartCenter code-injection check: closed. `201` does not mean the body ran. Confirmed effect is `browserseturl` browser fetch and JavaScript. No shell, file read, listener, or new debug port. CVE-2026-15570 is SSRF on MB181/TiVo.
12. Carved the Medion EFSL profiles out of the zip. Did not extract the 243 MB `kylo_usb_update.bin` itself.

## Open / next
- [ ] Get VESxxx panel code: service menu Product Info (needs physical remote: HDMI source, hold 0, Home, 4-7-2-5) or mainboard sticker photo. Blocks correct firmware request.
- [ ] This set's `mb230_swprofile.bin`: only via physical USB export (`DownloadProfile.mb230`, service menu, USB Operations). Medion tokens must not be reused as this set's path. Even the right object is TLS + signed + encrypted, so it is a worse RE artifact than the Medion Kylo USB image.
- [ ] Exact image: Vestel Germany / Telefunken support still needs D65U546A + 2199818 + SN 8368599 900143 + MB230_VESTEL 0.51.52.3 + VESxxx. Closest public same-family core is forum-gated `MB230_VESTEL_0.51.19.0` (uyducu, older than 0.51.52.3).
- [ ] Unpack the Medion Kylo image (RE only, do not flash). `mstar-bin-tool` does not apply. Script load base `0x3300000`. Rootfs is UBI at file `0x1280020` with three gzip squashfs 4.0 volumes (556 / 7429 / 298 inodes; main one `bytes_used` `0x92e1583` at file `0x18c2020`). Kernels are uImage Linux 4.9.118 at `0x540020` and `0xa20020`. Needs `ubi_reader` + `unsquashfs`. Header: `4ca4BOOT`, `NT72673`, board `nt72671b_a32_n33006_nand`, U-Boot 2016.07.
- [ ] Optional net: fuzz 127.0.0.1:8080 real paths via TV JS (POST/SOAP/WSDL variants), probe WS 7681 subprotocols, dump FritzBox host info via TR-064.
- [ ] Cleanup: `/tmp/opencode/exfil3.py` through `exfil6.py` were still running at save time (ports 8003-8006). `exfil.py` and `exfil2.py` were not. Kill with a separate `pkill -f` so the pattern does not match the shell itself. `artifacts/captures/tv_capture.pcap` is the empty MITM capture.
- SmartCenter reverse shell / remote execution: closed, do not pursue. No shell was demonstrated, and that work is out of scope.

## Artifacts (in repo: artifacts/)
- manuals/: MB230_manual.pdf + MB230.txt (service manual, SoC/flash/update)
- captures/: tv_capture.pcap (empty MITM attempt). Live `dd.xml` was not saved; the TV did not answer on port 56790 at save time.
- logs/: probe*.log (DIAL/SSRF hits incl. TV UA), exfil*.log (loopback 8080 + JS API)
- scripts/: probe_server.py, probe2.py, exfil*.py (SSRF browserseturl + JS exfil servers)
- firmware/: `MD30010_Firmware_Full_Update_DE.zip` (bin stays inside the zip). Extracted beside it: `kylo_usb_update.scr`, the German PDF, `medion-mb230_swprofile.efsl` (91825 bytes), `medion-mb230_hwprofile.efsl` (11752-byte prefix; header claims `0x9e40` but the next EFSL starts at `+0x2de8`), `medion-swprofile-ota-strings.txt`.
- tools/mstar-bin-tool/: unpack/pack/extract_keys (wrong format for this Kylo image; no --help)
- rear-label.jpg (D65U546A label photo)
- Originals also under /tmp/opencode/ (may vanish on reboot).
- Do NOT re-run full nmap -sV across all ports (hangs); use targeted socket checks.

## 2026-10-30 — dump + signature discovery + apdat0 plan
- Full 512 MB NAND dump in `artifacts/dumps/tv_nand.bin` (n00..n63, 8 MB each). Layout: secos 0x6c0000, U-Boot 0x980000 (`g6bd49a61`, 2021), 6 FDTs 0xac0000-0xc40000, ker0 0xd40000, ker1 0x1440000, UBI from 0x2040000 to end.
- Three UBI containers: `fs0` (+ap0/ap1/nvt_sign) at 0x2040000, `apdat0` at 0xe340000 (10 MB), `buffer` at 0xed40000.
- TV bootargs (from FDT): `console=ttyS0,115200 ... ubi.mtd=12 ubi.block=0,0 root=/dev/ubiblock0_0 rootfstype=squashfs ro quiet init=/init`.
- **fs0/ap0/ap1 ARE signature-verified** (`nvt_sign` volume; kernel: "UBI Security Verify volume Fail! ... goto shutdown", "Get fs0/ap0 signature fail") → do NOT patch them.
- `apdat0` is NOT verified and is mounted rw at `/conf`. UBIFS geometry: leb_size 253,952, leb_cnt 31 (7,872,512 B).
- `/conf/remote.txt` + `nvt.app.debug.ini` path: remote.txt makes appmgr use debug.ini, whose idtv program is `/mtd_apexe/board/run_aurora.sh`; debug.ini also sets `enableMultiuser=n` (apps likely run as root there). run_aurora.sh then mounts CIFS and runs `$REMOTELF/aurora.elf` (isgdb=0) or gdbserver (isgdb=1).
- Built `apdat0_full.ubifs` (original contents + `remote.txt` = `0:192.168.178.167:tvhack:tvhack123`), padded to 31 LEBs with 0xFF.
- Host: Samba share `//192.168.178.167/mb230cifs` (user tvhack / tvhack123, force user root) serving `aurora.elf` (boot hook shell script), static musl `dropbear` (313 KB), host keys, `authorized_keys`.
- SSH key for the TV: `~/.ssh/tvhack_tv` (ed25519); root password fallback `TvH4ck-2026`.
- Restore path on stick: `apdat0_orig.ubifs` + `restore_apdat0.scr.txt`.

## 2026-10-30 — SSH ROOT ON THE TV (working, persistent, no host dependency)
- `apdat0` (=/conf) written via U-Boot `ubi part apdat0` + `ubi write` (no-op test first: wrote original back, readback md5 matched, TV booted).
- Payload in apdat0:
  - `/remote.txt` = `0:127.0.0.1:x:x` → appmgr uses `nvt.app.debug.ini` (enableMultiuser=n ⇒ apps run as root), CIFS mount fails instantly, so `run_aurora.sh` executes the LOCAL `/conf/remotelf/aurora.elf`.
  - `/remotelf/aurora.elf` (hook): restores real MAC 70:54:B4:FE:70:C6 + udhcpc, mounts devpts, bind-mounts a patched `/etc/passwd` (root home=/tmp/tvh_root), installs authorized_keys, starts dropbear:22 + telnetd:2323, logs to /conf/tvhack.log, execs real `/vendor/com.nvt.idtv/aurora.elf`.
  - `/remotelf/dropbear` (static musl ARM 313 KB), host keys, authorized_keys.
- Login: `ssh -i ~/.ssh/tvhack_tv root@192.168.178.87` (key) — verified after full power cycle (uptime 34 s, key auth, no manual step). `telnet 192.168.178.87 2323` = unauthenticated root fallback.
- Gotcha fixed: dropbear `-c /bin/sh` is a FORCED-COMMAND flag (kills `ssh host cmd`); removed from hook. Home dir must be 700 (`umask 077; chmod 700 /tmp/tvh_root`) or pubkey auth is refused.
- Side effect: debug.ini disables netflix/amazon/youtube/dialserver → 56789/56790 down, those apps absent. 7681/2870/12345/26442/8080 still up. MAC/IP unchanged (70:54:b4:fe:70:c6 / .87).
- Restore: `restore_apdat0.scr.txt` (fast) or `restore_full_nand.scr.txt` on the stick; pristine 512 MB dump `artifacts/dumps/tv_nand.bin` md5 36de1801478ce38c8042845374add41d.

## 2026-10-30 final state (synced to artifacts)
- Live TV verified: `ssh root@192.168.178.87` with `~/.ssh/id_ed25519`, `~/.ssh/id_rsa`, `~/.ssh/tvhack_tv`; both new keys tested LOGIN_OK; survives reboot.
- `/conf/remotelf/aurora.elf` md5 `c8257b7ab0cf4e979a20b7da20cd5445`; `/conf/remotelf/authorized_keys` md5 `0d11cf71a7e3054cb1359d62f9033cee` — both identical to `artifacts/payload/`.
- `artifacts/payload/apdat0_full.ubifs` rebuilt from the live hook+keys and re-verified by extracting it back; md5 `a83d45fead862f8d96daf6be1dfe57d6` (padded to 31×253,952 = 7,872,512 B).
- `artifacts/payload/apdat0_orig.ubifs` md5 `fde74f196b11740de528735e49bcc976`.
- `artifacts/scripts/`: `nand_dump.scr` (7425 B), `noop_write_apdat0.scr`, `apply_apdat0_hook.scr`, `restore_apdat0.scr`, `restore_full_nand.scr` (7201 B) — all < 8 KB.
- `/conf/remote.txt` = `0:127.0.0.1:x:x`; CIFS share on the laptop is no longer used at boot (Samba can stay stopped).
- TV app list still shows Netflix/Amazon/YouTube in the launcher; dialserver (56789/56790) is off because `debug.ini` disables it.
- Open next steps: optional telnetd:2323 removal, ssh hardening, and possibly restarting appmgr with `nvt.app.ini` after the hook to restore DIAL without losing sshd.

## 2026-10-30 telemetry audit
- Full report: `artifacts/reports/TELEMETRY-2026-10-30.md`; raw `settingsDB.db` pulled to `/tmp/opencode/settingsDB.db`.
- Active: plog.vstlsrv.com (54.171.187.177), orion.portaltv.tv (52.209.152.10/34.242.168.133/34.245.121.149), search backend 7345023508.fxm9485766783.com (88.99.123.80, Hetzner), Samba TV 216.183.117.106 (ACR bootstrap only), S3 :80 OTA probe, Bunny CDN IPv6.
- ACR disabled (`sambatvCid.json enabled:false`, `/acr_enabled false`). Consent: marketing=1, 3rd-party=1, ads=1, samba=0.
- IDs: ad ID `71613941-14eb-49f5-a310-a3505c0867f8`, half-private user `u22162473093285252`, gracenote `49081922121839746-0688B3D8675C4E852C7B21CCD1FB43FE`.
- Not linked: Alexa/GA (`AlexaReadyTable.m_fti_flag=0`, no /conf/aws_iot_certs or ga_*), so the serial-number `user_identity` MQTT publish is inactive.
- Report includes a blocklist and three mitigation levels (TV settings, hook-level /etc/hosts or iptables, router DNS).

## 2026-10-30 privacy: advertising / 3rd-party sharing turned OFF
- Real consent lives in `settingsDB.Settings` (not the `Privacy_*_Last_Logged_Item` tables, which are derived logging state).
- Flipped to 0 and verified after reboot: `m_privacy_advertising_enabled`, `m_privacy_personalized_advertisi`, `m_privacy_marketing_and_profilin`, `m_privacy_3p_data_share_for_adv_`. Already 0: `m_samba_acr_enabled`, `m_privacy_marketing_rules_enable`; `m_do_not_track_enabled`=1.
- Procedure: edit DB offline (no sqlite3 on TV) → write `settingsDB.db.new` → atomic `mv` → chown idtv:idtv_group, chmod 666 → sync → `reboot -f` (bypasses app flush).
- Artifact: `artifacts/privacy/settingsDB.privacy-off.db` md5 `6f6916a6d00ba5a5d4b4c07898e51b3b`.
- `artifacts/payload/apdat0_full.ubifs` rebuilt with this DB (md5 `1cb6b0c7cd4bfa26c1d7163f1f9473c3`) so a re-flash keeps privacy off. The USB stick copy is stale until re-copied.
- Product telemetry (`plog`) and portal/appclicks are Vestel first-party and still enabled; ACR off. Network blocking not yet applied.

## 2026-10-30 cleanup + current TV state
- Repo trimmed 3.2 GB → 553 MB: removed `artifacts/firmware/` (Medion MD30010 zip + Kylo image + extracts), `artifacts/tools/mstar-bin-tool`, old SSRF probe scripts/logs/captures, and the regenerable dump extracts (`ubi_out/`, `tv_fs0/`, `tv_ap0/`, `n00..n63.bin`).
- Kept: `tv_nand.bin` (512 MB pristine) + `dumps/split_for_usb.sh` (recreates the 64 chunks on a stick), `payload/` (now with `hosts` + final settingsDB), `scripts/*.scr`, `privacy/`, `profiles/`, `reports/`, `manuals/`.
- `payload/apdat0_full.ubifs` rebuilt again with the corrected DB (internet=1, ads/marketing/3p/logging=0) — md5 `c4b09e81c773f932c076195279e3f400`.
- **TV is currently offline on purpose-danger**: `m_privacy_internet_connection_en=0` was written during the telemetry-off pass and the TV cuts its own network when it is 0. Must be 1. Fix on the TV (Settings → Privacy → Internet connection = ON) or re-flash `payload/apdat0_full.ubifs` (which now has 1).

## 2026-10-30 WoL + privacy reset
- WoL verified: magic packet to 70:54:b4:fe:70:c6 wakes the TV from quick standby (uptime
  preserved, app/sshd immediately available). `m_wake_on_settings=1`; ntkethmac.ko keeps PHY
  power in STR_OFF suspend. Tool: `tools/wol.py`.
- The TV was NOT network-disabled by the privacy flag — it was in standby (no ARP/ping while
  asleep, magic packet wakes it). After wake: internet works (ping 8.8.8.8 OK).
- Privacy flags were reset to defaults at some point (`m_privacy_internet_connection_en=1`,
  `m_privacy_advertising_enabled=1`) — the ads/3p-off state is no longer in effect; re-apply
  if wanted (keep internet=1 this time).
- Ports now: 22, 2323, 7681, 2870 open; 56789/56790 closed (debug.ini).

## 2026-10-30 correction: privacy "Internet connection" gates the network
- Owner re-enabled all privacy settings in the UI, which restored connectivity → confirms
  `m_privacy_internet_connection_en` = 0 really disables the TV's network (the flag is
  functional, not cosmetic). The "it was just standby" theory was wrong; standby was coincidental.
- Current live state (owner's choice): internet=1, advertising=1, personalized=1, marketing=1,
  3p_share=1 — i.e. the earlier privacy-off state was deliberately reverted.
- WoL still verified working in this state (magic packet wakes from quick standby, uptime preserved).
- If privacy-off is re-applied later: set only advertising/personalized/marketing/3p to 0 and
  LEAVE internet_connection = 1, then re-check connectivity (ping + portal) immediately.

## 2026-10-30 on-device sqlite3 + network self-heal
- Built static musl ARM sqlite3 3.54.1 (1.35 MB) → installed at `/conf/sqlite3` (also /buffer/sqlite3).
  /conf: 3.4 MB volume, 2.0 MB free after install; /buffer: 232 MB, 206 MB free.
- Hook now: forces `m_privacy_internet_connection_en=1` in `settingsDB.db` before exec'ing
  aurora, and runs a 15 s eth0 watchdog (ifconfig up + udhcpc) — kill switch `/conf/no_force_net`.
- Tested: set flag to 0 + `reboot -f` → TV returned online in 20 s, log "db internet flag forced: 1",
  DB reads 1. Privacy UI can no longer lock us out.
- `artifacts/payload/apdat0_full.ubifs` rebuilt with hook + sqlite3; md5 `2f5ef636fa6a76cdfb65f21f8c327654`.

## 2026-10-30 privacy re-applied (with self-heal in place)
- Set `m_privacy_advertising_enabled`, `m_privacy_personalized_advertisi`,
  `m_privacy_marketing_and_profilin`, `m_privacy_3p_data_share_for_adv_` = 0; left
  `m_privacy_internet_connection_en` = 1. Values persisted across a reboot (app did not rewrite).
- Verified egress after reboot: no plog.vstlsrv.com, no orion.portaltv.tv, no search backend,
  no google/amazon; hosts block (`/etc/hosts` bind) is effective.
- Samba TV (ACR/CID) ignored /etc/hosts, so `/vendor/iptables -I OUTPUT -d 216.183.117.0/24 -j REJECT`
  is added by the hook (0 packets hit after adding; only stale CLOSE_WAIT sockets remain).
- Remaining egress: AWS S3 `:80` connectivity probe (`3.5.x`) — deliberately left for OTA/connectivity.
- WoL re-confirmed: TV slept and woke in 2 s on magic packet.
- `artifacts/payload/apdat0_full.ubifs` rebuilt (hook + sqlite3 + iptables rule); md5 `dd3d05c7dedb053f9586f993d0ab8068`.

## 2026-10-30 standby behaviour + helpers
- TV suspends ~3 min after boot/resume because it sits on HDMI1 with **no signal**
  (platform no-signal timeout; DB flags m_no_signal_stdby / m_auto_poweroff_stdby are
  already 0, so the gate is the profile capability `Profile_GetNoSignalTimeoutEnabled`
  in StandbyController, not the user setting). Suspend = STR, PHY kept powered → WoL works.
- Helpers (nondestructive): `tools/ensure_awake.py` (checks 22/7681/2870, sends magic
  packet if asleep, waits for sshd) and `tools/tvssh.sh` (ensure_awake + `/usr/bin/ssh -F /dev/null`).
- Option 3 (not done): patch `NoSignalCapability`/`isNoSignalTimeoutEnabled` in the
  /vendor overlay to stop it sleeping permanently.

## 2026-10-30 DIAL restored, keymap + screenshot oracle
- DIAL was dead only because `nvt.app.debug.ini` disables dialserver. It runs fine
  standalone as root: `( cd /vendor && exec ./com.nvt.dial/com.nvt.dialserver ) &`
  added to the hook (kill switch `/conf/no_dial`). After reboot: `56789/56790` up,
  `dd.xml` 200, keys/SmartCenter commands accepted and executed.
- Verification without watching the TV: root can read the OSD framebuffers.
  `tools/tvshot.py` dumps `/dev/fb1` (launcher/menus, 1920x2160, 2 frames, RGBA) or
  `--layer 0` (full-screen apps) to PNG; all-zero layer = nothing drawn (normal on
  the no-signal source). Proof shots in `artifacts/protocol/shot-*.png`.
- Key codes are defined on the TV: `/vendor/vk_keymapping_db.xml` (Virtual Remote =
  what `<key code=…>` takes), `/vendor/ice_keymapping_db.xml` (internal ICE codes),
  `/vendor/dial_appname_to_prefixurl_list.xml` (DIAL app names). Corrections to the old
  cheatsheet: **1010 = KeyExit, 1037 = KeyExit2, 1056 = KeyAux/source**, and the arrows
  were missing: down 1019, up 1020, left 1021, right 1022 (OK = KeySelect 1053).
- WS `7681` pushes state once on connect then only on change, so a silent socket is
  normal; POST always answers 201 and proves nothing. Confirmed with screenshots:
  MENU(1048) opens the launcher, RIGHT(1022) moves focus to Netflix, MENU from the tool
  tile opens `Instellingen → Beeld`.
- `POST /apps/<name>` (YouTube/Netflix/…) returns 201 but starts nothing: those apps are
  the ones `debug.ini` disables, so DIAL app launch cannot be used to enter the portal.
- Report: `artifacts/reports/DIAL-KEYS-SCREEN-2026-10-30.md`.
- Payload image rebuilt from a **live `/conf` snapshot** (not `apdat0_orig`), so it matches
  what runs: `apdat0_full.ubifs` md5 `76b1464a4b70566b014a58ac39ef795b`, hook md5
  `2b1d4b7dc7acad8ca0e9b5ccaff03da7`, round-trip verified with `ubireader_extract_files`
  (`diff -r` clean), numeric owners preserved (101 = idtv, 0 = root), `.log` excluded.

## 2026-10-30 UI stack (how the screen is drawn)
- No X/Wayland/Qt: `aurora.elf` is a DirectFB 1.7 app (`system=fbdev`, gfxdriver `tdga`,
  wm `sawman`, IR via `libdirectfb_nvt_uart_ir`), config `/vendor/etc/mp/.directfbrc`
  (1920x1080 ARGB32, layer 0 = OSD_NORMAL1 stacking middle/upper, layer 1 = OSD_NORMAL2 lower,
  no-cursor, no-layers-clear). It holds fb0..fb4 + `/dev/mali0` + `/dev/dri/card0` open.
- Launcher/menus/dialogs are drawn into **fb1 (OSD_NORMAL2, 2x1080 frames)**; fb0
  (OSD_NORMAL1) is the app/video OSD and is all-zero (transparent) when idle; fb2 (1280x720,
  16bpp) looks unused; fb3/fb4 are hardware cursor planes.
- Look-and-feel is data: `/vendor2/dynamic_ui` (default.ui + telefunken18_{svg,img,font}
  `.bin` with `.txt` name/hash/offset indexes). Web apps use `libomi.so` + `/vendor/browser_dir`
  (omi_worker, no fb fd → aurora composites them), e.g. `html_applications/digital_ib`.
- Proved we can paint: wrote a magenta box into both fb1 frames with dd, captured it with
  `tools/tvshot.py`, then restored the saved bytes (read-back md5 identical). DFB keeps shadow
  state, so it will repaint/tear over stray pixels.
- Report: `artifacts/reports/UI-STACK-2026-10-30.md` (routes: re-skin assets / paint overlay /
  own stage in the boot hook / HTML UI via browserseturl / patch aurora + LD_PRELOAD).

## 2026-10-30 P0 browser probe (launcher feasibility)
- OMI browser = **Chromium 67** (`OPR/46 OMI/4.13.5.431`, `Model/Vestel-MB230 ... _TV_NT72671_2019`),
  1920x1080 at **devicePixelRatio 1.5 → 1280x720 CSS viewport**. 24/28 feature probe passes;
  missing only `aspect-ratio`, `inset`, `:is()`, touch events (OmarchyTV's compat.js covers the first two).
- Keys reach the page as DOM events: arrows kc 37-40, OK = Enter 13, digits = DigitN. MENU (1048) and
  EXIT (1037) never reach the page (the TV consumes them).
- Page can be served from the TV itself (busybox `httpd` + shell CGI, 127.0.0.1:8181) and same-origin
  `fetch('/cgi-bin/...')` works; a POST straight to DIAL 56789 from page JS fails (CORS TypeError),
  so the CGI must do the DIAL calls.
- Switching: **EXIT closes the browser and returns to the stock TV UI**; `browserseturl` re-opens the
  page. Repeatable. That is the Model A loop, no process games.
- Beware stale framebuffers: after EXIT, fb0 still shows the last page although it is closed. Verify
  with beacons/keys, not pixels.
- A page left open keeps the TV awake past the 3-min no-signal timeout (heartbeats every 20 s for >4 min).
- Input sources are still unresolved: no source key (1056 = tuner info), `m_current_source` is not live,
  and aurora's SmartCommand table (0x151578c) has `setcurrentchannel` but no `setsource`.
- Report: `artifacts/reports/P0-BROWSER-PROBE-2026-10-30.md`, shots in `artifacts/protocol/p0/`.

## 2026-10-30 P0b source switching (solved)
- `KeyAux` (1056) opens the native **Bronlijst**: TV, BACK AV, HDMI1, HDMI2, HDMI3, YPbPr,
  VGA/PC, Wireless Display, "Audio video delen" (9 rows). It opens with the **current source**
  highlighted and UP/DOWN **wrap** (no clamping).
- Verified: `1056, DOWN x3, OK` selected HDMI2; the OSD banner showed `HDMI2 | HDMI` (live signal).
  The list is transient and the OSD paints asynchronously — send the keys as a tight burst
  (~0.25 s), never screenshot in between, otherwise the arrows land in the channel list instead.
- HDMI2 on this set is the owner's laptop: it shows a picture only while that screen is unlocked,
  so "no signal" on HDMI2 is the laptop sleeping, not a failed switch. The banner is the oracle.
- `m_source_names` (BLOB in settingsDB) lists the supported sources; `m_current_source` is NOT live.
- No SmartCenter `setsource`; the FROST/test-tool list at 0x14e177b has GETSOURCE/GETHDMISTATUS/
  get_screen_capture but we have not found its socket — not needed, keys suffice.

## 2026-10-30 P1 mock launcher (Model A proven on the TV)
- `launcher/index.html` + `launcher/cgi-bin/act` (repo) deployed to `/buffer/launcher`, served by the
  TV's busybox `httpd -p 8181`; opened with `browserseturl` to `http://127.0.0.1:8181/`.
- All three tiles verified on screen: TV = EXIT(1037) → stock UI; Sources = 1037 + 1056 → native
  Bronlijst; Apps = 1037 + 1048 → stock portal. Full loop repeatable.
- Key gotcha: **the browser consumes TV-control keys** (1056 with the page in front just closes the
  page), so a CGI must send EXIT first and the real key ~1 s later.
- The Bronlijst times out in ~2 s → navigate in one tight burst; a screenshot in between kills it.
- bushybox httpd CGI must emit `Content-Type` + blank line or the client gets "error getting response".
- Framebuffer formats differ per layer: **fb0 = BGRA, fb1 = RGBA** — `tools/tvshot.py --layer 0`
  now swaps red/blue automatically (`--format` overrides).
- **Standby over the network**: `<command>Standby</command>` (capital S) via the SmartCenter POST suspends the TV
  (connection dies, ports close); `KeyPower` (1012) is ignored. `tools/tvssh.sh` wakes it (WoL).
- Host side: the TV is on the laptop's DP-1 (Vestel 65UHD_LCD_TV). Waking it needed
  `wtype -k Shift_L` with `WAYLAND_DISPLAY=wayland-1` after `hyprctl`'s Lua-only dispatch
  (`hyprctl dispatch dpms on` no longer works in Hyprland 0.56; DPMS was simply off).

## 2026-10-30 P2 boot integration (launcher at boot, with a lock-out guard)
- Hook (`/conf/remotelf/aurora.elf`) now: starts `httpd -p 8181 -h /buffer/launcher` when
  `/conf/launcher_enable` exists, runs an opener loop (waits for DIAL, `browserseturl` to
  `http://127.0.0.1:8181/?boot=N`, 15 attempts), and a respawner that restarts httpd if it dies.
- Success = `/conf/launcher_ok` written by the page via `/cgi-bin/act?a=boot-ok`; failure increments
  `/conf/launcher_fail`, and after **3 consecutive failed boots** the flag is deleted and httpd
  stopped. Verified live with a broken page (streak pre-seeded to 2): TV kept its stock UI.
- `tools/launcher.sh open|close|status` on the host; the flag is currently ENABLED on the TV.
- Attempt ~6 is normal: the browser only accepts browserseturl once the portal is up (~40-60 s).
- Still open: re-entry after EXIT (only tools/launcher.sh open or a reboot), and the payload image
  is stale again (hook changed) -> P5.

## 2026-10-30 P2b re-entry via a stock-portal tile
- The stock portal's app tiles are rows in `settingsDB.db` → `LauncherItem`: `m_item_type=2`,
  `m_parent_id` 1 = Home row / 0 = Apps list, `m_order_index` = position, `m_application_url` = what the
  tile opens, `m_application_thumb_url` = icon (TV downloads it to /buffer/downloaded_images/
  launcher_app_thumb<N>), `m_original_id` = app id (900 collides with a real app -> wrong caption; use
  900000).
- Added two rows (ids 202/203) with url `http://127.0.0.1:8181/` and icon `http://127.0.0.1:8181/icon.png`,
  rebooted: our icon is the **first tile** and pressing OK opens the launcher. Re-entry from the remote
  now works: MENU -> tile -> launcher; EXIT -> stock TV.
- Backup before the change: `/conf/sqlite_db/settingsDB.db.pre-tile`. Tool: `tools/portal_tile.sh
  add|remove|status`. Reboot required (no live refresh). Caption still says "amazon alexa" (cosmetic,
  label comes from a cache we have not found).
- Report: `artifacts/reports/P2b-PORTAL-TILE-2026-10-30.md`.

## 2026-10-30 P3 OmarchyTV ported and booting
- Repo: `launcher/omarchy/` = upstream `webos/app` (index.html, app.js, data.js, dhh.js, compat.js,
  omarchy_themes.json) + 2 wallpapers (last-horizon, tokyo-night) + CaskaydiaMono font (~1.3 MB).
- Port = `platform.js` (`OMARCHY_PLATFORM="vestel"`, `OMARCHY_VESTEL_APPS/INPUTS`, `OMARCHY_LAUNCH`
  -> `/cgi-bin/act`, boot-ok + alive heartbeat) + 2 hooks in `app.js` (launchApp intercept, app/input
  list override) + viewport meta `width=device-width`.
- Scaling: the app has `fitStage()` (min(innerWidth/1920, innerHeight/1080)) and a fixed 1920x1080
  `#stage`. Our viewport is 1280x720 CSS (DPR 1.5). `width=1920` in the meta -> innerWidth 1920 -> no
  scaling -> clipped; an extra body transform double-scales. Correct: `width=device-width`, no transform.
- Verified: boots into it (boot-ok on attempt 6), workspaces 1-5, TV tile = EXIT to stock, Sources tile =
  Bronlijst, stock-portal tile opens it. Hook now opens `/omarchy/`; the P1 mock stays at `/`.
- Payload rebuilt after the hook change: `76b1464a4b70566b014a58ac39ef795b` (hook `f6fa58f1...`).
- Report: `artifacts/reports/P3-OMARCHYTV-PORT-2026-10-30.md`.

## 2026-10-30 launcher weather location
- Upstream OmarchyTV hardcodes its author's town (Alessandria, IT) and its "Automatic (from IP)" entry
  only reset to that value. Port fix in `launcher/omarchy/`:
  - `platform.js` seeds `localStorage["omarchytv_cfg_2110"]` with `OMARCHY_VESTEL_WEATHER` (now
    **Heiloo, NL 52.60252/4.68815**) when the stored city is a known default (empty / Alessandria /
    our earlier Amsterdam guess); a user-chosen city is never overwritten.
  - "Automatic (from IP)" now really queries `ipwho.is`; city search geocodes any typed name via
    `geocoding-api.open-meteo.com` (the old preset list is gone).
- Gotcha: patching app.js to touch `state` before it is declared breaks init (blank panels) -- do
  config seeding in platform.js before app.js instead.

## 2026-10-30 system panel = the TV itself
- The laptop-backed PC panel is replaced by the TV's own stats: `launcher/cgi-bin/stats` (busybox shell
  CGI, no python on the TV) emits the JSON OmarchyTV expects (host/kernel/cpu/ram/deploy + temp/uptime/
  disk). CPU = delta on /proc/stat (loadavg fallback, clamped), RAM from MemTotal/MemAvailable, host
  from /conf/friendlyname.txt. `app.js` accepts a full URL in cfg.pcHost; platform.js points it at
  `http://127.0.0.1:8181/cgi-bin/stats`; label "tv · system"; AI panel hidden (upstream shows fake $).
- `tools/pcstatus.py` (host side) is kept as an optional alternative, unused by default: the laptop can
  be off, nothing in the launcher depends on it.
- Gotcha: seeding a partial `cfg.widgets` object in localStorage hides ALL panels -- merge with the
  defaults {term,weather,pc,ai,deploy}.

## 2026-10-30 24-hour clock
- Upstream "auto" clock = 24 h unless the UI language is English; this browser reports en-US, so it
  showed 12 h. platform.js now exposes OMARCHY_PREFERS_24H and app.js's use24h() consults it.
- The browser reports timeZone as a bare **"CET"** (not Europe/Amsterdam) -- measured on screen with a
  debug div: tz=CET lt=1:00:00 PM h24=false. Accept legacy zone names (CET/CEST/MET/EET/WET/GMT/UTC)
  plus Europe/Africa/Asia/Australia; America/* stays 12 h; last resort = look for am/pm in a 13:00 render.
- Result: bar clock shows 14:10. The app's own Clock format setting (auto/12/24) still overrides.
- Gotcha: this browser caches JS hard. After editing app.js/platform.js, bump the ?v=N on the script
  tags in index.html, else the old code keeps running (bit me twice).

## 2026-10-30 launcher polish
- `/cgi-bin/net.js` (CGI -> text/javascript) sets `window.OMARCHY_NET_NAME` ("eth0 · 192.168.178.87 up"),
  loaded before app.js; the terminal panel prints it instead of the hardcoded "Wi-Fi".
- Hidden in the injected CSS (platform.js): `[data-action=bt]`, `[data-action=batt]`, `#spacex-pill`
  (fake countdown), `#grokbot-companion` (no AI backend).
- The top-bar Wi-Fi glyph is still upstream's (tooltip "Wi-Fi & network"); the real interface is in the
  terminal panel.

## 2026-10-30 idle behaviour + Dutch UI
- Screensaver verified: saver/index.html loads in the iframe (the Aquarium renders on this SoC) and any
  key dismisses it. Idle test hooks: ?devsaver=<min> | ?devsaver=off | ?devoff=<min>.
- "Turn off after idle" now sends EXIT (hand the screen back) rather than <command>Standby</command>:
  Standby returns 201 but is ignored in several app states (also from the LAN), EXIT always works and
  the TV's own no-signal timer suspends it ~3 min later. Verified: asleep, then WoL woke it (uptime kept).
- Dutch UI: added the `nl` STRINGS block + nl day/month arrays (`vr 30 okt`), tile labels, and
  platform.js seeds cfg.lang="nl" unless the user picked a language. 24h clock now falls out of both the
  language rule and OMARCHY_PREFERS_24H.
- Gotcha: the STRINGS object is `de: {...}` without a trailing comma -- adding a block needs `},`.

## 2026-10-30 themes/wallpapers/intro + bar glyph
- All 28 themes got artwork in /buffer/launcher/omarchy/wallpapers (53 webp downscaled to 1920x1080 q82
  + 5 mp4, 4.6 MB). 15 upstream files needed a base64 decode (they are stored base64 in the repo) and the
  GitLab API rate-limits bulk downloads -> the fetcher falls back to GitHub raw and decodes UklGR... .
- Animated intro plays on home resume (01-eyes-wide.mp4); verified by frame diff (background changes,
  clock does not).
- Switching theme in the UI stores an explicit wallpaper path; restoring cfg.theme alone keeps the old
  image, so the ?theme= test hook also clears wallpaper + wallpaperIdx.
- Quirk: launcher URLs with more than one query parameter get mangled (?theme=last-horizon&fix=1 became
  theme "last-horizonfix") -- use a single param.
- Top bar: Wi-Fi arcs replaced by an Ethernet port glyph (we are wired). Test hooks: ?theme=<id>,
  ?devsaver=<min>|off, ?devoff=<min>.

## 2026-10-30 menu/workspace cleanup
- System menu rows that need webOS/AI services are filtered (`skipVestel` + OMARCHY_HIDE_UNSUPPORTED):
  Cinemarchy, Plug-ins, Ambilight.
- Workspace 4 repurposed from "pair a PC" to "Deze tv": string-table entries rewritten in all four
  languages, pair/simulate buttons removed from index.html with null guards in app.js (they were
  dereferenced unconditionally), GPU placeholder -> "-", status line without the "connected to" wording.
- Lesson: DOM patches after load get overwritten by renderAll() which re-applies the string table --
  change the table or the render code instead.
- Bar: Ethernet port glyph replaced the Wi-Fi arcs.

## 2026-10-30 P5 stick attempt (blocked on hardware)
- `artifacts/scripts/make_stick.sh [mountpoint]` added: copies apdat0_full.ubifs + apdat0_orig.ubifs,
  installs kylo_usb_update.scr (= apply_apdat0_hook.scr), the alternative scripts (restore_apdat0,
  noop_write, restore_full_nand, nand_dump, spi_dump), optional 64x8MB NAND chunks (SKIP_NAND=1 to skip),
  plus README.txt and md5sum.txt.
- The stick at /mnt/tvusb was empty and dropped off the USB bus during the first write (dmesg:
  "FAT-fs (sdb1): Directory bread(block ...) failed", then no USB block device). Stale mount removed with
  `umount -l`. Needs a physical reseat (and possibly fsck.fat / reformat) before retrying.

## 2026-10-30 dd-able apdat0 image (instead of the stick)
- /proc/mtd on the TV: mtd13 "apdat0" = 0x00A00000 (10 MB, 40 x 256 KB PEBs) at NAND offset 0x0E340000;
  mtd14 "buffer" = 0x11000000. Only apdat0 is unsigned, so it is the flashable target.
- `artifacts/payload/apdat0_mtd13.img` = raw dump of mtd13 from the live TV (contains the hook, sqlite3,
  launcher flags), md5 875804842f4bb6fd71dd2c6097080edd, 10,485,760 bytes, UBI# at 0 on all 40 PEBs.
- `make_nand_image.sh` merges it into tv_nand.bin -> 512 MB full image (only apdat0 changed; verified the
  neighbouring partitions are byte-identical to the pristine dump).
- `flash_image.sh` has two modes: `host <img> <target> [offset]` (dd into a file/device) and
  `tv <img> --yes` (copies to the TV, ubidetach 13, dd to /dev/mtd13, reboot) -- the TV mode is NOT yet
  exercised end to end.
- The pristine dump's apdat0 slice has no remotelf (stock) -> confirms the image is really our payload.

## 2026-10-30 suspend with the launcher (resume watcher)
- <command>Standby</command> is ignored on this build in every state tried (source, portal, page in front).
  The **power key (1012)** does suspend, including with the launcher page displayed.
- A page does not survive suspend: the browser drops it and the set wakes on its source. The hook now
  watches dmesg for "PM: suspend exit" (buffer cleared at boot), presses MENU, re-posts browserseturl and
  retries until /conf/launcher_alive (page heartbeat, 5 s) is newer. Typical: 3 failed attempts, 4th works.
- Idle ("Turn off after idle") now suspends directly (a=sleep in the CGI) instead of handing the screen
  back; tools/launcher.sh sleep does it on demand.
- Artifacts refreshed after the hook change: apdat0_full.ubifs 76b1464a4b70566b014a58ac39ef795b,
  apdat0_mtd13.img 875804842f4bb6fd71dd2c6097080edd, hook aurora.elf 2b1d4b7dc7acad8ca0e9b5ccaff03da7.

## 2026-10-30 native boot splash (negative result, useful)
- `launcher/bootsplash/splash.c` -> 17.7 KB static ARM binary drawing "OMARCHY TV / VESTEL MB230 /
  STARTING" on /dev/fb1 (both frames, 5x7 bitmap font, no DFB).
- It renders correctly when the UI is already up (verified), but run from the hook *before* aurora it logs
  "splash: drawn" while the layer reads back blank -- the OSD plane does not present content until the stock
  app/DirectFB initialises it (re-tested with a 6 s hold). Hook reverted to the plain exec.
- Consequence: any native overlay on the spare fb2 plane needs the plane enabled by us (ntkdriver plane
  params / display ioctls) -- that is the open question for "native overlay" (option C).
- Report: artifacts/reports/BOOTSPLASH-2026-10-30.md.

## 2026-10-30 OSD init path reversed (tdga ioctl protocol)
- The display is set up by libdirectfb_tdga.so (NVTGA_OSD_*), not by aurora itself; the fbdev system module
  only opens/mmap's /dev/fbX.
- Vendor ioctls (type 0xc8): 0x400cc883 = _IOW(12) buffer descriptor, 0x800cc884 = _IOR(12) acquire a free
  buffer, 0x4008c80a = _IOW(8) config, 0x4020c86b/0x8020c86c = _IOW/_IOR(32) region, 0x4004c865 and
  0x4004c871 = word settings; standard FBIOPAN_DISPLAY 0x4606 flips.
- Init: FBIOGET_VSCREENINFO -> layer{type 0x5f, xres, yres, format} -> for ARGB also 12-byte + 8-byte ioctls;
  buffers = 1|2|8 derived from line_length vs yres (that is the multi-frame mapping we see on fb1).
- Present: 132 (get buffer) -> draw -> 131 (hand back) -> 0x4606 pan. Raw fb writes without that handshake
  are not displayed -- which explains the failed pre-aurora splash.
- Next: live probe on fb2 (1280x720 16bpp) with 0x800cc884 to dump the descriptor layout.
- Report: artifacts/reports/OSD-INIT-RE-2026-10-30.md; Ghidra dump in /tmp/opencode/ghidra_tdga_osd.txt.

## 2026-10-30 how aurora initialises (Ghidra, 90k symbols)
- NovoSystemInitialiser::initialise() @0xf27a6c: 388 named calls constructing every service/manager
  (command processor, profile config + NVRAM + user prefs, remote/input/AV source navigator, tuner +
  per-operator scan controllers, DVB components, EIT/EPG, picture/sound/video, DVR/PVR, UI/OSD, portal +
  ApplicationManager, WiFi/IoT/Alexa/device-id, logging/diagnostics/testtool/hotel/burn-in, Samba ACR +
  Kochava attribution, launcher manager + DialController at the end).
- initialiseNovoSystem() @0xf29e70: 99 calls -> NovoPortalUrlManager, NovoLauncherManager (the portal tile
  system we edit), VestelMarketClient, GraceNote, GlobalSearchEngine, first-run settings (NVRAM/cable/
  satellite/video/tuner/sound), initialiseDvr, then initialiseBrowser() + NovoWebBrowserListeners
  (this is why browserseturl needs the boot to settle), video wall, virtual remote, internet check.
- DialController + getWebsocketServerInterface (ws 7681) are created in initialise(), which is why DIAL
  answers before the portal paints -- our opener uses it as a readiness probe.
- Reports: artifacts/reports/AURORA-INIT-2026-10-30.md (map), OSD-INIT-RE-2026-10-30.md (display ioctls).

## 2026-10-30 read files from the dump, not from the TV
- `artifacts/scripts/extract_from_dump.sh <vol>:<path>` carves mtd12 (fs0, 0x2040000 + 0xc300000) out of
  dumps/tv_nand.bin, ubireader-extracts the volumes (fs0=root, ap0=/vendor, ap1=/vendor2), unsquashfs'es the
  requested one into /tmp/opencode/rootfs/<vol> (cache) and copies the file out.
- Verified: vendor:/lib/dfb/directfb-1.7-1/gfxdrivers/libdirectfb_tdga.so from the dump == the copy pulled
  from the TV (md5 a770a7e5e7d33c2857e773a91c6c2654). No need to wake the set for libraries, assets, aurora,
  the kernel or secos.
- unsquashfs needs -no-xattrs here, otherwise the xattr errors make it exit non-zero under `set -e`.

## 2026-10-30 the OSD driver is a DRM driver (ntkndpdrm.ko)
- /dev/fb0..4 (OSD_NORMAL1/2/3 + cursors) are the fbdev emulation of `ntkndpdrm.ko`, a normal DRM/KMS driver
  (drm_gem_cma dumb buffers, ndp_drm_plane_create, osdp_plane_ops, ndp_osd_plane_page_flip/mode_set).
  The 0xc8 vendor ioctls used by libdirectfb_tdga.so are that driver's fbdev path.
- The fb ioctl handler `_ker_osd_drm_fb_Ioctl` is an UNDEFINED symbol in ntkndpdrm.ko/ntkndp_pq.ko and is
  absent from both kernel slots (ker0 2021, ker1 2019; kallsyms reconstructed with vmlinux-to-elf - 64k/66k
  symbols, they do have do_fb_ioctl/fb_ioctl). So it comes from a component not in our dump set.
- Because the driver is DRM, the documented KMS API is usable instead: CREATE_DUMB/MAP_DUMB + ADDFB +
  SETPLANE. `launcher/osdprobe/drmprobe.c` implements it (static ARM, no libdrm), validated read-only on a
  normal DRM host (amdgpu). Pending: run on the TV (needs awake) to enumerate the OSD planes and try a paint.

## 2026-10-30 source handling + CEC (reverse engineered)
- Source list is owned by ICE: `ICE_ExternalNumberOfInterfaces()` / `ICE_ExternalGetName(i)` (13 descriptors,
  0x38 bytes each, name pointer at +0x2c). Names in .rodata: TV, BACK AV, HDMI1-4, S-VIDEO, SIDE AV, EXT1-S,
  YPbPr, VGA/PC, Wireless Display. `SRC_CFG_SourceSupportTable` (0x1770104, 22 x {id, enabled}) enables
  0,1,2,3,4,10,14,15,16,19,20.
- `ExternalAVSourceManager::selectExtSource(idx, secondary)`: per-window indices at this+0x34/0x38, video
  device via ICE_ExternalGetVideoDevice, audio separately through AudioIF (with a separate AD path), ARC via
  setHDMIAudioReturnChannel, SCART via muteScartOut; publishes state + video events. InputManager keeps the
  UI side and its ~2 s list timeout is `InputManager::timeout`.
- CEC: CECInterface (FROST thread, ICE_CECInit/Open/SendData, read() parses frames into CECMessage and
  publishCECMessage -> CECMessagePublisher listeners), NovoCECController (logical address/device type, audio
  status, RCP passthrough, getCECEnabled), MCU flags _HW_MCU_SetCECEnableOnOff/SetCECAutoPwrOnOff,
  ICE_LowPowerSetCECEnable/OneTouchPlay, profile Profile_IsCEC*.
- CECKeyMapper::m_key_mapping (0x152f8a8, 50 pairs) = ICE key -> CEC user-control code (512..521 -> 0x20..0x29
  digits, 522..525 -> 0x01..0x04 arrows, 526 -> 0x00 select, 528/603 -> 0x0D exit, 527 -> 0x0B back, ...).
- A CEC device shows up as its HDMI port, never as a separate source entry; ACTIVE_SOURCE handling lives in
  ICE (only debug strings in aurora).

## 2026-10-30 CEC renames inputs (learned + cached, not one-shot)
- Flow: `Report Physical Address (0x84)` on hotplug -> `setDevicePhysicalAddress` + `setDeviceType` ->
  `NovoCECController::getHDMIName(LA)`; if no stored name it sends `Get OSD Name (0x46)` to that LA.
- The device's `Set OSD Name (0x47)` is truncated to 14 bytes and stored in the CEC device table
  (`NovoCECController`, stride 0x128, field +0x13c = UString), mirrored onto the peer LA that shares the
  same physical address (+0x150 link from setDevicePhysicalAddress). `isMessageValid` rejects a BROADCAST
  0x47 (dest 0xF) - only a directed reply is accepted.
- `InputManager::getExternalInterfaceName` (0x00fb09d4) priority: CEC name (connector type 7 = HDMI only)
  > `UserPreferences::getSourceName` (m_source_names, persisted, 20 slots x 0x14 at pref+0x7700) >
  factory `NovoSystemInformationProvider::getHdmi*Name`. So an advertised name beats a hand-made label.
- RAM only: `clearDevice` clears +0x13c and re-pushes the default label with `CIDSetHDMIisCEC(false)`.
  After reboot the input is "HDMI 1" again until the device is re-detected. Nothing is written to settingsDB.
- `CIDSetInputName` -> `ICE_CIDSetInputName` is NOT the OSD label: it fills a global 0x80-byte buffer and
  calls `PLAT_SAMBA_SetVideoInputType/Number/Name` (CID/ACR layer).
- PA->input table (.rodata 0x152fa4c): 0x1000->group 3 (HDMI1), 0x2000->4 (HDMI2), 0x3000->9 (HDMI3),
  0x4000->12 (HDMI4), 0x5000->13.
- Separate and unrelated: `_ice_cec_Handler` (0x008067bc) has an LA-4 state machine (state 0: 0x47 from LA 4,
  state 1: Report Power Status ON from LA 4) that broadcasts `Request Active Source (0x85)`. State starts
  0xff and the arm flag is .bss, so it is a dormant test-tool injector; ICE_CECSendData sets state 1 when the
  TV sends `Give Physical Address (0x83)`.

## 2026-10-30 the remote is a UART, not evdev (native-shell input is viable)
- Path: IR receiver -> MCU -> /dev/ttyS2 (115200 8N1 raw, 9-byte frames) -> aurora (_micom_receive,
  DEV_Uart_GetKey*, DEV_UART_IR_*) -> DEV_UART_SendDFBKey -> /tmp/myfifo (20-byte records) ->
  libdirectfb_nvt_uart_ir.so (Dev_Uart_GetKey4) -> dfb_input_dispatch -> DFB -> UI.
- termios exactly: c_iflag=0, c_oflag=0, c_cflag=0x18b2 (CS8|CREAD|CLOCAL|B115200, 1 stop, no parity),
  c_lflag=0, VMIN=1, VTIME=0, tcflush(TCIFLUSH), tcsetattr(TCSANOW). open(O_RDWR|O_NOCTTY).
- Frame: FF FF TT A1 A2 CC ~CC B7 B8. TT=0x50 => key event; TT=0x5E => other (raw A1 kept, repeat flag
  cleared). B7/B8 unknown (repeat/long-press?).
- Remote = NEC extended address 0x66 0x11. nec_keycode_map (.rodata 0x1800, 52 x u32) = packed frames,
  ALL satisfy cmd^~cmd==0xFF. custom_keycode_map (0x18d0, 52) = vendor key symbols: 0x30-0x39 digits,
  0x0d OK, 0xf000-0xf105 others. Two decoders exist: Dev_Uart_GetKey (matches A1 vs ss_keycode_map 0x19a0)
  and Dev_Uart_GetKey3 (matches the whole 4-byte NEC code) - confirm which the current MCU uses by dumping.
- Key-injection paths (vendor test hooks): /mtd_apdat/irsim (=/conf/irsim) FF A B (A^B) stream;
  /tmp/kp_fifo 12-byte {status,?,u32 code,u32 type} type 4=press 1=release 8=repeat; /tmp/myfifo 20-byte.
- For the native shell: ttyS2 is exclusive to whoever runs; aurora owns it today. Our shell reads it while
  it owns the screen and MUST close the fd before exec'ing aurora.
- Tool: launcher/osdprobe/irkey (static ARM) decodes and namesthe buttons.

## 2026-10-30 CEC spec (HDMI 1.3a Supplement 1) vs aurora
- Normative doc is public: HDMI 1.3a Supplement 1 CEC (97 pp). Local text at /tmp/opencode/cec13a.txt.
- Bus: ~2.4 ms/bit, 10-bit blocks (8 data + EOM + ACK), header = initiator<<4|destination, dest 15 =
  broadcast, max 16 blocks, up to 5 retransmissions, wired-AND arbitration.
- Addressing: PA is the HDMI tree (1.0.0.0 = HDMI1, 2.0.0.0 = HDMI2...), TV is 0.0.0.0 and the only legal
  holder of LA 0. On a new PA a device shall allocate an LA and broadcast <Report Physical Address>.
- Device OSD Name Transfer (CEC 13.11): TV may send directed <Give OSD Name> (0x46) whenever it discovers a
  device; the device "shall" answer <Set OSD Name> (0x47) if it supports the feature; [OSD Name] = 1..14
  ASCII; both messages are DIRECTLY ADDRESSED ONLY; follower must "store the name and use it in any menus
  associated with that device". Persistence is not required (aurora keeps it in RAM).
- aurora conforms: 14-byte clamp, broadcast 0x47 rejected, per-LA store, name preferred over user/factory.
  The LA-4 -> <Request Active Source> machine is a legal use of 0x85.
- Steam Deck: if it doesn't implement OSD Name Transfer it will never be renamed, and there is NO EDID
  fallback - HDMI inputs come from ntkndphdmi.ko and never appear as DRM connectors (/sys/class/drm only has
  card0-LVDS-1, card1-LVDS-2).

## 2026-10-30 CEC: spec vs aurora, message by message
- Two layers: ICE (`ice_cec.c` - framing, validation, CEC Version + Give/Report Physical Address replies,
  workarounds) and NovoCECController (policy: device table, ARC, audio mode, key map, UI). "Missing in aurora"
  usually means "handled in ICE" - e.g. 0x9E/0x9F exist only as ICE strings.
- TV sends: 0x00 Feature Abort, 0x32 Set Menu Language (broadcast), 0x46 Give OSD Name, 0x70, 0x71, 0x7D,
  0x82 Active Source (PA 0x0000 when the TV takes the screen), 0x83, 0x84, 0x85, 0x8F, 0x90, 0xC1/0xC2/0xC3.
- TV accepts: 0x00, 0x04, 0x0D, 0x36, 0x44, 0x45, 0x47, 0x71, 0x72, 0x7A, 0x7E, 0x80, 0x82, 0x83, 0x84,
  0x85, 0x8F, 0x90, 0x91, 0x9D, 0xA3, 0xC0, 0xC5. Absent: 0x8D/0x8E (Device Menu Control - optional),
  0x64 Set OSD String, 0x89/0xA0 vendor, recorder/tuner/timer/deck sets.
- Strictness beyond the spec: operand lengths checked on every branch (0x80=4, 0x82=2, 0x84=3, 0x90=1),
  never replies to a broadcast, Feature Abort with reason 0 (unrecognised) / 1 (not in correct mode), per-device
  reasons via setFeatureAbortReason. Buffers Active Source while powering up (isActiveSourceMessageReceived).
- Three non-spec workarounds worth remembering: LA-4 0x47/0x90 -> Request Active Source; `==WA== Device reply
  address` rewrites destinations of 0xC3/0xA4/0x70 and suppresses 0xC1 after 0xC0; ForceAMPInitial retries the
  amplifier handshake.

## 2026-10-30 openaurora started (modular aurora replacement)
- Location: `openaurora/` (GNU make, `make` / `make check`). Target order: (1) perfect aurora
  replacement, module by module; (2) native OmarchyTV UI on top. Tracked in TODO.md P8/P9.
- Safety model: one event loop is the only place module code runs, so dlclose happens only between
  dispatches. Service lookups are hold-tracked (a provider cannot be unloaded while held); every
  registration is owned by the module and swept by the host on unload; state handoff via
  save()/restore() for reload. Modules never link host symbols - only the API table (a test caught
  exactly that: mod_ticker referenced oa_now_ms and failed to load).
- ABI: include/openaurora/module.h - magic+abi+size, requires[], init/start/suspend/resume/stop/
  destroy, save/restore. Events in events.h, service ifaces in services.h. Single export
  `oa_module_entry()`.
- Working: core (loop/registry/events/timers/watches/post/config), control plane over unix socket +
  stdin (list/status/load/start/stop/unload/reload/suspend/resume/loglevel/quit), mod_ir (real ttyS2
  protocol, 52-key table), mod_ticker (clock service), mod_ui_console (UI lifecycle demo, selection
  survives reload). `make check`: 12 assertions green.
- Open: display/source/cec/settings/power/panel/audio/pq/apps/services modules; cross-compile choice
  (musl dynamic loader vs TV glibc 2.23).

## 2026-10-30 aurora census (Ghidra) - parity scope
- 84,547 functions. Script /tmp/opencode/ghidra_scripts/AuroraCensus.java -> /tmp/opencode/aurora_census.txt;
  report artifacts/reports/OPENAURORA-PARITY-2026-10-30.md.
- Biggest groups: EASIControlTask 1568, ICE 1460, EASIControlInterface 1113, NEBULA 1010, DemoApp 895,
  UserPreferences 820, Profile 692, UIFrameworkControlInterface 530, ProfileConfiguration 461,
  NovoLoggingManager 314, NovoController 295, UIS/TunerManager 193, StandbyController 196,
  MHEGClient 167, CIHost 180, INCA 180, OperaEngine 162, BluetoothController 110, NovoAVSClient 161.
- Hardware boundary by call volume: DEV_GPIO 175, DEV_NIM 116/76fns, DEV_AMP 82, DEV_PANEL 57,
  DEV_SPI 24, DEV_IIC 22, DEV_TCON 16, DEV_UART 12 + DEV_Uart 11, DEV_MTD 11, DEV_SRC 7, DEV_PWM 6;
  2,7xx distinct ICE_* callees.
- Keyword volume: dlna 11557, hdcp 5123, tuner 4531, teletext 4128, pvr 3494, epg 2298, picture 2259,
  bluetooth 2176, ci+ 1788, ota 1003, netflix 916, alexa 455, hotel 421.
- MISSED by the first plan: teletext/MHEG/FreeviewPlay (MHEGClient, fvp::FreeviewPlay, TeletextIF),
  CI+/CA (CHIL, INCA, CAStackServer/Controller, CIStackProxy, ConditionalAccessIF ~850 fns), DVB-S
  (SatelliteFastScanServiceFinder), PVR stack, Bluetooth, Alexa AVS, NEBULA portal, hotel mode, FRC.
- Consequence: "perfect replacement" redefined as tiers - T1 display/ir/power/panel/audio/source/
  settings/services (required), T2 cec/apps/pq/network/bt/teletext/mheg/tuner/epg/pvr/ci (only if used),
  T3 portal/voice/hotel/search/ota (probably never). If the set stays an HDMI monitor, descope T2's
  broadcast stack explicitly rather than half-build it.

## 2026-10-30 vendor HAL: link, don't rewrite (openaurora T1)
- ap0/lib (mounted /vendor/lib) ships UNSTRIPPED vendor libs. libNDAL3.so 388KB/4132 syms exports 121
  Hal_* entries: Hal_Pwm_{Init,Open,Set_ByChannel,Close,Terminate}, Hal_Gpio_{Read,Write,Config,Init,
  PinctrlRead/Write,SetDriving}, Hal_Audio_* (incl. Hal_Audio_Ctrl), Hal_Video_*. libNDAL_EX.so 2332
  syms; libWasabi.so 74k syms (stock UI framework); libomi.so/ffmpeg/mali are stripped (unusable at
  symbol level).
- Backlight chain: DEV_PANEL_SetPWMBacklightLv(0..1000) -> DEV_PANEL_SetPWMBacklightLevel -> DEV_PWM_Set_
  ByChannel -> plat_set_dimming -> Hal_Pwm_Init(); Hal_Pwm_Open({ch,freq,duty,0,1});
  Hal_Pwm_Set_ByChannel(ch,{freq,duty 0..4095,shift}). Channel/freq come from a per-model table we have
  not found -> config-driven (panel.channel, panel.freq) and calibrate on the set.
- Amp: DEV_AMP_* are vtable dispatchers (handle+0x80 etc.) into a chip driver object created by
  DEV_AMP_FACT_Create; this set has an AD82586C (I2C, RAM/reg dumps, EQ, subwoofer). Do NOT reimplement -
  Hal_Audio_Ctrl comes from libNDAL3. Explains ForceAMPInitial retries.
- Rule: modules dlopen the HAL (not link) so they still build/load on the laptop and degrade with a
  warning. Never call the HAL while aurora owns the same hardware (no refcounting); init order
  Init->Open->Set matters; HAL is tied to firmware 0.51.52.3.
- Implemented: modules/panel (backlight service, save/restore percent, suspend/resume), tested degraded;
  ARM cross-build of all 4 modules + core verified.

## 2026-10-30 openaurora: link the vendor HAL (toolchain constraint)
- Vendor libs are glibc 2.23 (ld-2.23.so in the TV rootfs), so the TV build MUST be glibc
  (arm-linux-gnueabihf). musl cannot dlopen a glibc .so, and linking one into a musl module fails on
  system@GLIBC_2.4 / pthread_setcancelstate@GLIBC_2.4.
- Makefile targets: `make` (host, -DOA_NO_VENDOR stubs), `make arm` (musl, stubs, sanity), `make tv
  TV_CROSS=<glibc gcc>` (links libNDAL3.so with -l:libNDAL3.so and -Wl,-rpath,/vendor/lib). `make vendor`
  runs scripts/fetch-vendor.sh which carves libNDAL3.so + libNDAL_EX.so out of dumps/tv_nand.bin via
  artifacts/scripts/extract_from_dump.sh (note: that script writes FLAT to <out>/<basename>).
- Toolchain hunt so far: Linaro 6.1.1-2016.08 URLs are dead (301 -> linaro.org/contact); Ubuntu
  old-releases cross-toolchain-base pool has 2.33+ (2.34+ breaks via __libc_start_main@GLIBC_2.34, so use
  <=2.33); the TV rootfs has only runtime libc.so.6 - no crt1.o/linker script, unusable as sysroot.
  Remaining options: prebuilt glibc<=2.33 armhf, crosstool-ng build, or implement PWM/GPIO ourselves.

## 2026-10-30 correction: the TV is glibc 2.23, NOT musl
- aurora.elf INTERP = /lib/ld-linux-armhf.so.3; vendor libs NEEDED libc.so.6/libpthread.so.0/libdl.so.2/
  libm.so.6/ld-linux-armhf.so.3; rootfs has libc-2.23.so, ld-2.23.so, libpthread-2.23.so; no musl anywhere.
- Live proof: /proc/<aurora>/maps shows /lib/ld-2.23.so + /lib/libc-2.23.so.
- The musl confusion comes from OUR tooling: dropbear/irkey/wspoll/drmprobe are static musl builds made with
  /tmp/opencode/arm-linux-musleabihf-cross. Static musl tools are fine (no dlopen); a musl openaurora could
  not link/load the glibc vendor HAL.
- So the TV target needs arm-linux-gnueabihf (glibc); keep glibc <=2.33 because 2.34+ requires
  __libc_start_main@GLIBC_2.34. Verify a candidate toolchain with `readelf -V` (max symbol version must be
  <= GLIBC_2.23) and `readelf -l` (interpreter should be ld-linux-armhf.so.3).

## 2026-10-30 openaurora TV toolchain solved + vendor HAL proven
- Toolchain: Arm GNU Toolchain 10.2-2020.11 arm-none-linux-gnueabihf (glibc 2.31), downloaded from
  developer.arm.com (the 10.3-2021.10 URL 404s, 10.2-2020.11 works, ~104MB, extracts to
  /tmp/opencode/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf). glibc 2.31 is fine; 2.34+ is not
  (__libc_start_main@GLIBC_2.34). Check with `readelf -V` -> max must be <= GLIBC_2.23.
- Gotcha: fcntl() is versioned GLIBC_2.28 -> replaced with pipe2(O_NONBLOCK|O_CLOEXEC) and
  socket(AF_UNIX, SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC). Core now maxes at GLIBC_2.17, modules at 2.4.
- Verification without the TV: qemu-arm-static -L /tmp/opencode/rootfs/root build-tv/openaurora ... ; with
  LD_LIBRARY_PATH=vendor/lib the panel module loads against libNDAL3 and the HAL prints
  "Can not open /dev/nvt/ntkled" -> the PWM path is reached (device nodes absent on a laptop).
- Vendor link set: libNDAL3 + libNDAL_EX + libasound.so.2 + libmhash.so.2 + libnteec.so (transitive
  closure). libNDAL3 needs DATA symbols from libNDAL_EX (gs32Ndp_PQ_fd) and imports the vendor app
  supplies (v4l2_open) -> modules dlopen with RTLD_LAZY; data relocs stay eager so NDAL_EX must be linked.
  RPATH /vendor/lib:/vendor/lib/libs (ALSA lives in libs/).
- Makefile: VENDOR_MODULES=panel only; `make tv TV_CROSS=...`; fetch-vendor.sh is path-aware
  (name=volume:path) and fetches the 5 libs from dumps/tv_nand.bin.

## 2026-10-30 openaurora T1 plans (Ghidra)
- Reusable vendor surface is ONLY Hal_* (libNDAL3, 643 fns, 18 families) + NDAL_* (libNDAL_EX, 160).
  PLAT_*/DEV_*/ICE_* are statically linked inside aurora.elf and unreachable from outside.
- Hal families: Video 124 (incl. Hal_Video_Libdrm_drmModeSetPlane/drmModeRmFB/Hal_Video_Drm_Set_Property),
  Demux 116, OS 108, PQ 97, Audio 60, VideoDec 26, Vin 22 (Hal_Vin_SetStreamStart/SetPcMode/Close =
  source switching), Gpio 20, Pwm 12, SPI 12, Debug 12, IfCmd 10, Userdata 6, Iic 6, Menu 6.
- power recipe (ice_hardware_SuspendToRam @0x752f70): CI park -> IR disable -> NVRAM flush ->
  MCU wake time -> sync() -> open("/sys/power/state") -> write("mem",3) -> on return NVRAM init,
  DEV_Uart_ResumeClean, IR enable, ice_SendKeyToMw(0x21f). Recommended first module (no RE left).
- audio: DEV_AMP_Init does Hal_Audio_Ctrl(0x3a /*IISMCLK*/,{0x100}) then task "DEV_AMP_ID"; amp is an
  AD82586C behind a vtable object from DEV_AMP_FACT_Create; PLAT_AIN_* also calls Hal_Audio_Ctrl.
  Next: extract the opcode immediates at all 57 Hal_Audio_Ctrl call sites = the audio API.
- settings: settingsDB.db has 66 tables; UserPreferences 821 fns, Profile 692, ProfileConfiguration 461,
  NvramStorage 94.
- report: artifacts/reports/OPENAURORA-T1-PLANS-2026-10-30.md (per-module plan + gates + TV measurements).
- Hal_Audio_Ctrl opcode table extracted (37 opcodes, 56 call sites) - see the T1 plans report:
  0x11/0x1e/0x40 AIN init, 0x12/0x13 SIF, 0x16/0x37 balance, 0x1b/0x1f/0x20 audio-description,
  0x22 A/V sync master, 0x2a AIN connect (routing), 0x2c/0x2d speaker DAC gain, 0x2e/0x3e volume table,
  0x2f SPDIF type, 0x30 SPDIF copy protection, 0x32 input gain, 0x33 MAIN VOLUME, 0x35 SPEAKER HW MUTE,
  0x36 DAC inverse, 0x3a I2S MCLK (amp), 0x3c/0x3d speaker main/mix gain, 0x41/0x46 unclassified.
  mod_audio needs init+routing+0x33+0x35+0x3a. Unknown: per-opcode parameter struct layouts.

## 2026-10-30 ON-TV display measurements (decisive)
- DRM /dev/dri/card0 driver "ndp" (Novatek DRM Driver): 1 CRTC with mode_valid=0 fb=0 -> KMS is NOT the
  scanout path. 8 planes: 0/1 unused (NV21..), 2 live video (fb=63), 3..7 OSD XR24/AR24 (fb=111/108/112/72/74).
  Dumb buffers unusable: CREATE_DUMB leaves size=0, MAP_DUMB offset + mmap = EINVAL, PRIME dmabuf mmap =
  EINVAL, SET_MASTER = EINVAL.
- fbdev nodes: fb0 OSD_NORMAL1 1920x3240 32bpp, fb1 OSD_NORMAL2 1920x2160 32bpp, fb2 OSD_NORMAL3 1280x1440
  16bpp, fb3/4 cursors. Painting fb2 (band and full frame, both virtual frames) writes correct pixels
  (1f f8 = magenta 565) but is INVISIBLE on the panel even with the layer live -> needs the driver handshake.
- tdga: region read (0x8020c86c) works on all fbs; acquire (0x800cc884) and commit (0x400cc883) return
  EFAULT for every descriptor layout tried ({0,0,0},{ptr,bytes,0},{0,ptr,bytes},{bytes,ptr,0},{ptr,0,bytes},
  {ptr,bytes,1}) with a 4K-aligned buffer -> descriptor is not a plain user pointer (physical/handle?).
- Conclusion: mod_display should use DirectFB (libdirectfb-1.7.so.1, 7812 syms + libdirectfb_tdga.so) as
  aurora does, not KMS. Recorded in OSD-INIT-RE-2026-10-30.md addendum and TODO P8.
- Tooling bug found: cd.size is __u64 but was printed with %u -> ARM vararg shift -> SIGSEGV (vendor
  coredump handler prints "Killed", dmesg shows [COREDUMP] SIGNR:11). drmprobe fixed (+size derivation,
  +PRIME, +master/clear, unbuffered output).

## 2026-10-30 ON-TV power test: suspend + WoL verified
- /sys/power/state = "freeze mem". `echo mem > /sys/power/state` really suspends: ping stops, dmesg shows
  Freezing user space -> PM: suspend of devices complete (1775ms) -> PM: resume of devices complete (1351ms)
  -> PM: suspend exit. Uptime keeps counting (suspend-to-RAM, not a reboot).
- A magic packet (our ensure_awake/WoL) wakes it; the hook's resume watcher fired ("resume: detected") and the
  launcher heartbeat was 1s old within seconds. No MCU wake timer needed for the WoL path.
- So mod_power = write "mem" + module suspend/resume ordering; IR wake and the PLAT_SYS_SetMcuWakeUpTime
  equivalent remain optional refinements.
- Observation: after several relaunches there were 3 omi_worker processes (2958/3026/3227) - the opener may be
  spawning extra browser instances; worth a look when touching the launcher.

## 2026-10-30 CORRECTION: raw fb1 writes DO work (display path found)
- With the launcher closed, a plain mmap+write to /dev/fb1 (OSD_NORMAL2, 1920x1080, 2 buffers, RGBA)
  IS visible on the panel (magenta band confirmed visually; readback ff 00 ff ff). FBIOPAN_DISPLAY works
  (returns 0, yoffset readback 1080) -> double buffering available, no tdga acquire/commit needed.
- fb2 (OSD_NORMAL3 1280x720 16bpp) is NEVER visible -> that plane is disabled on this model. Do not use it.
- Nuance: fb1 before aurora/DFB initialises the layer is still invisible (that was the original boot-splash
  result). So mod_display = mmap fb1 + pan, but the open question is whether we can bring fb1 up before
  aurora has ever run, or must replicate a one-time InitLayer.
- Anomaly: panning back to frame 0 was accepted (rc 0) yet yoffset read back 1080 - the flip did not take.
- Also seen: omi_worker accumulates (3 processes) across launcher relaunches.

## 2026-10-30 CORRECTION: boot-time fb paint WORKS (quarter-size)
- One-shot hook test (fb1 mmap + fill, before `exec aurora`) was VISIBLE at boot - so the OSD plane is
  scannable from the hook after all. Supersedes the old "boot splash impossible" conclusion.
- It only covered the TOP-LEFT QUARTER: the OSD region is 3840x2160 (4K canvas) while /dev/fb1 is
  1920x1080. Runtime region read: {0,0,1920,1080} current | {0,0,3840,2160} max. aurora sets the region;
  before aurora runs it is the 4K default.
- To fill the screen: write region via ioctl 0x4020c86b with 32-byte {x,y,w,h,?,?,maxw,maxh} =
  {0,0,1920,1080,0,0,1920,1080}, or render at 3840x2160. Prepared as bootpaint v3 (logs to
  /conf/bootpaint.log) but the reboot trigger did not fire -> UNCONFIRMED.
- Hook restored to shipped version (md5 2b1d4b7dc7acad8ca0e9b5ccaff03da7 = artifacts/payload/aurora.elf);
  test binaries removed. Launcher left DISABLED (/conf/launcher_enable removed) per user request; leftover
  omi_worker processes killed.

## 2026-10-30 openaurora runs on the TV + backlight investigation
- First real run on the set: /buffer/openaurora (glibc build) loaded ticker/panel/power, published services,
  the panel module opened /dev/ntkled successfully ("Can not open /dev/nvt/ntkled, try path /dev/ntkled" ->
  "Open /dev/ntkled successfully"), and power auto-enabled itself (saw /dev/ntkled). Clean shutdown.
- Firmware-derived PWM contract (libNDAL3.so, ghidra_ndal3 project): void Hal_Pwm_Init(void);
  void Hal_Pwm_Open(uint desc[6]) {ch,freq,duty,shift,mode,extra}; void
  Hal_Pwm_Set_ByChannel(uint ch, uint desc[4]) {freq,duty,shift,mode}; void Hal_Pwm_Close(void) - ALL void,
  so no return value to check (our earlier "failures" were reading a void return). mode must be < 9.
  Board constants from .data: gu32PanelFreqInHz=15000, genPwmdimMode=1; astPwmBasePin = {108,9,0},{119,2,9},
  {124,1,11},{126,1,12}. Chain: DEV_PANEL_SetBacklightLevel -> SetPWMBacklightLv -> SetPWMBacklightLevel ->
  DEV_PWM_Get_ChIdx(0x6c) -> DEV_PWM_Set_ByChannel -> plat_set_dimming -> Hal_Pwm_*. DEV_PANEL_GetPWMBackLightLevel
  is an EMPTY function (no readback exists). au8GPIOFuncName[0x6c] = "BACKLIGHT_LEVEL", [0x6e]=PANELPWR,
  [0x6f]=BKLTPWR. GPIO query = 0x88-byte struct, name pointer at +0x80, ioctl 0xc088671f -> id at +0,
  pin at +4, dir at +8; BACKLIGHT_LEVEL reports pin=1 dir=none.
- Channel sweep 0..12 at 10%: only a BRIEF FLICKER, then back to normal -> the value IS applied and then
  overridden while aurora owns the panel (gstBacklightPara cached level re-applied). So the path is real;
  steady-state verification needs a process that owns the panel (native shell, aurora stopped).
- Tools left on the set: /buffer/openaurora/*, /buffer/gpioprobe, /buffer/fbpaint, /buffer/tdgaprobe,
  /buffer/tdgaexp, /buffer/drmprobe.

## 2026-10-30 mod_display works (fbdev path proven on the TV)
- openaurora/modules/display: opens display.fb (default /dev/fb1), reads var/fix, renders into the back
  frame, msync + FBIOPAN_DISPLAY to present, publishes a `display` service (clear/fill/present/info).
  Self-test (display.test=1) magenta band was CONFIRMED VISIBLE on the panel. No DRM, no DirectFB.
- Options: display.fb, display.set_region=1 (writes OSD region 0x4020c86b back to the fb size - the
  4K->1080p fix, unverified on a cold boot), display.test=1. suspend/resume close/reopen the layer.
- Still open: region write before aurora runs (boot paint currently covers a quarter), and the native-shell
  ownership test.

## 2026-10-30 mod_audio: init order is mandatory (opcodes crash cold)
- Contract: void Hal_Audio_Ctrl(int opcode, uint *params), 56-byte block. 0x33 = volume (params[0]),
  0x35 = speaker mute (params[0]=dev id, params[1]=0/1), 0x3a = I2S MCLK (params[0]=0x100).
- Calling 0x33 without init SEGFAULTS inside the HAL (vendor coredump handler -> "Killed",
  dmesg [COREDUMP] SIGNR:11). Mandatory prelude, order from aurora's PLAT_DRV_INI_Init:
  Hal_Audio_Protect_Init() -> Hal_Audio_Mixer_Ctrl_Init() -> Hal_Audio_Proxy_Init() (all void(void)),
  then plat_common_init("DVIF"), Hal_Gpio_Init(), Hal_Iic_Init(), plat_vin_slot_init(),
  NDAL_VideoExt_OpenDevice(0,10).
- With the prelude, mod_audio applies volume 15/20/100 on the TV with no crash. NOT verified audibly:
  this household uses an external soundbar as the volume authority and has the TV audio levels disabled.
  Re-test with TV speakers selected, or use mute 0x35.
- Lesson: this is the first HAL call that can kill the process -> argues for isolating risky HAL users in a
  helper process later.

## 2026-10-30 IR keymap attempt: contention + watchdog, do it in-shell instead
- An external reader of /dev/ttyS2 gets 0 bytes while aurora runs (aurora's read loop wins every byte).
- Killing the vendor aurora makes the set REBOOT (watchdog), which also kills a capture in progress and
  truncates the log. Confirmed twice.
- Running irkey from the boot hook is unreliable: the hook can execute twice per boot (appmgr relaunches the
  app), so the one-shot flag is consumed by the first instance and the second goes straight to aurora.
- Static route also failed: the driver tables give NEC cmd -> DFB symbol (52 entries, known), but the
  symbol -> ICE keypress-id mapping is code inside aurora, not a table; a scalar scan for 0xf0xx only found
  false positives (an OpenSSL OCSP function).
- Decision: learn the keymap inside the native shell, where mod_ir owns /dev/ttyS2 exclusively - the UI can
  ask the user to press a key and label it. That is the only reliable place.
- Hook restored to the shipped version (md5 2b1d4b7dc7acad8ca0e9b5ccaff03da7); irkey + flags removed.

## 2026-10-30 HOW the IR keymap is built (answers the earlier dead ends)
- ICE_RemoteInit (0x77cdbc) -> _init_keypad_type() -> _profileloadkeydefinitions() -> ice_SetLongButtonKeys()
  -> _HW_MCU_GetSysCodeStatus() + _HW_MCU_GetRcAddr() (source of the 0x66 0x11 NEC address) -> two FROST
  IR threads.
- _profileloadkeydefinitions (0x77ac5c) BUILD THE TABLE AT RUNTIME:
    memset(keytable, 0xff, 0x51e);                 // u16 per entry, ~0x28f entries
    for (i = 0x200; i != 0; i--) {
        keyId = base[i];                            // key-id table at DAT_0077ae58
        code  = Profile_GetRemoteControllerCode(type, keyId);   // per-key IR code from the PROFILE
        keytable[i] = flagged ? reverse_byte(code) : code;      // table at DAT_0077ae5c
    }
  So the RC codes are per-model data in the profile/NVRAM, NOT a static table in the binary - which is why
  every {0xf0xx, id} scan found nothing and why a "table" never appeared. ICE_RemoteRegisterKeypressFunction
  only stores the callback pointer; the mapping is this keytable.
- Consequences for getting the map: (a) read the built keytable out of a running aurora via
  /proc/<pid>/mem (root, same box, read-only), or (b) decompile Profile_GetRemoteControllerCode to find the
  profile blob and read it from the NAND dump. Either gives keyId -> IR code for parity.
- MEASURED (aurora is ET_EXEC, fixed addresses 0x10000..0x1742000, so /proc/pid/mem reads work):
  *DAT_0077ae5c = 0x18e7c20, 0x51e bytes, 143 entries populated, remaining 0xffff. The populated values are
  SMALL (0..28, and 255), i.e. key *ids*, not NEC command bytes. So either this table is the key-id space
  and the RC codes live elsewhere, or Profile_GetRemoteControllerCode returns a key id for this model and
  the NEC mapping happens in the MCU/driver. Next step is to decompile Profile_GetRemoteControllerCode and
  read the profile blob - that settles the encoding.

## 2026-10-30 WHERE the IR codes actually live (the "file with all the codes")
- Profile_GetRemoteControllerCode(type,keyId) -> sprintf(buf,"RemoteControl/%s",type) -> ProfileManager
  getProfile("hwprofile") -> vtable get(keyId). So the codes are in the NVRAM profile "hwprofile",
  section "RemoteControl/<type>", NOT in any binary or /vendor file.
- The string "hwprofile" sits in the dump at NAND 0xeb97201, inside apdat0 (/conf, 0xE340000+0xA00000),
  immediately after an "EFSL" marker -> the NVRAM blob. That is the file to parse for parity.
- The two XMLs in /vendor cover the id spaces: ice_keymapping_db.xml = KeyString -> ICE keypress id (512+),
  vk_keymapping_db.xml = KeyString -> DIAL code (1000+). artifacts/protocol/ has both.
- So the full chain is: NEC code (driver table) <-> DFB symbol <-> ICE keypress id (XML) <- RC code
  (hwprofile/RemoteControl/<type>). Only the last hop is data we still have to read.
- Reading aurora's memory works because it is ET_EXEC: dd if=/proc/<pid>/mem bs=1 skip=$((0xADDR)) count=N.

## 2026-10-30 FOUND: the file with all the remote codes
- /conf/mb230_hwprofile.bin, 24514 bytes, md5 1b063b2cc7faa8b92c0f843921e561c7. Copy kept at
  artifacts/protocol/mb230_hwprofile.bin.
- EFSL container: "EFSL" magic, then "hwprofile", then binary records. Readable strings name the remote
  TYPES: VRc48XX (Vestel RC48 - this set), RCA_43XX_RC5, SingleTact, plus MB230/MB200, Si2157, 12Watt,
  Surround Sound, TIGroup7, default.aqs.bin.
- Profile_GetRemoteControllerCode(type,keyId) -> "RemoteControl/<type>" inside this profile; ICE_RemoteInit
  -> _profileloadkeydefinitions() turns it into the runtime table (aurora's *0x77ae5c, 0x51e bytes, 143
  entries populated we measured).
- Still to do: parse EFSL + the Profile record layout to get keyId -> RC code as data; then combine with
  ice_keymapping_db.xml (KeyString -> ICE id 512+) and the driver's nec/custom keycode maps (NEC -> symbol)
  for full parity.

## 2026-10-30 KEYMAP RECOVERED (no parsing needed)
- aurora's runtime table at *0x77ae5c is indexed by ICE keypress id and holds the RC code. Read it straight
  out of the running process (ET_EXEC, so VA == symbol address):
    PTR=$(dd if=/proc/$P/mem bs=1 skip=$((0x77ae5c)) count=4 | od -An -tu4)
    dd if=/proc/$P/mem bs=1 skip=$PTR count=1310 | od -An -tu2 -v -w2
  Then join with artifacts/protocol/ice_keymapping_db.xml (KeyString -> ICE id).
- RESULT (saved: artifacts/report/IR-KEYMAP-2026-10-30.md + artifacts/protocol/ir-keymap-2026-10-30.csv,
  143 rows): digits 0-9 -> codes 0-9; Up 20, Down 19, Left 21, Right 22; Select/OK 53; Exit 10; Exit2 37;
  Power 12; Vol+ 16, Vol- 17; Mute 13; P+ 32, P- 33; Info 18; Menu 48; EPG 47; Aux/source 56; Lang 15;
  Red 55, Green 54, Yellow 50, Blue 52; Stop 24, Play 25, Pause 49, Rewind 27, FForward 28, Record 51;
  Netflix 39; TextPatMix 60; MyButton 59; MyButton2 26.
- Unused ICE ids stay 0xffff (memset default); a few keys this remote lacks are 255.
- Remaining: confirm which byte of the UART frame carries the RC code (FF FF 50 A1 A2 CC ~CC B7 B8) - one
  key press with irkey in the native shell, compared against this table.
- NEGATIVE RESULT (checked): the driver's ss_keycode_map is NOT the same code space - 42 distinct values,
  26 of our RC codes absent, and where they overlap the symbols disagree (driver code 0x11 -> symbol 0x30 =
  KeyZero, while the profile says KeyZero is code 0). So two encodings exist (profile RC codes vs the
  NEC/scancode path) and only a live capture decides which one this MCU puts in the frame.

## 2026-10-30 stock UI assets + layout extracted (parity)
- /vendor2/dynamic_ui (volume ap1) extracted with artifacts/scripts/extract_from_dump.sh vendor2:/dynamic_ui.
  default.ui = manifest (lists the packs, then "V.0.51.52.3", theme "telefunken18").
- teleftunken18_img.txt/.bin: index rows are "type name hash w h SIZE flag"; the .bin is the assets
  concatenated IN ORDER, so offsets are cumulative and field 6 is the size (verified by PNG signatures:
  0, 198, 519=198+321, 769=519+250...). Extracted 964 PNGs (6.8MB) to artifacts/ui-assets/ (10 non-PNG
  slots skipped). That is the complete stock artwork set.
- telefunken18_svg.bin is NOT vector art: it is the stock UI LAYOUT description, records of
  length-prefixed name + properties (background, rowItems, title_gRowL, #FFFFFF, #BE1E2D, none,
  matrix(0.9999 0 0 0.9999 13 4), /png/picture.png). 702 records, index = hash,len pairs in *_svg.txt.
  Copies in artifacts/ui-assets/_layout/ (font pack 30MB left on the image).
- Report artifacts/reports/UI-ASSETS-2026-10-30.md. Next: decode the layout property ids, pre-convert the
  needed icons to raw RGBA for mod_display, then assemble the parity UI on display+ir.

## 2026-10-30 /conf is offline too (no TV needed for device data)
- apdat0 (/conf) is mtd13 at 0xE340000, size 0xA00000, and lives in artifacts/dumps/tv_nand.bin. Carve it
  and run ubireader_extract_files:
    python3 -c "d=open('artifacts/dumps/tv_nand.bin','rb'); d.seek(0xE340000); open('/tmp/apdat0.bin','wb').write(d.read(0xA00000))"
    /tmp/opencode/venv/bin/ubireader_extract_files -o /tmp/apdat0_x /tmp/apdat0.bin
  -> /tmp/apdat0_x/<id>/apdat0/ (48 entries: settingsDB.db, mb230_hwprofile.bin, PCF.pcf, PQF.bin, ...).
- VERIFIED: the dump's mb230_hwprofile.bin is md5 1b063b2cc7faa8b92c0f843921e561c7 = the copy pulled from the
  live set. So the keymap source, settings DB and every other /conf file can be read without waking the TV.
  (Live settingsDB.db differs from the dump only by our edits: privacy flags + the OmarchyTV tile.)
## 2026-10-31 COLOUR ORDER WAS A TOOL BUG (all reference captures were swapped)
- tools/tvshot.py decoded every 32bpp layer as RGBA; the layers are BGRA (byte0 = blue), already
  proven by tools/colorbars on the panel. So every capture we owned had red/blue swapped, and the
  palette sampled from shot-launcher.png was wrong too.
- Fixed tvshot.py (default BGRA), corrected shot-launcher.png in place (pre-fix copy kept as
  shot-launcher.as-captured.png), and fixed the ui_fb palette.
- Truth: the stock portal's rail is orange->maroon #f7941e, #f15a29, #be1e2d, #811924, #481730.
  #BE1E2D is a literal in the vendor style pack (telefunken18_svg.bin: title_gRowL, itemPos, ...), so
  the corrected palette reproduces a value the firmware itself carries.
- Geometry from the corrected capture: title ink at (29,80) 262x54; rail 5 slots x24..144 from y211,
  120 px each, the FOCUSED slot grows to 150x150 with a 5 px #f7941e accent frame; 9 app cards
  171x95 at x=209+i*190, y=478.
- Content is the device's own: rail glyphs cut from the portal framebuffer; cards from
  /buffer/downloaded_images/launcher_app_thumb<N> (the LauncherItem rows m_item_type=2,
  m_parent_id=1); title with /vendor/fonts/DefaultProportional.ttf. openaurora/tools/mk-ui.py
  builds the OAI1 blobs. Parity: whole-frame mean |delta| 1.62/255; title 12.46 (font not exact).
- LAYER OWNERSHIP: both fb0 and fb1 belong to vendor aurora and fb1 is the UPPER one (the
  "Geen signaal" box drew over our fb0 paint). aurora updates by REGION, so whole-frame writes from
  another process interleave as horizontal strips. Its no-signal banner has a live clock => ~1 Hz
  repaint. Conclusion: a standalone UI needs to be the OSD owner; nothing can be "held" alongside.
- Our own FBIOPAN_DISPLAY was the original "flash": it made DFB flip back. Hence display.pan=0.
- SIGSTOP of vendor aurora -> MCU watchdog resets the set (confirmed again). libNDAL_EX exports
  NDAL_VideoExt_STBC_{StopWDT,FreshWDT,SetWDT,GetMcuWatchDogTimeout,SetMcuWatchDogTimeout}; a probe
  that called GetMcuWatchDogTimeout cold (dlopen RTLD_LAZY) was killed and the set rebooted TWICE.
  The STBC path goes through ndcommon_v4l2_config_ctrl and needs aurora's init order first. That is
  the next step (see artifacts/reports/UI-PARITY-2026-10-31.md section 4).

## 2026-10-31 THE GATE IS OPEN: MCU watchdog + the plane stack
- Vendor aurora feeds the MCU watchdog: PLAT_SYS_KickWatchDog() == NDAL_VideoExt_STBC_FreshWDT().
  Going through libNDAL_EX from another process RESETS the set (needs the full STBC state machine).
- Working route: pcbconfig talks 6-byte packets on /proc/nvtstbcipc. {0,0xae,hi,lo,1,0xae} = set
  timeout, {0,0xae,0,0,0,0xae} = disable. Implemented as openaurora/tools/mcu/wdtctl.c (fcntl needs
  syscall(SYS_fcntl,..) because the toolchain emits fcntl@GLIBC_2.28 and the set has 2.23).
- VERIFIED: after `wdtctl disable`, vendor aurora can be SIGSTOPped and the set stays up (224 s vs a
  reset within ~40 s before). Killing it does NOT respawn it either (appmgr leaves it dead), so a
  clean "no vendor UI" state is reachable without a reboot.
- mod_display region: the size is words 2/3 (local_b8/local_b4) per NVTGA_OSD_SetRegion, but the
  field layout is NOT settled: `[4]=1920 [5]=1080` showed the layer as a bottom-right quadrant,
  `[4]=0 [5]=0` showed nothing. The driver also sends 0x4004c871 (stride) and 0x4008c80a (8-byte cfg)
  after the region write, and 0x400cc883 + 0x4008c80a at InitLayer - mod_display sends none of them.
  Next: decompile the kernel fb ioctl handler for 0x4020c86b in ker*_sym.elf and name the fields.
  Do not keep guessing at it on the panel.
- The OSD is hidden because the VIDEO PLANE is configured over DRM by aurora: SETPLANE 0xc03064b7
  (plane 0x3c/0x3b/0x38, crtc 3840x2160, src 1920x1080 16.16), ADDFB2 0xc01864ba, plus the Nuvoton
  vendor ioctl 0xc1a46451 with a 420-byte plane table. Hal_Video_Set_DrmPlaneInfo(plane, u32[12])
  stores a 12-word descriptor per plane at base+plane*0x30+8.
- Boot trace saved: artifacts/protocol/oatrace-boot-2026-10-31.txt (capture with
  openaurora/tools/hold/oa_trace.so via LD_PRELOAD; enable with `touch /conf/oa_trace_enable`).
- NEXT: decode the 12-word plane descriptor from the kernel driver (handler for 0xc1a46451), then
  implement plane+region setup in mod_display, then the /conf/ui_test boot mode. Full write-up:
  artifacts/reports/PANEL-TAKEOVER-2026-10-31.md.

- Recorded in AGENTS.md so future sessions do not wake the set for data.

## 2026-10-31 END-OF-SESSION STATE (pick up here)
- Set: vendor UI running, payload hook (226 lines, 2b1d4b7d), telemetry blackholed, dialserver up,
  /conf/coredumpnoreset set (crashes no longer reset). /buffer: openaurora 3fa345d7, 11 modules, 15 ui
  blobs, wdtctl/fbregion/drmprobe.
- Verified today: mod_plat runs PLAT_DRV_INI_Init's order (audio prelude -> plat_common_init("DVIF") ->
  GPIO/I2C -> VIN defaults -> NDAL_VideoExt_OpenDevice(0,10)) with requires[] gating; mod_display opens
  fb1, commits the region, flips; ui_fb paints the parity portal (1.65/255 vs the stock reference).
  Runtime swap works: wdtctl disable + kill -9 aurora -> no reboot, no respawn, ours owns fb1.
- Blocker: our OSD is not on the glass (HDMI1 is). Unproven hypotheses: plane z-order / plane never
  enabled / plane scans a different buffer / region write hides the window. The old measurement that
  /dev/fb1 never appears for our process without the vendor UI has NOT been retested.
- Next (review-approved): (1) cold-boot trace of OUR process with aurora never started; (2) supervisor in
  the hook (host-loop heartbeat -> SIGKILL -> wdtctl set 10 -> exec vendor UI) before any oa_boot test;
  (3) then boot-time takeover; (4) never drive dp_order without a 12-word plane record and a checked
  return (0xcccccccc is DRM_MODE_OBJECT_CRTC; a bad write oopses the kernel).
- Adversarial review (Grok 4.7) caught: a 32-byte stack overwrite in the dp_order path (now deleted),
  three unproven conclusions (retracted in the report), ui_fb hold leak on suspend, duplicate/lost holds
  in api_lookup, stop()/failed start() not cleaning registrations, and a test that asserted nothing.
  All fixed; make/make check/make tv green (16 assertions).
- Report: artifacts/reports/PANEL-TAKEOVER-2026-10-31.md (has a "Pick up here" section at the top,
  the retractions in sections 3/5/6, and the review in sections 9/10).

## 2026-10-31 THE "BRICKING" EXPLAINED: core_helper.sh resets the set on any crash
- The kernel core pattern is `| /usr/sbin/core_helper.sh gzip %p %e %t`. Without
  `/conf/coredumpnoreset`, that script runs `before_system_reset` (umount + `ubidetach` `/mtd_rwarea`,
  `/conf`, `/buffer`) and then `pcbconfig -i -w 1` — the MCU watchdog armed at **1 s** with nothing left
  to kick it, so the SoC resets ~1 s later. Every "bricked the TV" event today was one of our probes
  segfaulting; `/conf/always_logging_reset_count` reached 2 after the first two.
- `/conf/coredumpnoreset` is the documented off-switch (handler then calls `pcbconfig -i -w 0` and
  exits). It is set, and **verified**: a deliberate SIGSEGV left the set running (uptime 387→395 s,
  counter 2→3, `dmesg [COREDUMP] SIGNR:11`, aurora + dialserver still up). `/conf/enable_coredump`
  only controls whether cores get written to USB.
- The other reset path is `/vendor/run.sh`'s `trigger_reboot &`: it waits for `/tmp/trigger_reboot`, then
  `pkill -f aurora.elf`, `pcbconfig -i -w 10`, detaches the same three partitions and runs the file's
  command. Neither path is a process supervisor — appmgr has no watchdog and does not respawn.
- Boot chain for reference: inittab → rc.local → (show_bootlogo, active_usb, mount_ubifs with
  insert_idtv_ko &, mount_apdat &) → /usr/script/startup.sh → /vendor/run.sh → appmgr | run_aurora.sh.
- Practical consequence: development crashes are now harmless, which removes the main reason the
  takeover experiments kept "bricking" the set.

## 2026-10-31 STICK RESTORE DROPPED THE PROTECTIVE HOOKS (+ sysfs/mknod finding)
- Recovering with `restore_apdat0.scr`/`apdat0_full.ubifs` restored an OLDER /conf: the hook came back
  135 lines (payload is 226), so `hosts` (telemetry blackhole), `dialserver`, the Samba iptables block
  and the privacy force were all missing, and `/conf/remotelf/hosts` did not exist at all. The set was
  rooted but unprotected. Fixed by re-uploading `artifacts/payload/hosts` and the payload hook
  (+ our takeover block), verified: hosts bind 0, dialserver pid, db internet flag 1, 216.183.117/24
  REJECT present.
- Same restore broke ssh for two separate reasons worth remembering: the restored `authorized_keys`
  did not contain the laptop key, and `/tmp/tvh_root` was 777 (dropbear refuses pubkey auth unless the
  home is 700). Telnet on 2323 (unauthenticated root) is what got us back in.
- Display: fd nodes and sysfs disagree during boot. In a takeover boot the log shows
  `/dev/fb1: open No such file or directory, node missing, fb registered` - i.e. `/proc/fb` and
  `/sys/class/graphics/fb1/dev` say the framebuffer IS registered while the /dev node is absent.
  `mknod` in /dev works (devtmpfs, mode 755), and hotplug is `/sbin/mdev` (rc.local sets it in
  `active_usb()`; `mdev -s` is commented out). So the readiness signal must be sysfs, and the module
  now creates the node from `/sys/class/graphics/fbN/dev` when it is missing.
- `Hal_Video_DrmInit(1)` from libNDAL3 does run in our process (NDAL prints
  `[DRM Init] : drmModeGetEncoder= PASS`) but does not by itself make the fb node appear; libNDAL_EX
  must be dlopen'd with RTLD_GLOBAL first (gs32Ndp_PQ_fd).
- RE: `register_framebuffer` is called only from `.rel.init.text` and `unregister_framebuffer` only
  from `.rel.exit.text` of `ntkdriver_nondovi.ko` (no runtime (un)registration), and no other module
  imports either - so the node lifecycle is a devtmpfs/hotplug matter, not a driver one.

## 2026-10-30 UI strings extracted (langprofile)
- /vendor2/mb230_langprofile.bin (5,171,699 bytes) = EFSL container, entry "langprofile", NOT compressed:
  NUL-separated UTF-8, one translation per language per string id in a fixed language order (for
  "Settings": Settings, Vendosjet, Parametrlər, Наладкі, Podešavanja, Настройки, Podešavanja, Nastavení,
  Indstillinger, Instellingen, Seaded, Asetukset, <empty>, Réglages ...). Dutch is slot 12 of that run;
  exact alignment needs the language enum (/vendor/mb230_langprofile.bin is a 30-byte stub).
- Extracted artifacts/ui-assets/_layout/lang-strings.txt: 70,948 fields, 59,359 unique, 56,715 texty (2.5MB).
- So parity now has artwork (964 PNGs), layout descriptors (telefunken18_svg.bin), strings (lang-strings.txt),
  the keymap (ir-keymap CSV) and the behaviour modules. Remaining gaps: layout property-id meanings, the
  language enum, and a PNG decoder (or pre-converted icons).

## 2026-10-30 FIRST NATIVE UI RENDER ON THE TV (vendor icons)
- mod_display gained blit(x,y,w,h,rgba) with alpha blending; oa_display_iface is now
  clear/fill/blit/present/info.
- openaurora/tools/icons-to-raw.py converts vendor PNGs to "OAI1" blobs (magic + w + h + RGBA).
- mod_ui_fb (new, module #10) looks up the `display` service (requires OA_SVC_DISPLAY), loads icons from
  ui.dir (default /buffer/openaurora/ui), paints a background + top/bottom bars + a tile grid with the icons
  centred, then present(). Deliberately a canvas test, not the final UI.
- CONFIRMED ON THE SET: "Yes - tiles with vendor icons". So the whole native chain works: display service ->
  fb1 -> panel, drawing the vendor's own artwork extracted from the firmware theme pack.
- Deployed: /buffer/openaurora/modules/mod_ui_fb.so + /buffer/openaurora/ui/*.rgba.
- Reading technique that works (keep it): aurora is ET_EXEC so VA == symbol address;
  dd if=/proc/<pid>/mem bs=1 skip=$((0xADDR)) count=N; find the pid with
  `for p in $(pidof aurora.elf); do tr "\0" " " </proc/$p/cmdline | grep -q vendor/com.nvt.idtv && echo $p; done`.

## 2026-10-30 mod_settings (last T1 module started)
- settingsDB.db Settings table = ONE row with named INTEGER columns (m_cec_enabled, m_privacy_*,
  m_current_source, ...). Module API: get_int(col)/set_int(col,v)/is_writable(); column names validated to
  [A-Za-z0-9_] because SQLite can't parameterise identifiers. Read-only by default (settings.readonly=1).
- Links the firmware's libsqlite3.so.0 (vendor:/lib/libs/libsqlite3.so.0, fetched + symlinked to
  libsqlite3.so for -lsqlite3) on the TV, system sqlite on the host; vendored third_party/sqlite3.h so the
  ARM toolchain (which has no sqlite header) builds the same code.
- Verified on the set: opened /conf/sqlite_db/settingsDB.db read-only, probe m_cec_enabled=1. Host test now
  runs against a copy of artifacts/privacy/settingsDB.final.db; suite is 15 assertions, all green.
- openaurora now has 8 modules: ticker, ir, ui_console, panel, power, display, audio, settings.

## 2026-10-30 source (Hal_Vin) fully mapped
- void Hal_Vin_Open(int idx)          -> opens /dev/ntkdvif (per-index suffix), returns fd (O_RDWR|O_NONBLOCK,
  FD_CLOEXEC)
- void Hal_Vin_SelectInput(int fd,int input) -> ioctl(fd, 0xc0045627, &input)
- void Hal_Vin_SetStreamStart(int fd)        -> ioctl(fd, 0x40045612, &{1})
- also GetHdmiStatus, GetRes, SetInputRes, SetPcMode, SetColorSystem, GetLock, GetVDLock (11 fns total, all void)
- aurora entry: plat_vin_slot_connect(slot, source_type) -> Hal_Vin_Open(0) -> FUN_0092e8ac(fd,type) ->
  Hal_Vin_SelectInput -> Hal_Vin_SetStreamStart -> Hal_Video_Set_V4l2Info (inputs surface as V4L2 devices on a
  VIN slot, which is why Hal_Vin - not DRM - is the right API).
- ID TABLE RESOLVED (.data 0x013b9e78, 26 entries, companion VPLT table at 0x013b9ee0): type 0->1, 5->0,
  10->2, 15->1, 16->2, 17->3, 18->3, 19->66, 20->3, 21->3, 25->255, others 0.
- So mod_source is now small: open, SelectInput(table[type]), SetStreamStart, publish. Enumeration from
  ICE_External*/SRC_CFG_SourceSupportTable. Not implemented yet; switching is disruptive to test.

## 2026-10-30 mod_source implemented (T1 code complete)
- openaurora/modules/source: Hal_Vin_Open(0) at start (confirmed on the set: "VIN open (fd 5)"),
  publishes a `source` service with select_type(type) -> Hal_Vin_SelectInput(fd, table[type]) +
  Hal_Vin_SetStreamStart(fd). id table transcribed from aurora .data 0x013b9e78.
- Switching DISABLED by default (source.enable=1) because it changes what is on screen; the module still
  opens the device and publishes the service so the rest of the system can discover it.
- T1 now has code for every module: display (verified), power (verified), settings (verified), audio
  (running), panel (running, overridden while aurora owns the panel), source (running, switch gated), ir
  (protocol done, keymap to be learned in-shell), plus ticker/ui_console demos. 9 modules total.

## 2026-10-31 continuation — recovery, DRM contract and TODO audit

- Read the handoff/native project and continued the accepted supervisor/scanout work. Vendor UI
  remained running throughout; no reboot, real watchdog command, source change or display write.
- Implemented `openaurora/tools/boot/oa-supervise.c`, built by make and make tv. The host dispatch
  loop atomically writes PID + CLOCK_MONOTONIC milliseconds to `/tmp/openaurora.alive` once a
  second; ui_fb writes `/tmp/openaurora.ready` only after a successful present, once per start.
  SIGTERM/SIGINT now actually stop the host (the old handler set an unread flag).
- Supervisor: 75 s startup grace, then 5 s heartbeat deadline; kill/reap child → `wdtctl set 10`
  → exec vendor UI. Three failures remove `oa_boot`; 30 healthy seconds clear the streak.
  `oa_safe` bypasses or ends the attempt. Helpers have a 2 s deadline. This is userspace recovery,
  not protection against a kernel hang or a dead supervisor, and readiness is not scanout proof.
- Deployed core, supervisor, ui_fb, config and opt-in hook. Existing files backed up with suffix
  `.pre-recovery-20261031`. `/conf/oa_boot` and `/conf/launcher_enable` remain absent;
  `/conf/coredumpnoreset` present. Hook MD5 `0b71c42f7b0fdfca2dd6cde78ae17de5`;
  final core after registry fixes `a898384df076572cf377749f25427689`.
  Images/stick were NOT rebuilt. Rebuild script now strips native takeover/trace/hold markers.
- Tests: make check passes lifecycle smoke checks, registry regressions and 11 recovery cases.
  ARM build max GLIBC remains 2.17. Registry regression executable also passed under qemu with
  the TV rootfs. On-TV recovery smoke used a fake display module and stub watchdog/fallback:
  heartbeat advanced, SIGSTOP timed out, child killed/reaped, stub fallback ran. It did not touch
  TV hardware or the vendor process. See `openaurora/docs/RECOVERY.md`.
- Offline DRM findings (`artifacts/reports/DRM-PLANE-CONTRACT-2026-10-31.md`): the 12-word cache
  has plane/CRTC/fb IDs at 0/1/2, CRTC xywh at 4..7, source x/y/h/w at 8..11. Getter/setter
  return int 0/-1 (assembly), despite Ghidra's void prototypes. `_hal_video_drm_kms_init` only
  discovers IDs; it DOES NOT SETPLANE. DrmInit's dummy framebuffer is 4096x2304 XR24, not evidence
  of fb1 binding. FlushPlaneInfo repeats discovery, not a hardware commit.
- Found the explicit vendor OSD commit: PLAT_OSD_Plane_Enable → plat_OSDPlaneMute → Window::SetMute
  → Window::Update → Sync_FrontSurface2HW → plat_Gfx_DRM_osd_drv::Update_Enable @00944860.
  Dirty state can GETFB/ADDFB2/SETPLANE; other branch submits 0xc00c6455 (three words). This is
  separate from the generic HAL plane cache. Need decode buffer allocation/flags/handle mapping
  before any hardware experiment. No dp_order write added or retried.
- Owner requested an audit of checked TODO entries. `TODO-AUDIT-2026-10-31.md` inventories all
  72 checkmarks present at audit start; 14 reopened (plus the earlier plane-setup retraction).
  Research milestones are explicitly historical where not repeated. Core/loader are not generally
  safe for threaded hot unload: posts lack module ownership. oa_module lacks the claimed size field.
  IR, independent display, native power lifecycle, audio, source, full settings, platform init and
  region semantics remain partial. Several older completion claims above are superseded by this audit.
- Audit reproduced and fixed three concrete bugs: full hold table returned an unprotected iface;
  stop(provider) bypassed held-service unload protection; failed init left service/timer registrations.
  Added tests/registry.c + tests/failed_init.c. Also fixed owner attribution during failed-start cleanup.
- Recount: NDAL3 has 321 distinct defined Hal_* dynamic functions, EX has 80 NDAL_* (old 643/160
  totals are not distinct dynamic function counts). lang-strings.txt has 56,715 sorted text lines, not language-aligned raw
  fields; Dutch-slot/ID mapping remains unresolved. Obsolete generated vendor-hal.h PWM declarations
  are still unused and wrong; correcting/removing them is an explicit TODO.
- Read-only target audit: SSH 22/telnet 2323/DIAL 56789/56790 open; dd.xml 200 with correct
  Application-URL; privacy values `1|0|0|0|0|0|1|0|1` = internet/ads/personalised/marketing/3p/ACR/
  DNT/logging/wake. Hosts block and Samba OUTPUT REJECT present. No new egress capture or real
  hardware parity test was performed.
- Next: ownership/ABI gaps in core and the OSD-specific buffer/commit chain are the concrete open
  work. Independent cold scanout and actual remote navigation still gate a native boot acceptance.

## 2026-10-31 stability work — USB recovery required after agent error

- Added payload/keepalive.sh: listener checks/respawn for SSH 22 + telnet 2323,
  root-home/key permission repair, hosts/Samba rule checks every five seconds,
  bounded DHCP repair and privacy/internet/WoL flag correction every minute.
- Tested stopping SSH and telnet listeners separately; fresh connections returned
  within seven seconds. First synced forced reboot succeeded: boot ID changed from
  2753791f-cc96-4a1f-85b9-35beaca6bacc to c5a60f8c-33d3-4c2f-86b2-a7c0aa80912d;
  key SSH, root telnet, .87/real MAC, hosts/IP block and privacy flags all survived.
  oa_safe was set, native/web launch flags removed. No-force-net removed.
- Rebuilt apdat0_full.ubifs successfully from this working state, round-trip verified.
  MD5 994da7ff76a0b2fbafb8f6fa0c682627. It contains hook 0b1cd32a9de2b0251f6252e6603db8b7
  and keeper 4ebd0f9d7ee9f470afde8b0f2807522b. ubireader -k verifies hook/keeper
  mode 700 uid/gid 0, dropbear 755, authorized_keys 600; oa_safe present, oa_boot absent.
- Then attempted a small setsid launch change. The agent erroneously used a remote
  command ending `cat > hook.new && ... && (sleep 2; reboot -f) ... &` while streaming
  the upload on stdin. The trailing & backgrounds the whole AND-list, so cat can
  read /dev/null and install an empty hook. Reproduced locally: zero-byte output.
  TV is now unreachable; no successful post-error hook readback. Treat as likely
  empty hook, not a proven hardware/firmware failure. Do not claim stability.
- A verified working image predates the mistake. Prepared artifacts/recovery-stability/
  with apdat0_full.ubifs and kylo_usb_update.scr (=apply_apdat0_hook.scr, 549 bytes,
  MD5 2c9a1b458714619f27ead258768db1a1, valid final percent marker).
- Owner has now inserted /dev/sdb1, FAT32 label TVHACK, UUID 5DA9-104B,
  mounted /run/media/ekollof/TVHACK. Only copy the prepared recovery image and active
  script after backing up their old versions; preserve the existing NAND chunks.
  Then sync/eject and ask owner to do OK-hold at mains power-on. Do not format.
- After recovery: verify SSH key login, root telnet, keeper singleton, expected
  privacy flags, hosts/Samba block, .87/real MAC and stock UI. No further deployment
  experiment is needed. Never stream uploads and background a reboot in one call;
  require a separate successful installed size/hash check before reboot.
- Stick preparation completed: old image and script backed up with suffix
  `.pre-stability-recovery-20261031`; both new files copied and readback MD5s match.
  Existing NAND chunks preserved. RECOVERY-STABILITY.txt added. /dev/sdb1 unmounted
  successfully, so the stick is safe to remove; optional power-off was denied by
  polkit and is unnecessary after sync/unmount. Awaiting owner's physical OK-hold
  recovery at TV power-on, then remote verification.
- Owner reported no progress bar. Rechecked the actual 8 GB stick: /dev/sdb1,
  TVHACK, UUID 5DA9-104B, DOS/MBR partition type 0x0c FAT32. Script is exactly
  kylo_usb_update.scr in the root, 549 bytes, byte-identical to the previously
  working script and matching the filename embedded in this TV's U-Boot.
  Both recovery-file hashes match. apdat0_readback.bin still hashes to the older
  76b1464a... image, so the new restore has not completed.
- Found persistent FAT dirty state and three orphan cluster chains after unmount.
  Ran fsck.fat -a while unmounted, preserving/recovering the orphan chains;
  subsequent fsck.fat -n is clean. Read-only remounted, compared both files
  byte-for-byte with prepared recovery artifacts, then unmounted and checked
  clean again. This repairs a real filesystem issue but does not establish why
  the TV skipped recovery. Stick is safe to remove for another physical attempt.
- Owner reports LED blinking with black screen, still no progress, asks for the
  previous agent's scripts. Restored exact stick apply_hook.scr.txt (584 bytes,
  MD5 de1591298d633cf6422a1ade4e8835a9) as active kylo_usb_update.scr. Its executable
  command lines match the prior 549-byte script; only comments/blank lines differ.
  Recovery image remains verified 994da7ff...; readback still old 76b1464a... .
- Imported the TV's own U-Boot into Ghidra: /tmp/opencode/ghidra_uboot/UBoot,
  uboot-code.bin, AARCH64 LE64, base 0x02800000 (uImage header removed). Logs:
  /tmp/opencode/uboot-recovery-analysis.log and uboot-recovery-trigger.log.
  FUN_02811c68 loads kylo_usb_update.scr at 0x03200000 then runs nvtsource.
  FUN_02811868 scans 8192 bytes for %, tokenises CR/LF, skips # comments and
  handles #@ for progress count. Current syntax/end marker meet those checks.
  FUN_02810d1c enters RC recovery when low byte of MMIO 0xfc04011c is 0xaa,
  loops usb_port 0..3, invokes nvt_script_update and reboots. Blink alone is not
  proof of that flag or successful file loading. No syntax defect identified.

## 2026-10-31 20:02 — recovery succeeded; TV now in use, read-only access only

- Owner confirmed exact older script worked, then allowed boot to finish.
- Fresh SSH key login and root telnet both verified. New boot ID:
  ebb0aaaa-72a2-4410-9f8d-95b9249f8a90. Uptime advanced past 62 s; same keeper
  PID 2234, SSH listener 1811, telnet and DIAL present. Vendor aurora running;
  no openaurora process. No further reboot or deployment performed.
- eth0 UP/RUNNING, .87 and real MAC preserved, LAN/default routes present.
  Hosts matches persistent blocklist, Samba OUTPUT REJECT present. DB values:
  internet/ads/personalised/marketing/3p/ACR/DNT/logging/wake = 1|0|0|0|0|0|1|0|1.
  Root home/.ssh 700; oa_safe and coredumpnoreset present; oa_boot absent.
- Live hook MD5 0b1cd32a9de2b0251f6252e6603db8b7, keeper
  4ebd0f9d7ee9f470afde8b0f2807522b, matching verified image. Reverted the local
  loose hook's untested setsid line to match that working version; no TV upload.
- Updated AGENTS/TODO/payload README and STABILITY-2026-10-31.md to reflect
  actual recovered state and distinguish old raw images from the current UBIFS.
- OWNER'S LATEST CONSTRAINT: TV is in use. Remote access/read-only diagnostics
  are allowed; do not interrupt operation. No writes/deployment, service restarts,
  network changes, remote keys, suspend/WoL or reboots until re-authorized. Use
  plain /usr/bin/ssh -F /dev/null, since tvssh.sh can auto-wake the set. Finish
  current task by leaving it alone; future RE can proceed offline.

## 2026-10-31 — offline ABI 2 follow-up and owner scope constraints

- Owner: **do not port telemetry or advertising into openaurora**, including
  disabled/optional versions. Exclude usage/app-click reporting, ACR, tracking
  SDKs/profiling, advertising IDs, sponsored placements and ad delivery. Local
  diagnostics and other TV features remain in scope. Updated AGENTS/TODO/design
  and README. Implementation tiers are an order of work, not permission to
  descope broadcast/EPG/PVR/teletext/CI+ from the bug-for-bug target.
- Owner also requires **`-Werror -pedantic` in CFLAGS**. Added to the default
  build, fixed dlsym pointer conversion using memcpy + static size assertion,
  and made config copy precision explicit. No warning suppression added.
- Implemented ABI 2: descriptor size, owner_id(), and post(priv, owner, cb, ud).
  Active owners/queue share a mutex; workers never read mods[]. Cleanup closes
  acceptance and cancels queued posts before stop/destroy. Dispatch pops one at
  a time and attributes callback registrations to the captured owner. Workers
  must still be joined by stop/destroy and canceled userdata reclaimed by modules.
- Real pthread fixtures cover reload/state/token rejection, queue saturation,
  late posts during stop, restart, failed init/start, and one callback unloading
  another queued callback's library. Guard-page fixtures cover old/short/extended
  descriptors. Full strict host suite passes; safety tests pass under ASan/UBSan;
  ARM strict build + safety/registry tests pass against TV libc under qemu.
  Max GLIBC requirement 2.17. Report: MODULE-SAFETY-2026-10-31.md.
- Build/test isolation fixed: tests/run.sh and recovery use selected BUILD.
  Offline ARM artifacts live in **build-tv-v2**, MD5 core
  1738708febf70a9c7602be1421390689. TV/recovery artifacts and old build-tv stay
  ABI 1. Rebuild core and all modules together when ABI 2 is eventually deployed.
  No TV access or changes in this continuation.
- TODO now distinguishes these fixes from remaining lifecycle concerns:
  reload ignores restore() failure and always starts previously stopped modules;
  suspended reload behaviour also needs defined semantics/tests. PWM generated
  header remains stale. Independent scanout/IR/native hardware parity still open.

## 2026-10-31 — firmware-first OSD correction and missing frame commit

- Owner reiterated: firmware is the source of truth; decompile/investigate before
  implementing. Deferred reload-state semantics work and traced OSD instead.
  No TV access, writes, drawing, deployment, keys or reboot in this continuation.
- Imported/analyzed vendor2/modules/ntkdriver_nondovi.ko into
  `/tmp/opencode/ghidra_nondovi nondovi`. External kernel relocation warnings were
  not treated as resolved evidence. Local functions and objdump were cross-checked.
  New project-local `openaurora/tools/re/DumpFirmware.java` outputs exact-symbol
  or @address decompilation + optional disassembly, MD5 and metadata warning.
- **Corrected old handoff claim:** fbdev ioctl 131/132 are palette set/get,
  `{start,count,pointer}`, via pfSetPaletteData/pfGetPaletteData. They do not
  acquire/return frame buffers. Updated historical report, TODO and AGENTS;
  withdraw old proposed "acquire with 132" experiment.
- **CREATE_DUMB only allocates 4 KiB:** ntkndpdrm `_ndp_drm_gem_cma_dumb_create_4k`
  ELF .text+0x12090 computes dimension-sized metadata but at +0x1210c loads
  r1=4096 before drm_gem_cma_create. Earlier large mmap failures do not rule out
  DRM for OSD. Actual Aurora uses private create 0xc0106440 with 16-byte
  size/flags/handle, then MAP_DUMB+mmap64, GEM flink/name table, ADDFB2/SETPLANE.
  Dedicated flags logical main/sub/txt/cursors: 0x40000/0x80000/0x100000/0x200000.
  68-byte OSD state is distinct from generic NDAL 48-byte records. Full table,
  ioctl payloads, addresses and startup sizes are in OSD-BUFFER-CONTRACT-2026-10-31.md.
- **Actionable display omission fixed offline:** current default display.pan=0
  previously returned success after msync, skipping FBIOPAN_DISPLAY. Vendor
  UpdateRegion/FlipRegion always commit with 0x4606; kernel pan binds physical
  buffer and calls pfUpdateEnable. present now always commits, including with
  one frame or rotation disabled. Sync/commit errors propagate and retain the
  frame for retry; resume chooses the current/next frame consistently.
- Actual display module is tested with wrapped fake fbdev in display_present.c:
  no device is opened. Three groups pass strict host, ASan/UBSan and ARM/qemu;
  full host lifecycle/registry/safety/recovery checks pass. TV build is still
  staged only in build-tv-v2. Cold scanout remains UNPROVEN. Do not call it fixed
  on screen or deploy while the set is in use.
- **PWM ABI recheck corrected another earlier claim:** Ghidra void metadata is
  wrong for Init/Open/Set/Terminate. ARM epilogues explicitly return scalars;
  Open reads 6 words, Set reads 4 and accepts mode<9. Close takes no channel.
  Created maintained vendor_pwm.h with size/offset assertions; panel uses it,
  fetch script copies it to vendor-hal.h. Panel policy/results interpretation
  unchanged; nonzero can take a shortcut without an ioctl. Strict host+ARM builds,
  header comparison and script syntax pass. See PWM-CONTRACT-RECHECK-2026-10-31.md.
- Raw decompilation/instruction logs are saved in artifacts/analysis/osd-2026-10-31.
  Next offline gate: trace framebuffer registration and physical-buffer callbacks,
  reconcile Sync_FrontSurface2HW's doubled page-aligned size with GEM allocation,
  and recover complete enable/alpha/resume state before choosing a cold backend.
  Do not assume the newly corrected commit is the sole source of invisible output.

## 2026-10-31 — read-only runtime preflight, verified on real TV

- Owner requested a TV library audit and `--check`, suggested ARM qemu, then
  explicitly authorized testing --check on real hardware with its temporary
  CPU/memory/disk cost. Also prefers `tools/tvssh.sh`, including WoL when needed.
  General constraints on UI/startup/restarts/network/persistent deployment remain.
- Initial installed-file loader traces found all library files, but three older
  modules (audio/panel/source) have unresolved v4l2_open/v4l2_ioctl from NDAL_EX.
  Loader exits ZERO despite those errors. Current Makefile already links the full
  v4l2/v4lconvert closure for every vendor module; rebuilt artifacts pass strictly.
- Implemented `core/check.c`, main --check before host init. Reads ELF class/
  machine/byte order, compiled ABI note and exported entry without executing it;
  invokes own PT_INTERP loader with TRACE_LOADED_OBJECTS/WARN/BIND_NOW/VERBOSE in
  a sanitized environment, preserving optional LD_LIBRARY_PATH. Child deadline
  10 s. Parses diagnostics as well as exit code. No dlopen, constructors,
  module entry/init/start, host loop, socket, heartbeat or HAL calls.
- OA_MODULE_EXPORT emits the entry and retained .note.openaurora. All eleven
  production modules use it; ABI 2 layout itself unchanged. Legacy/no-note
  modules fail as unverifiable. Actual runtime descriptor validation remains.
- Strict full host suite passes plus runtime failure coverage: constructor
  avoidance/no socket, ABI/note/entry/ELF failures, unresolved function despite
  loader exit 0, missing library, wrong symbol version, empty/missing module dir.
  ARM build max GLIBC 2.17. Full --check passes under ARM qemu in read-only bwrap
  namespace with actual TV /lib, /etc/ld.so.cache and /vendor/lib. ARM good,
  undefined-symbol, old ABI, missing note and entry cases behave as expected.
  Plain qemu -L alone doesn't remap child execve's absolute interpreter; namespace
  is necessary. Initially omitted loader cache caused sqlite lookup failure;
  exact TV cache fixes test environment, not an added path assumption.
- **Real TV test PASS:** temporarily staged 401,200 bytes (core + eleven modules)
  in /tmp/openaurora-check.9xcvwi. Verified every file size+MD5 in a separate
  connection, then ran only --check. Core MD5 424ef6ea213cb706acb51a7adb39f778.
  All twelve artifacts OK, exit 0. Removed staged files and directories afterward
  using tvssh.sh; it reported already awake, so no WoL sent.
- Before/after: boot ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 unchanged, uptime
  2640.29→2709.33, vendor PIDs 1899+1737 unchanged; installed core MD5 remains
  a898384df076572cf377749f25427689. No native process left; oa_safe and
  coredumpnoreset present; oa_boot absent. No persistent binary/config changed.
- Report: RUNTIME-CHECK-2026-10-31.md; usage: openaurora/docs/RUNTIME-CHECK.md;
  raw logs in artifacts/analysis/runtime-check-2026-10-31/. The feature is in
  development build-tv-v2 and was tested from /tmp, not installed over ABI 1.

## 2026-10-31 — offline kernel OSD init follow-up

- Progress: decompiled KER_OSD_DrvInit, _ker_osd_fb_SetPar,
  _ker_osd_Set_BufPhyAddrSize and PreInitSetting; cross-checked registration
  relocations and physical binding instructions. No TV access or runtime edits.
- Driver init at .init.text+0x1904 (Ghidra 0x687b74) allocates/registers FIVE
  framebuffers. Corrected AGENTS claim that only Aurora creates them. Registration,
  node publication and visible presentation are separate facts.
- SetPar sets alpha blending=1, alpha=255, format, stride and rectangles; non-logo
  planes may receive it during driver init after successful CheckVar.
- Physical binding includes frame-order offsets, source origin and flip handling;
  overwrites callback pair word 1 with effective address. Do not equate every
  userspace/kernel similarly named structure. Separate-buffer=2 meaning and GEM
  double-size relation still open. Report OSD-KERNEL-INIT-2026-10-31.md and raw
  logs in artifacts/analysis/osd-2026-10-31. Next: callback construction and buffer
  allocation; no new ioctl/probe justified yet.

## 2026-10-31 — offline OSD callback/order continuation

- Resolved Get_DriverCallback to static gstDriverCallbackList via decompilation
  and .data relocations. Set_FbInfoPointer validates plane and stores kernel
  fb_info into gpstHwFbInfo; no userspace callback registration needed.
- Recovered ExchangeBufOrder transition table for selected frames 0/1/2 and
  previous groups 0..2,3..5,6..8,9. Values are stateful; physical binding consumes
  their frame groups. Existing kernel pan handler already implements this;
  do not duplicate guessed private enums in the userspace renderer.
- Separate-buffer nonzero parameter creates alternate MAU entries and size check;
  value 2 and selection/zero-buffer paths still need complete tracing. Updated
  OSD-KERNEL-INIT report, TODO and raw evidence. No TV access or runtime edits.

## 2026-10-31 — MAU selection and authorized read-only fb1 query

- Traced preferred/current/deferred-switch ioctl branches 0xbc/0xbd/0xbe and
  pan's alternate-memory selection. Exporter can combine physical regions but
  allocates state; diagnostic does not export or switch anything.
- fbregion now opens O_RDONLY and submits only variable/fixed screeninfo,
  current-MAU and region GETs. Wrapped host/ARM tests assert that exact read-only
  request set. New binaries only in build-strict/build-tv-v2, not installed.
- Owner said querying TV is fine. Staged helper /tmp, verified hash/size separately,
  queried fb1 once, removed staging. MAU=0 primary; base35bbb000, size16588800,
  stride7680, offset0,1080, 1920x1080 virtual1920x2160 BGRA; region
  {0,0,1920,1080,0,0,3840,2160}. This does not show a current bank mismatch and
  does not prove what happened during earlier takeovers. No setter/drawing run.
- Vendor PIDs1899/1737 and boot ebb0aaaa-72a2-4410-9f8d-95b9249f8a90 unchanged,
  uptime3321.11. Report OSD-MAU-SELECTION-2026-10-31.md and saved raw logs.
