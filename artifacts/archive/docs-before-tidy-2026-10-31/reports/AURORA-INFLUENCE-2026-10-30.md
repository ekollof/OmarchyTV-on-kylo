# Aurora influence map — 2026-10-30

`aurora.elf` is the TV's main UI/app process (`/vendor/com.nvt.idtv/aurora.elf`,
29 MB, ARM EABI5, glibc 2.23, stripped). Under our hook it runs as **root**
(`nvt.app.debug.ini` → `enableMultiuser=n`). It owns the DIAL command execution
path, the state WebSocket, DLNA, the browser orchestration, OTA, telemetry and
the on-screen UI.

This is a survey of **inputs we can influence** and what they reach — not a
memory-corruption audit.

---

## 1. Local files (`/conf`, read-write UBIFS — we control these)

| File | Effect | Status |
|---|---|---|
| `/conf/remote.txt` | appmgr → `debug.ini` → `run_aurora.sh`; `0:ip:u:p` runs `$REMOTELF/aurora.elf` from **/conf** as **root** | **in use** for sshd |
| `/conf/remotelf/aurora.elf` | the boot hook itself | in use |
| `/conf/browser/starturl.txt` | hotel/portal "dynamic URL" built at boot; browser opens it → JS in the TV browser | candidate |
| `/conf/browser/baseurl.txt` | portal base URL override (`novo_src/portal_controller`) | candidate |
| `/conf/diagnostic/diagnostic.txt` | remote-diagnostic settings (`https://remotediag.vestek.com.tr/…`) | candidate |
| `/conf/demo_commands.txt` | demo-mode command file (parser not yet located) | unverified |
| `/conf/dialsetting.ini` | DIAL identity (friendlyName, model, MAC, `alexa_enabled`) | read at boot |
| `/conf/hosts` | our telemetry blackhole (bind-mounted over `/etc/hosts`) | in use |
| `/conf/sqlite_db/settingsDB.db` | all user/privacy settings incl. `m_samba_acr_enabled`, `m_*privacy*`, `RemoteDiagnostic` | in use |

Shell exec found in this area:

```
sh  /mtd_apdat/script.txt &          # /mtd_apdat -> /conf (same volume)
```

That is a **local root shell hook** if the trigger can be found (r2 analysis
running; the string has no direct literal pointer, so the caller builds/computes
it). Even without the trigger it is worth knowing: `/conf/script.txt` is a
shell script the app is *designed* to run.

## 2. USB stick (`/media/sda1`, physical access)

| File | Effect |
|---|---|
| `keyscript.txt` | **IR simulator**: parses key/time records and injects remote key presses (`[IR_SIM] Send [%d] Key:0x%0x, delay:%d`) — scripted UI navigation |
| `board/` | if `/vendor/run.sh` is missing, the whole app runs from `sda1/board` (not our case, but a designed override) |
| `*profile*.bin`, `config_update.txt`, `IRsim*`, `ptf/ptq` files | service-menu / factory update ops (read into `/conf`, apply settings) |
| `logo.jpg`, `video` sources | media/bootlogo inputs |

## 3. Network

### 3.1 DIAL / SmartCenter — port 56789 (**currently disabled** by `debug.ini`)

The command table is large (~300 commands). The interesting ones:

```
SETURL / OPENURL     start a given URL and report page-load status
GETURL               current page URL
TVURL                start a URL via VODServiceManager
inject_ait_url       inject an AIT XML URL into the browser -> HbbTV app runs
SETUSERAGENT         override the portal user agent
SETCHANNEL, LISTPR, ADD_FAVOR, SETVOLUME, PICTUREMODE, …
ADD_RECORD / ADD_TIMER / startfollowtv / recliblist …
```

`inject_ait_url` and `SETURL/OPENURL` are the strongest: they make the TV's
browser fetch **arbitrary URLs** and (for AIT) run them as an HbbTV application,
which is JavaScript armed with the HbbTV + `SmartTvA_API` host objects. We used
the same class of primitive earlier (`browserseturl`) before DIAL was disabled.

