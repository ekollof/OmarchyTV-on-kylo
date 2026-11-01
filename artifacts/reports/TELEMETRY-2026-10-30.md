# TV telemetry / egress report — TELEFUNKEN D65U546A (Vestel MB230, 0.51.52.3)

> **Status: Dated pre-remediation egress/consent survey. Current privacy protections are in STABILITY/MEMORY. Telemetry/advertising are excluded from openaurora.** See [report index](README.md) and
> [current corrections](../../FINDINGS.md); old plans do not authorize TV changes.

Date: 2026-10-30
Target: `192.168.178.87` (MAC `70:54:b4:fe:70:c6`)
Access: root shell on the TV (`ssh root@…`) + static analysis of the dumped firmware.

## TL;DR

The TV does emit telemetry. Three independent pipelines are active:

1. **Vestel product telemetry** (`plog.vstlsrv.com`) — device, storage, operator, privacy and usage data, on a 24 h polling interval, plus event-driven logs.
2. **Vestel portal** (`orion.portaltv.tv`) — app list, notifications and **app-click tracking**, all keyed on the **MAC address**.
3. **Search backends** — every global-search query is sent to a third-party host (`7345023508.fxm9485766783.com`, Hetzner) **with the TV's MAC**, and to Google CSE / YouTube Data API using API keys embedded in the firmware.

A fourth pipeline, **Samba TV ACR** (automatic content recognition), is installed and connects out, but is currently **disabled** on this set.

Identifiers in play: Ethernet MAC, advertising ID, a pseudonymous user ID, a Gracenote ID and the DIAL UUID. Marketing profiling and third-party data sharing are **switched on** in the settings database.

---

## 1. Method

- Live: `netstat -tunap` on the TV, repeated sampling of `/proc`-backed sockets over ~100 s, process attribution.
- Static: URL format strings, payload field names and API keys extracted from `aurora.elf`, `cidclient`, `bsa_server`.
- Config/state: `/conf`, `/buffer/samba`, and `settingsDB.db` (pulled to the host and queried with sqlite3).

TLS payloads were **not** decrypted; field names below come from the binaries and from device state files, which is reliable for "what is designed to be sent".

## 2. Endpoints actually contacted (verified live)

| Destination | Host / owner | Process | Port | Purpose |
|---|---|---|---|---|
| `54.171.187.177` | `plog.vstlsrv.com` — Vestel | `aurora.elf` | 443 | product telemetry ("plog") |
| `52.209.152.10`, `34.242.168.133`, `34.245.121.149` | `orion.portaltv.tv` — Vestel (AWS eu-west-1) | `aurora.elf` | 443 | portal apps / notifications / app clicks |
| `88.99.123.80` | `7345023508.fxm9485766783.com` — Hetzner | `aurora.elf` | 443 | video search backend |
| `216.183.117.106` | **Samba TV, Inc.** | `cidclient` | 443 | ACR / CID bootstrap (`fsui.cid.samba.tv`, `flingo.tv`) |
| `3.5.75.91` | AWS S3 (eu-west-1) | `aurora.elf` (attrib. `bsa_server`) | **80 (plain HTTP)** | OTA probe `…/vstlupdate/Essentials/2kb.txt` |
| `2400:52e0:1e01::1444:1` | Bunny CDN (BunnyWay) | `aurora.elf` | 443 | portal/CDN assets |

These are long-lived keep-alive TLS connections — they show up on every sample, so "the TV looks idle" says nothing about egress.

Resolv.conf: `nameserver 192.168.178.1` (router), so DNS itself leaks hostnames to the FritzBox.

## 3. What is sent

### 3.1 Vestel product telemetry — `plog.vstlsrv.com`

Endpoint paths present in `aurora.elf`:

```
/setdevicedata      /setdevicestorageinfo   /setadditionalfeature
/setoperatordata    /setfvpdata             /setprivacydata
/setrecoverydata    /setdiagnosticmenukeys
```

Event/log categories compiled into the same component:

```
heartbeat, device_power_up, home_launcher, menu_usage, rc_usage,
search_usage, pvr_usage, channel_info, external_source_info,
channel_list_automatic_triggers, channel_list_user_triggers,
picture_settings, sound_settings, wifi_v2, network_discovery,
network_speed, internet_outage, ai_on_tv_activity, ai_on_tv_settings,
user_identity
```

