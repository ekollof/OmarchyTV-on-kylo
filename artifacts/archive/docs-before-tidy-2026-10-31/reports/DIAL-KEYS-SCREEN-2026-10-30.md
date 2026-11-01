# DIAL standalone, virtual RC keymap, and screen capture (2026-10-30)

## 1. DIAL works without appmgr

`nvt.app.debug.ini` disables dialserver, which is why `56789/56790` were dead. The
binary runs fine on its own as root:

```
( cd /vendor && exec ./com.nvt.dial/com.nvt.dialserver ) &
```

Started from the boot hook (`/conf/remotelf/aurora.elf`, kill switch `/conf/no_dial`),
so DIAL is back on every boot. Verified after a reboot:

| check | result |
|---|---|
| `GET :56790/dd.xml` | 200, `Application-URL: http://<tv>:56789/apps/` |
| `POST :56789/apps/SmartCenter` (`<command>tvstate</command>`) | 201, WS 7681 pushes `<tv_state value='NOSIGNAL'/>` |
| `POST /apps/SmartCenter` `<remote><key code='1048'/></remote>` | 201, TV opens the launcher (screenshot) |
| `POST /apps/SmartCenter` `<browserseturl><load url=… page='RC'/>` | 201, TV fetches the URL (200 in the access log) |
| `POST /apps/YouTube` (`<launch>`) | 201 but nothing starts — YouTube is disabled in `debug.ini` |

Payload bodies are logged by dialserver into `/conf/tvhack.log`
(`Starting the app with params <...>`), which is a reliable "did it arrive" check that
does not depend on the screen.

Notes:
* `browserseturl` only has a visible effect while the idtv browser is up (portal/app
  state). In `NOSIGNAL` the fetch may still happen but nothing is displayed.
* The WS `7681` endpoint sends the current state once on connect and then only on
  change, so "no frames" is normal — do not read it as "command failed".
* An HTTP `201` is returned for any well-formed POST; it proves nothing on its own.

## 2. Virtual RC keymap — the real codes

The API codes are defined on the TV, not guessed:

* `/vendor/vk_keymapping_db.xml` — Virtual Remote codes (**these are what
  `<remote><key code=…/></remote>` takes**).
* `/vendor/ice_keymapping_db.xml` — internal ICE keypress codes (offset variants,
  e.g. `KeyZero` 512 vs 1000); same key set, used inside the app.
* `/vendor/dial_appname_to_prefixurl_list.xml` — the app names DIAL accepts
  (`YouTube`, `Netflix`, `vr`, BBC iPlayer, …).

Copies in `artifacts/protocol/`. Useful entries:

| key | code | key | code |
|---|---|---|---|
| KeyZero…KeyNine | 1000–1009 | KeyMenu | 1048 |
| KeyExit | 1010 | KeyExit2 | 1037 |
| KeyWide | 1011 | KeyPause | 1049 |
| KeyPower | 1012 | KeyYellow | 1050 |
| KeyMute | 1013 | KeyRecord | 1051 |
| KeyPreset | 1014 | KeyBlue | 1052 |
| KeyLang | 1015 | KeySelect (OK) | 1053 |
| KeyVolumeUp | 1016 | KeyGreen | 1054 |
| KeyVolumeDown | 1017 | KeyRed | 1055 |
| KeyInfo | 1018 | KeyAux (source) | 1056 |
| KeyDown | 1019 | KeyMediaBrowser | 1057 |
| KeyUp | 1020 | KeyPlaylist | 1059 |
| KeyLeft | 1021 | KeyMyButton | 1062 |
| KeyRight | 1022 | KeyNetflix | 1064 |
| KeyStop | 1024 | KeySearchCloud | 1065 |
| KeyPlay | 1025 | KeyMediaGuide | 1066 |
| KeyRewind | 1027 | KeySports | 1067 |
| KeyFforward | 1028 | KeyEpg | 1047 |
| KeySubtitle | 1031 | KeyChannels | 1045 |
| KeyProgramUp | 1032 | KeyFavourites | 1040 |
| KeyProgramDown | 1033 | KeyQuickMenu | 1043 |
| KeyPrevious | 1034 | KeyPipPap | 1046 |
| KeyDual | 1035 | KeySleep | 1042 |

Corrections to the earlier cheatsheet: **1010 is `KeyExit`, not "BACK"** (`KeyExit2` =
1037), and **1056 is `KeyAux`** (the source/AV key). The arrows were missing entirely:
`1019` down, `1020` up, `1021` left, `1022` right. OK is `KeySelect` 1053.

`Keyboard*` entries in the same file (8, 10, 32, 48–57, 65–90, 97–122, …) are the
text-entry codes — usable for on-screen keyboards via the same POST.

## 3. Screenshot over SSH (new oracle)

Nothing prevents root from reading the OSD layers, so the TV screen can be captured
with no on-screen UI at all:

| layer | name | geometry | bpp |
|---|---|---|---|
| `/dev/fb0` | OSD_NORMAL1 | 1920×3240 (3 × 1080 frames) | 32 |
| `/dev/fb1` | OSD_NORMAL2 | 1920×2160 (2 × 1080 frames) | 32 |
| `/dev/fb2` | OSD_NORMAL3 | 1280×1440 (2 × 720) | 16 |
| `/dev/fb3` | OSD_CURSOR1 | 256×512 | 32 |

Launcher, menus and dialogs are drawn into **fb1**, full-screen apps into fb0 (pixels
are RGBA, 7680 B stride). An all-zero layer means "nothing drawn on this layer now";
that is the normal state on the no-signal source, so blank ≠ broken.

Tool: `tools/tvshot.py` (wakes the TV, dumps a layer, writes PNG per frame, exit 2 if
all frames are blank).

```
tools/tvshot.py                             # fb1 -> /tmp/opencode/tvshot_0.png
tools/tvshot.py --layer 0 --frames 1 --out /tmp/opencode/app.png
```

This makes key/command injection verifiable without watching the TV: press a key,
capture, diff. Example (see `artifacts/protocol/shot-launcher*.png`):

* `1048` (MENU) → launcher ("Startpagina") on fb1
* `1022` (RIGHT) → focus moves from the sidebar tile to the Netflix tile
* `1048` from the tool tile → `Instellingen → Beeld` (contrast 95, brightness 50,
  sharpness 50, colour 60, energy saving off, zoom auto, virtual HDR on)
* `1037` (KeyExit2) twice → back to the (black) no-signal source