### 3.2 State WebSocket — port 7681 (still listening)

Owned by aurora, served by `novo_src/virtualremotehandler` (DialController).
It carries `tv_state`, volume/channel events, `browserseturl`, and virtual-remote
key injection (`setVirtualRemoteKey`). Whether the full command set is reachable
without dialserver is **unverified** — worth testing when the TV is back.

### 3.3 Others

- **DLNA 2870** — SOAP/XML parsing from LAN.
- **Portal/CDN** (`orion.portaltv.tv`, Bunny CDN) — server-supplied JSON/XML/HTML;
  trust boundary is Vestel, not us.
- **OTA** — plain-HTTP probe to S3 + HTTPS `.dcf`.
- **Remote diagnostic** — `https://remotediag.vestek.com.tr/Diagnosticadmin/rdiagws.ashx?uuid=`,
  gated by profile flag; server URL is settable (`SETRDSERVERURL`), command set
  includes `PRINTALLPROFILE`, `COPYCONFPART`, `GETLOGGINGFILE`, `SETSDXDATA`.
- **Miracast/WiDi** — Wi-Fi only (this set is on eth0); historical Vestel
  injection bug (`';sh</mnt/hd0a/a &'`).
- **Bluetooth** (`bsa_server`) — BLE/AVRCP parsing, no shell sinks seen.
- **Alexa / Google Assistant** — built in but **not linked** (no certs in `/conf`).

## 4. Shell-command sinks (from `strings`, all with `%s`)

Roughly 40 templates. Grouped by the value they interpolate:

```
cp/mv/rm/chmod/ln ... %s ...          USB mount path, profile/log paths
wget -s %s/mb100_hwprofile.bin        server URL from profile/config
arping %s -w 10 -f -I %s | grep ...   diagnostic network test (IP/iface)
dd if=%s/test.file ...                USB speed test (mount path)
cat /proc/%s                          process name from code
echo 0 > /sys/bus/usb/devices/%s/...  USB device path
chmod 666 %s / chmod 777 -R /buffer   fixed paths
```

No `exec*`/`dlopen` with a config- or network-derived path was found; `dlopen`
references are system libraries only. The shell sinks are service/diagnostic
paths, not remotely reachable today.

## 5. Parsers (memory-safety surface, for completeness)

- **XML**: DIAL `dd.xml`, DLNA SOAP, HbbTV **AIT**, hotel channel tables, profiles.
- **JSON**: portal, ACR, Alexa/GA.
- **Binary**: EFSL profiles, PVR/media streams, JPEG/PNG (logo/EPG), SDX.

Given root is already ours, these matter only for a remote-attacker scenario.

## 6. Practical takeaways

- **Strongest local control:** `/conf/remote.txt` (root ELF at boot) — already used.
- **Browser control without code exec:** `starturl.txt` / `baseurl.txt` (local),
  or DIAL `SETURL`/`inject_ait_url` (network) if dialserver is restored.
- **Scripted UI control:** USB `keyscript.txt` (IR simulator) — useful for service
  menu automation when physically present.
- **Restoring DIAL** (via the `/vendor` overlay planned in `TODO.md`) would bring
  back the network command surface — and the telemetry that comes with it.

## 7. Open questions

- [ ] Trigger for `sh /mtd_apdat/script.txt &` (r2/r2ghidra analysis running).
- [ ] Does WS 7681 accept DIAL commands without dialserver?
- [ ] `demo_commands.txt` parser and whether it executes anything.
- [ ] Remote-diagnostic URL override + CA pinning (could expose the diagnostic
      command set locally).

---

# Ghidra findings (2026-10-30, second pass)