Interpretation: the TV reports what you watch (channel/source), how you use it (menus, remote, search, recordings), your picture/sound settings, Wi-Fi/network details, storage state, recovery events, and encrypted "user identity" data.

Polling config (from `settingsDB.db`, `LoggingConfig`): `m_polling_interval_hrs = 24`, with "always logging" counters, i.e. a daily upload plus event-driven reports.

### 3.2 Vestel portal — `orion.portaltv.tv`

Format strings:

```
https://orion.portaltv.tv/v1/apps?macaddr=%s
https://orion.portaltv.tv/v1/appclicks?country=%s&brand=%s&device=%s&swversion=%s&macaddr=%s
https://orion.portaltv.tv/v1/notifications?country=%s&brand=%s&device=%s&swversion=%s&lang=%s&notifno=%d&macaddr=%s
```

So the portal knows the device by **MAC**, plus country, brand, model, firmware version and language, and receives an **app-click stream**.

### 3.3 Global search — two third parties

```
https://7345023508.fxm9485766783.com/api/v1/vestel/api/search/video?
        q=%s&country=%s&brand=%s&device=%s&swversion=%s&macaddr=%s&limit=%s
https://www.googleapis.com/customsearch/v1?key=AIzaSy…&cx=…&q=%s&gl=%s&num=%d
https://www.googleapis.com/youtube/v3/search?q=%s&key=AIzaSy…&part=snippet&chart=mostPopular&regionCode=%s&maxResults=%d
```

Every search string is sent to the Hetzner-hosted Vestel backend **tagged with the TV's MAC**, and to Google (CSE + YouTube Data API) with the TV's embedded API keys.

### 3.4 "User identity" message (built but only sent when voice assistants are linked)

```
publishMessageToUserIdentityTopic
  user_identity_id=%s serial_no=%s cognito_id=%s
  eth_mac_adress=%s wifi_mac_address=%s bluetooth_bd_addr=%s
  ref_code=%s tv_mktg_consent=%d privacy_version=%s
  privacy_data_share_3rd_party_item:%d data_share_version:%s
  advertising_id:%s is_limited_ad_tracking:%d ads_storage:%d
```

Transport when active: AWS IoT MQTT (`aa8u5ipzaqyfu-ats.iot.eu-central-1.amazonaws.com`, `…eu-west-1…`), topics `Vestel/AA/TV`, `Vestel/prod/AA/…`; plus Google Assistant endpoints `google-ast-prod.homevsmart.com` (with `x-api-key`) and Amazon Alexa endpoints.

On this set the assistants are **not linked** (`/conf/aws_iot_certs` and `/conf/ga_*.pem` absent, `AlexaReadyTable.m_fti_flag = 0`), so this message should not currently be emitted. It remains one settings-toggle away.

### 3.5 OTA

The update client probes `http://s3-eu-west-1.amazonaws.com/vstlupdate/Essentials/2kb.txt` over **plaintext HTTP**, and the device's own OTA descriptor is `0009df/7706.dcf` on `s3-eu-west-1.amazonaws.com/vstlupdate` (fetched over HTTPS, signature-checked).

## 4. Identifiers present on the device

| Identifier | Value | Where |
|---|---|---|
| Ethernet MAC | `70:54:b4:fe:70:c6` | `/sys/class/net/eth0/address`, `/conf/mac.txt`, all portal/search URLs |
| Wi-Fi MAC | `70:54:b4:fd:64:1c` (from DIAL desc) | plain in `dd.xml`, `user_identity` message |
| Advertising ID | `71613941-14eb-49f5-a310-a3505c0867f8` | `settingsDB.AdvertisingID` |
| Pseudonymous user ID | `u22162473093285252` | `settingsDB.Half_Private_User_Id` |
| Gracenote ID | `49081922121839746-0688B3D8675C4E852C7B21CCD1FB43FE` | `/conf/gracenote_user_id` |
| DIAL UUID | `uuid:d3f5ce9e-8215-416b-a277-a52843a0b99e` | `dd.xml` |
| Serial number | `8368599 900143` | label + `user_identity` message field `serial_no` |

## 5. Privacy / consent state (from `settingsDB.db`)

