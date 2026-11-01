#!/bin/sh
# Explicit integration test: real host process/control socket, recorder display.
set -eu
[ "$#" -eq 2 ] || exit 2
command -v socat >/dev/null
core=$1
modules=$2
work=$(mktemp -d /tmp/oa-skin-socket.XXXXXX)
child=
cleanup() {
    if [ -n "$child" ]; then kill "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi
    rm -rf "$work"
}
trap cleanup EXIT HUP INT TERM
request() { printf '%s\n' "$1" | socat -T 2 - "UNIX-CONNECT:$work/control.sock"; }
await_ready() {
    tries=0
    while [ "$tries" -lt 200 ]; do
        reply=$(request skin)
        case "$reply" in *"active $1 state ready fallback $2"*) printf '%s\n' "$reply"; return;; esac
        tries=$((tries+1)); sleep 0.01
    done
    cat "$work/host.log" >&2; exit 1
}
for configured in missing ../invalid; do
    rm -f "$work/control.sock"
    printf 'autoload=nav_display,ui_engine\nui.skin=%s\nui.ok_file=\nui.fps=0\nui.skin_timeout_ms=100\n' "$configured" > "$work/ui.cfg"
    "$core" --modules "$modules" --config "$work/ui.cfg" --sock "$work/control.sock" --log 0 < /dev/null > "$work/host.log" 2>&1 &
    child=$!
    tries=0
    while [ ! -S "$work/control.sock" ]; do
        tries=$((tries+1)); [ "$tries" -lt 200 ] || exit 1; sleep 0.01
    done
    await_ready vendor 1
    request 'skin omarchy'; await_ready omarchy 0
    request 'skin vendor'; await_ready vendor 0
    request 'skin badenter'; await_ready vendor 1
    request quit
    wait "$child"; child=
done
printf '  ok   Unix socket startup fallback, runtime switching, enter failure and clean process shutdown\n'
