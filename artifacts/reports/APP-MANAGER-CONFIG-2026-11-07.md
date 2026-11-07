# App-manager configuration and launch table — 2026-11-07

Status: static extraction from the shipped rootfs. No TV execution or writes.

## Sources

```text
vendor/data/nvt.app.ini         MD5 8ee5da7acf733fd2b768d2877b0ce9b5
vendor/data/nvt.app.debug.ini   MD5 6c96eac0bd48083c4e1601327f0696ed
                                 SHA256 b2278d90d95fdc2830880554c7f52917ac502978c645bbf65541cccae55fd3ed
vendor/data/nvt.app.nfs.ini     MD5 98d5ecc5e4335b60b3c7cd730540af37
```

The stock process observed earlier is:

```text
/vendor/com.nvt.appmgr/com.nvt.appmgr --applist=/vendor/data/nvt.app.debug.ini
```

so the **debug** file is the active profile on this unit. Parsed copies and
tables are in `artifacts/analysis/vendor-inventory-2026-11-07/config/`:
`nvt.app.ini`, `nvt.app.debug.ini`, `nvt.app.nfs.ini`,
`app-config-keys.tsv` (519 key/value rows) and `app-list.tsv` (30 app rows).

## Active-profile differences

`nvt.app.debug.ini` differs from the retail `nvt.app.ini`:

| key | retail | debug |
|---|---|---|
| `enableMultiuser` | y | n |
| `dialserver` | y | n |
| `netflix` | y | n |
| `amazon` | y | n |
| `youtube` | y | n |
| `youtube_test` | y | n |
| `idtv` program | `/mtd_apexe/board/com.nvt.idtv/aurora.elf` | `/mtd_apexe/board/run_aurora.sh` |

The debug profile is therefore an Aurora-only/engineering profile, not the
retail multi-app profile. The app manager still parses Netflix/Amazon/YouTube
sections, but they are disabled by `[apps]`.

## App table

`app-list.tsv` columns: file, section, enabled, program, layer, initialize/
switch-off/switch-on/power-off state, properties, repeat interval (µs), repeat
magnification. Retail `nvt.app.ini`:

```text
dialserver    y  com.nvt.dial/com.nvt.dialserver       BG/BG/BG/BG
idtv          y  com.nvt.idtv/aurora.elf       1280x720 FG/PAUSE//VOLCTRL|NODFB|IPCKEY
privserver    y  com.nvt.privserver/...                1/1/NODFB|IPCKEY
netflix       y  com.nvt.nflx/netflix          1280x720 PAUSE/PAUSE/FG/STOP_BY_SELF
amazon        y  com.nvt.amazon/run.sh         1920x1080 STOP/STOP_BY_SELF/FG/STOP_BY_SELF
ntts          n  com.nvt.ntts/nvttts                   PAUSE/BG/NOREG|BG|DELAY3|NODFB|DELAY
youtube       y  com.nvt.cobalt/run.sh         1280x720 STOP/STOP_BY_SELF/FG/STOP_BY_SELF
youtube_test  y  com.nvt.cobalt/run.sh         1280x720 STOP/STOP/FG/STOP
demo1/demo2   n  com.nvt.dfb_demo1/2           1280x720 STOP/STOP_BY_SELF
```

Key per-app properties:

```text
VOLCTRL     app receives volume keys
NODFB       app does not own DirectFB surface
IPCKEY      app receives IPC key events
SUSPEND     app is involved in suspend
BOOT1       launch at boot stage 1
DELAY10     10 s delayed launch
LOGO        app may show the boot logo
SENDSTATE   app receives state changes
CLEAROSD    clear OSD on switch
RP/RE       power/resume related
NOREG       not registered in the app list
```

YouTube sections additionally set `repeatkey_delayinterval=19000` µs and
`repeat_magnification=4`; this is app-level key-repeat policy, separate from
the BLAST IR `aux=01` repeat marker.

## Hotkeys and global config

```text
[hotkey]
f1=idtv
f2=netflix
f3=demo1
f4=
```

`[config]` keys include `delay_launch=6`, `wait_for_idtv=y`,
`logo_on_layer=y`, `DFB_pre_allocate=y`, `welcome=n`. The debug profile changes
only the app enable/program fields, not the hotkey table.

## Limits

- The initial app manager is `privserver`; `idtv` is the Aurora app. The
  replacement must preserve the app-manager contract if it continues to be
  launched as `idtv` (program name, IPC keys, layer/Dfb properties).
- This is config data, not proof that the debug profile is the right profile
  for a production replacement; the owner may prefer the retail profile.
- No app launch, IPC, DirectFB or TV behavior was exercised.
- MD5s are in `elf/elf-census.tsv`; config copies are committed as text.

Report: APP-MANAGER-CONFIG-2026-11-07. Related: APP-MANAGER-KEYBOARD.