Full analysis of `aurora.elf` (Ghidra 12.1.2 headless, ARM little-endian, stripped).
Raw output kept in `artifacts/reports/ghidra/` (`ghidra_out.txt`, `ghidra_refs.txt`,
`ghidra_sinks.txt`, `ghidra_callers.txt`).

## 1. `/conf/script.txt` + `/conf/IRsim*` are DEAD CODE in 0.51.52.3

The handler is `FUN_0085e584` (0x85e584). At 0x85e7aa it does:

```
stat("/mtd_apdat/IRsim3")      -> if present:
  stat("/mtd_apdat/script.txt") -> if present:
      system("sh  /mtd_apdat/script.txt &")
```

That would be a root shell hook. It is **unreachable**:

- no `bl`/`b` to 0x85e584 anywhere,
- no function pointer to it in data,
- no `movw/movt` pair constructing its address,
- its strings (`/mtd_apdat/IRsim2|3|4`, `script.txt`, the `sh …` command) have
  no references outside that one function,
- `_parseKeyScript` (`FUN_0085e028`, the USB `keyscript.txt` IR simulator) is
  called only from inside that same unreachable function.

So the "script.txt hook" and the USB IR-simulator are compiled in but not wired
up in this build. (Strings survived the feature removal.)

## 2. Shell-command layer

All command execution funnels through four wrappers:

| wrapper | mechanism |
|---|---|
| `ice_system_command(char*)` | `execlp("/bin/sh","sh","-c",cmd)`, falls back to `system()` |
| `TD_OS_runShell(char*)` | `execl("/bin/sh","sh","-c",cmd)` |
| `PLAT_SYS_SystemCall(char*)` | `system(cmd)` |
| `ice_popen_command_func_1/2` | `popen(cmd, mode)` |

Callers: 86 functions (15 × popen, 4 × execl, 1 × execlp, 15+ × system).

## 3. Unquoted interpolation → shell injection (the interesting part)

`RobotestCommandProcessor::copyDirectory` (0xc2f88c) builds:

```
mkdir %s/%s  /  rm -rf %s/%s/*  /  cp -R %s/%s/* %s/%s
```

with `param_2[1]` taken **directly from the Robotest text protocol**, no quoting.
A line like `copydirectory 'x' 'a; reboot'` would execute arbitrary commands.
Reachable only through the Robotest server on **12345**, which is broken on this
firmware (its `select()` loop clears `robotester_running` and closes every
connection immediately) — so this is a *latent* RCE, not a live one.

Other unquoted builders (all fed by config/USB, not by remote input):

- `getUpdateFilesFromTFTP` → `ftpget %s %s/%s %s` (server/path parsed from arg; no callers found)
- `ICE_CopyChannelTableFromRemovableDevice`, `ICE_CopyDigitalIbToRemovableDevice`,
  `_fsUnzipFolder`, `CheckHotelUILogo` → USB mount path / filename into `cp`, `mkdir`, `rm`
- `ICE_InternetPing` → ping/arping with an IP argument

`getNetworkTime` only runs a fixed `mkdir -p /dev/shm/iplayer`.

## 4. What is actually live

- `/conf/remote.txt` → local root ELF at boot (ours, in use).
- `/conf/browser/starturl.txt` → 8 real references (`checkForStartUrlFile`,
  `getStartupURL`, `getRFHotelTVApplicationURL`, `launchHotelTVApp`, …), i.e. the
  boot URL override works.
- `/conf/demo_commands.txt` → `DemoApp::startDemoMode` (line parser, space-split args).
- DIAL command surface — disabled by `debug.ini`, restorable via overlay.
- WS 7681 — up, command surface still unverified.

## 5. Conclusion

Static analysis says: no hidden root-shell file hook besides the one we already
use (`remote.txt`). The `script.txt` path is a red herring in this firmware.
The shell layer is broad but driven by config/USB/service paths; the only
protocol-fed injection found (`copyDirectory`) sits behind a broken server.
