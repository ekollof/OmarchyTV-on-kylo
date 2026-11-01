#!/bin/sh
# Drive the offline TV simulator (tools/tvsim.py).
#
#   tools/tvsimctl.sh key 1019          # send a remote key (DIAL code)
#   tools/tvsimctl.sh load [url]        # what browserseturl does (default: the launcher)
#   tools/tvsimctl.sh shot [file]       # screenshot -> file (1920x1080, like the TV's fb0)
#   tools/tvsimctl.sh state             # PORTAL / SOURCE / STANDBY
#   tools/tvsimctl.sh dial '<xml>'      # POST a raw DIAL payload, e.g. "<remote><key code='1048'/></remote>"
SIM=${SIM:-127.0.0.1:8181}
URL=${LAUNCHER_URL:-http://127.0.0.1:8181/omarchy/}

case "$1" in
  key)
    curl -s -m5 "http://$SIM/sim/key?code=$2"; echo ;;
  load)
    curl -s -m5 --get --data-urlencode "url=${2:-$URL}" "http://$SIM/sim/load"; echo ;;
  shot)
    curl -s -m20 --get --data-urlencode "path=${2:-/tmp/opencode/tvsim.png}" "http://$SIM/sim/shot"; echo ;;
  state)
    curl -s -m5 "http://$SIM/sim/state"; echo ;;
  dial)
    curl -s -m5 -X POST -H "Content-Type: text/plain; charset=ISO-8859-1" \
      -H "application_name: tv smart centre" --data "$2" -o /dev/null -w '%{http_code}\n' \
      "http://$SIM/apps/SmartCenter" ;;
  *)
    sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 1 ;;
esac
