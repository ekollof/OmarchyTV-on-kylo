#!/bin/sh
# Add / remove the "OmarchyTV" tile in the stock portal (Startpagina) on the TV.
#
#   tools/portal_tile.sh add|remove|status
#
# The portal tiles live in the settings DB: LauncherItem (m_item_type=2 = application).
# Two rows per app: m_parent_id=1 (Home row) and m_parent_id=0 (Apps list).
#   m_application_url            -> page the tile opens (our launcher)
#   m_application_thumb_url      -> icon; the TV downloads it to
#                                   /buffer/downloaded_images/launcher_app_thumb<N>
#   m_order_index=0              -> first position
#   m_original_id=900000         -> high, unused id (900 collides with a real app,
#                                   which is how we got a wrong caption)
#
# A backup of the DB is taken before the first change: settingsDB.db.pre-tile
TV=${TV:-192.168.178.87}
URL=${LAUNCHER_URL:-http://127.0.0.1:8181/}
ICON=${LAUNCHER_ICON:-http://127.0.0.1:8181/icon.png}
HERE=$(dirname "$0")

tv() { "$HERE/tvssh.sh" "$@" 2>/dev/null; }

case "$1" in
  add)
    tv "db=/conf/sqlite_db/settingsDB.db
        [ -f \$db.pre-tile ] || cp -f \$db \$db.pre-tile
        id=\$(/conf/sqlite3 \$db 'select coalesce(max(m_id),0)+1 from LauncherItem;')
        COLS='m_application_image_crc,m_application_thumb_update_time,m_application_update_time,m_channel_handle,m_id,m_is_delete_disabled,m_is_deleted,m_is_featured,m_is_moved_by_user,m_is_out_of_sync,m_item_type,m_menu_return_id,m_order_index,m_original_id,m_parent_id,m_server_id,m_application_ad_image_url,m_application_ad_url,m_application_name,m_application_thumb_file_name,m_application_thumb_url,m_application_url,m_fvp_application_id,m_menu_string_id,m_recommendation_url,m_application_ad_tracking_url'
        /conf/sqlite3 \$db \"insert into LauncherItem (\$COLS) values (0,0,0,-1,\$id,1,0,1,0,0,2,-1,0,900000,1,1,'','','OmarchyTV','','$ICON','$URL','','','','');\"
        id2=\$((id+1))
        /conf/sqlite3 \$db \"insert into LauncherItem (\$COLS) values (0,0,0,-1,\$id2,1,0,1,0,0,2,-1,0,900000,0,1,'','','OmarchyTV','','$ICON','$URL','','','','');\"
        /conf/sqlite3 \$db 'select m_id,m_parent_id from LauncherItem where m_original_id=900000;'
        sync; echo 'reboot needed for the portal to re-read the DB'"
    ;;
  remove)
    tv "db=/conf/sqlite_db/settingsDB.db
        /conf/sqlite3 \$db \"delete from LauncherItem where m_original_id=900000;\"
        echo 'removed'; sync"
    ;;
  status)
    tv "db=/conf/sqlite_db/settingsDB.db
        /conf/sqlite3 -header -column \$db \"select m_id,m_parent_id,m_order_index,replace(cast(m_application_url as text),char(0),'') as url from LauncherItem where m_original_id=900000;\"
        echo -n 'icon served: '; wget -qO- -T 3 http://127.0.0.1:8181/icon.png 2>/dev/null | wc -c
        t=\$(/conf/sqlite3 \$db \"select replace(cast(m_application_thumb_file_name as text),char(0),'') from LauncherItem where m_original_id=900000 limit 1;\" 2>/dev/null)
        echo -n 'downloaded:  '; [ -n \"\$t\" ] && ls -l /buffer/downloaded_images/\$t 2>/dev/null || echo none"
    ;;
  *)
    echo "usage: $0 add|remove|status" >&2; exit 1 ;;
esac
