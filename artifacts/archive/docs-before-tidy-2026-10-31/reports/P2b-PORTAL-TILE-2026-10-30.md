# P2b — re-entry: the launcher as a native portal tile (2026-10-30)

The missing half of the loop: after `EXIT` hands the screen back to the stock TV, how do you
get the launcher again? Answer: **add it as a tile of the stock portal** ("Startpagina", the
screen you get with MENU). No patching, no key hijack.

## Where the tiles live

`/conf/sqlite_db/settingsDB.db`:

| table | contents |
|---|---|
| `LauncherItem` | one row per portal tile (`m_item_type=2` = application) |
| `LauncherItemId` | next-id counter |
| `PortalUrlItem` | service URLs (Gracenote, Foxxum search, speedtest, Rakuten, remote diagnostics) |

`LauncherItem` columns that matter (26 total: 16 integers + 10 blobs):

| column | meaning |
|---|---|
| `m_parent_id` | **1 = Home row**, 0 = Apps list (each app has both rows) |
| `m_order_index` | position in the row (`0` = first) |
| `m_original_id` | app id; also names the downloaded thumb `launcher_app_thumb<N>` |
| `m_item_type` | 2 = application |
| `m_application_name` | display name |
| `m_application_url` | **what the tile opens** |
| `m_application_thumb_url` | icon URL — the TV downloads it to `/buffer/downloaded_images/` |
| `m_menu_string_id` | label key (leave empty) |
| `m_is_featured` / `m_is_deleted` / `m_is_delete_disabled` | 1 / 0 / 1 |

## What was done

Backup first: `/conf/sqlite_db/settingsDB.db.pre-tile` (962,560 B).

Two rows inserted (Home + Apps) with

```
m_id = max+1 / max+2, m_parent_id = 1 / 0, m_item_type = 2,
m_order_index = 0, m_original_id = 900000, m_server_id = 1, m_is_featured = 1,
m_is_delete_disabled = 1, m_is_deleted = 0,
m_application_name = 'OmarchyTV',
m_application_url = 'http://127.0.0.1:8181/',
m_application_thumb_url = 'http://127.0.0.1:8181/icon.png'
```

then `sync` + reboot (the portal reads the DB at startup).

## Result (verified on the TV)

* The portal now shows **our icon as the first tile**, next to Netflix/Prime/YouTube.
* **OK on the tile opens the launcher** (`http://127.0.0.1:8181/` in the browser) — the
  re-entry works from the remote alone.
* The icon is downloaded by the TV itself from our httpd and cached in
  `/buffer/downloaded_images/launcher_app_thumb900`.
* Full cycle now: boot → launcher · `EXIT` → stock TV · `MENU` → portal · OK on the tile →
  launcher.

Screenshot: `artifacts/protocol/p2/02-portal-tile.png`.

## Gotchas found

0. **It takes two OK presses on a freshly entered portal**: the first focuses the tile, the
   second launches it (observed twice). Once the tile is focused, one press is enough.
1. **`m_original_id` 900 collides** with a real Vestel app id — the tile then showed the wrong
   caption ("amazon alexa"). Moved to `900000`. The caption *still* reads "amazon alexa" — the
   portal resolves the label from a cache we have not found; it is cosmetic only (the tile
   works). Changing `m_menu_string_id` to `''` did not change it.
2. The app **rewrites parts of the row** on boot (it filled `m_menu_string_id` = `AMAZON_ALEXA`
   and `m_application_thumb_file_name` = `launcher_app_thumb900`); our URL and icon URL survived.
3. The portal needs a **reboot** to re-read the table (no live refresh observed).
4. `LauncherItemId` holds the next id; new rows above the current max are accepted even if the
   counter is behind.

## Tooling

`tools/portal_tile.sh add|remove|status` does all of the above over ssh (backup, insert both
rows, or delete them), and `status` prints the rows + whether the icon is served/downloaded.

## Not covered (next)

* Live refresh without reboot (probably needs the launcher manager to re-scan; the stock UI has
  a "Verplaats/Wissen" menu for tiles, so there may be an internal refresh trigger).
* The correct caption/label field.
* Removing the tile from the flashable payload image (it is per-device state; the image built
  before this change does not contain it — rebuilds will unless the rows are excluded).
