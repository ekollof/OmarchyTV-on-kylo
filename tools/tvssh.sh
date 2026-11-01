#!/bin/sh
# ssh to the TV, waking it first if it is in standby.
# The plain `ssh` on this host has OnConnect hooks that break non-interactive
# commands, so this uses /usr/bin/ssh -F /dev/null.
#
#   tools/tvssh.sh 'uptime'
#   TV_HOST=192.168.178.87 TV_KEY=~/.ssh/id_ed25519 tools/tvssh.sh
set -e
HERE=$(dirname "$0")
TV_HOST=${TV_HOST:-192.168.178.87}
TV_KEY=${TV_KEY:-$HOME/.ssh/id_ed25519}
python3 "$HERE/ensure_awake.py" --host "$TV_HOST" ${TV_QUIET:+--quiet}
exec /usr/bin/ssh -F /dev/null -T \
    -i "$TV_KEY" -o IdentitiesOnly=yes \
    -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null \
    "root@$TV_HOST" "$@"