`Privacy_Last_Logged_Items` — **to_log** flags:

```
connection=1  cookies=1  hbbtv=1  portal=1  samba=0  update=1
```

`Privacy_Advertising_Last_Logged_Item`:

```
advertising_enable=1   ads_storage_enable=1
```

`Privacy_Mkt_Prof_Last_Logged_Item`:

```
marketing_profile_enable=1   third_party_data_sharing_enable=1
privacy_policy_version=0.1.0.10
```

So: **marketing profiling and third-party sharing are ON, advertising is ON, ACR (Samba) is OFF.**

ACR state confirmed live:

```
/buffer/samba/sambatvCid.json → {"enabled": false, "ts_s": …}
http://localhost:8080/acr_enabled → {"enabled": false, "explicit": true}
```

Note the ACR client still opens a TLS session to Samba TV (216.183.117.106) for CID bootstrap/keepalive even while disabled.

## 6. What is NOT happening (good news)

- No ACR content fingerprinting/upload: disabled both by the TV setting and by the stored ACR config.
- Alexa / Google Assistant not linked → no `user_identity` MQTT publication, no serial-number upload via that path, no Amazon/Google account tokens on the device.
- Amazon DRS (dash replenishment) tables exist but are unprovisioned.
- No plaintext credentials seen in traffic; everything except the OTA probe is TLS.

## 7. Assessment

- The TV maintains **persistent outbound TLS sessions** to Vestel (portal + telemetry), a third-party search host, Samba TV and a CDN, and performs a **plaintext HTTP OTA probe** at boot.
- **Viewing metadata is not being fingerprint-uploaded** (ACR off), but **usage metadata is**: channel/source, settings, search terms, app clicks, network/storage state, plus a stable device identity (MAC, ad ID, pseudonymous user ID).
- The most privacy-relevant single item is the **global search** path: free-text queries leave the TV together with the MAC address, to a host that is neither Vestel-branded nor documented in the UI.
- Consent flags in the database indicate marketing profiling and third-party sharing are enabled; those are the switches to flip if you want to reduce this at the source.

## 8. How to stop or reduce it

Options, in increasing order of thoroughness:

1. **On the TV** — disable marketing/third-party sharing and advertising in the privacy/settings menu (the DB flags above show they are on). ACR is already off.
2. **In the hook we already control** — add DNS/firewall blocks at boot, e.g. point these hostnames at `0.0.0.0` via `/etc/hosts` (bind-mounted like `/etc/passwd`) or install iptables rules before the app starts. Hosts worth blocking:

```
plog.vstlsrv.com
orion.portaltv.tv  alpha./beta./andromeda.portaltv.tv
vstlsrv.com  portaltv.tv
7345023508.fxm9485766783.com
fsui.cid.samba.tv  flingo.tv
sh.vestelsmarthome.com  homevsmart.com
aa8u5ipzaqyfu-ats.iot.eu-central-1.amazonaws.com  *.iot.eu-west-1.amazonaws.com
```

3. **On the router** — block those domains for the TV's IP; keeps the TV's DNS clean and is independent of the TV firmware.

## 9. Reproduce

```sh
# live endpoints
ssh root@192.168.178.87 'netstat -tunap'
# ACR state
ssh root@192.168.178.87 'cat /buffer/samba/sambatvCid.json; wget -qO- http://localhost:8080/acr_enabled'
# consent flags
ssh root@192.168.178.87 'cat /conf/sqlite_db/settingsDB.db' > settingsDB.db
sqlite3 settingsDB.db 'select * from Privacy_Mkt_Prof_Last_Logged_Item;'
# endpoint strings
strings artifacts/dumps/tv_ap0/com.nvt.idtv/aurora.elf | grep -E 'plog|orion|macaddr'
```

## 10. Caveats

- TLS payloads were not decrypted; field names come from firmware strings and on-device state, not from captured bodies.
- DNS queries go to the FritzBox and were not captured here; hostnames above are inferred from the firmware's URL constants and matched by resolving them.
- The `3.5.75.91:80` socket was attributed to `bsa_server` by busybox netstat; the URL in the boot log (`…/vstlupdate/Essentials/2kb.txt`) and the AWS S3 range identify it as the OTA probe.
