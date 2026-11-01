#!/bin/sh
# /cgi-bin/net.js - the TV's real network identity, as a tiny script loaded before app.js.
# Upstream prints a hardcoded "Wi-Fi" in the terminal panel; this set is wired.
echo "Content-Type: application/javascript"
echo "Cache-Control: no-store"
echo

IFACE=""
for i in eth0 eth1 wlan0; do
  if ifconfig "$i" >/dev/null 2>&1; then IFACE="$i"; break; fi
done

IP=""
if [ -n "$IFACE" ]; then
  IP=$(ifconfig "$IFACE" 2>/dev/null | awk '/inet addr:/{sub("addr:","",$2); print $2; exit}')
  [ -z "$IP" ] && IP=$(ifconfig "$IFACE" 2>/dev/null | awk '/inet /{print $2; exit}')
fi

LINK="offline"
[ -n "$IP" ] && LINK="up"

printf 'window.OMARCHY_NET_NAME = "%s";\n' \
  "$(printf '%s · %s %s' "${IFACE:-net}" "${IP:-no address}" "$LINK" | sed 's/"/\\"/g')"
