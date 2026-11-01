#!/bin/sh
# Open / close the web launcher on the TV (P2).
#
#   tools/launcher.sh open     # browserseturl -> http://127.0.0.1:8181/
#   tools/launcher.sh close    # EXIT key -> back to the stock TV UI
#   tools/launcher.sh status   # is the launcher enabled / served / on screen?
#
# The TV must be awake (tools/tvssh.sh wakes it). Needs no ssh for open/close.
TV=${TV:-192.168.178.87}

post() {
  curl -s -m 5 -X POST \
    -H "Content-Type: text/plain; charset=ISO-8859-1" \
    -H "application_name: tv smart centre" \
    --data "$1" -o /dev/null -w '' "http://$TV:56789/apps/SmartCenter"
}

case "$1" in
  open)
    # the browser only accepts browserseturl while the app's portal is up: if the TV is on
    # a source, EXIT first is harmless
    post "<?xml version='1.0' ?><browserseturl><load url='http://127.0.0.1:8181/?manual=1' page='RC'/></browserseturl>"
    echo "launcher requested on http://$TV (http://127.0.0.1:8181/)"
    ;;
  sleep)
    post "<?xml version='1.0' ?><remote><key code='1012'/></remote>"
    echo "suspend requested (power key) - the TV sleeps; the launcher reopens on wake"
    ;;
  close)
    post "<?xml version='1.0' ?><remote><key code='1037'/></remote>"
    echo "EXIT sent (1037) - stock UI"
    ;;
  status)
    echo -n "8181: "; (exec 3<>/dev/tcp/$TV/8181) 2>/dev/null && echo "serving" || echo "not listening"
    echo -n "56789: "; (exec 3<>/dev/tcp/$TV/56789) 2>/dev/null && echo "DIAL up" || echo "DIAL down"
    HERE=$(dirname "$0")
    "$HERE/tvssh.sh" 'echo -n "  flag: "; [ -f /conf/launcher_enable ] && echo enabled || echo disabled;
                      echo -n "  success marker: "; cat /conf/launcher_ok 2>/dev/null || echo none;
                      echo -n "  failure streak: "; cat /conf/launcher_fail 2>/dev/null || echo 0;
                      echo -n "  httpd: "; pidof httpd 2>/dev/null || echo "not running";
                      grep -a launcher /conf/tvhack.log | tail -3' 2>/dev/null
    ;;
  *)
    echo "usage: $0 open|close|sleep|status" >&2
    exit 1
    ;;
esac
