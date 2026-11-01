#!/bin/sh
# openaurora lifecycle tests: load, start, reload (with state handoff), unload.
# Everything runs on the build host; the IR module degrades to "no device" here.
set -eu

cd "$(dirname "$0")/.."

BUILD=${BUILD:-build}
BIN=${BIN:-$BUILD/openaurora}
SOCK="/tmp/openaurora-test-$$.sock"
LOG="/tmp/openaurora-test-$$.log"
OUT="/tmp/openaurora-test-$$.out"
CFG="/tmp/openaurora-test-$$.cfg"
TDB="/tmp/openaurora-test-$$.db"
PID=
FAILS=0

# point settings at a copy of the settings DB captured from the TV, if we have one
rm -f "$CFG" "$TDB"
if [ -f ../artifacts/privacy/settingsDB.final.db ]; then
    cp ../artifacts/privacy/settingsDB.final.db "$TDB"
    printf 'settings.db = %s\n' "$TDB" > "$CFG"
else
    : > "$CFG"
fi

cleanup() {
    [ -n "$PID" ] && kill "$PID" 2>/dev/null || true
    [ -n "$PID" ] && wait "$PID" 2>/dev/null || true
    rm -f "$SOCK" "$LOG" "$OUT" "$CFG" "$TDB"
}
trap cleanup EXIT INT TERM

say() { printf '%s\n' "$*"; }
ok()   { printf '  ok   %s\n' "$*"; }
bad()  { printf '  FAIL %s\n' "$*"; FAILS=$((FAILS + 1)); }

expect() { # expect <description> <haystack> <needle>
    case "$2" in
    *"$3"*) ok "$1" ;;
    *) bad "$1 (wanted '$3', got: $(printf '%s' "$2" | tr '\n' '|'))" ;;
    esac
}

expect_missing() {
    case "$2" in
    *"$3"*) bad "$1 (should not contain '$3')" ;;
    *) ok "$1" ;;
    esac
}

ctrl() { python3 tests/client.py "$SOCK" "$@" 2>/dev/null; }
ctrl_retry() {
    lifecycle_attempt=0
    while [ "$lifecycle_attempt" -lt 100 ]; do
        lifecycle_reply=$(ctrl "$@")
        case "$lifecycle_reply" in
            ok*) printf '%s\n' "$lifecycle_reply"; return 0 ;;
        esac
        lifecycle_attempt=$((lifecycle_attempt + 1))
        sleep 0.01
    done
    printf '%s\n' "$lifecycle_reply"
}

say "starting $BIN"
"$BIN" --modules "$BUILD/modules" --sock "$SOCK" --config "$CFG" --log 3 \
       --load plat --load ticker --load ui_console --load panel --load power --load settings >"$OUT" 2>"$LOG" &
PID=$!

i=0
while [ ! -S "$SOCK" ]; do
    i=$((i + 1))
    if [ "$i" -gt 100 ]; then
        say "control socket never appeared"; sed -n '1,40p' "$LOG"; exit 1
    fi
    sleep 0.1
done

say "1) both modules started"
r=$(ctrl list)
expect "ticker started" "$r" "started"
expect "ui_console listed" "$r" "ui_console"

say "2) panel degrades gracefully without the vendor HAL"
r=$(ctrl list)
expect "panel started" "$r" "panel"
r=$(cat "$LOG")
expect "warned about missing HAL" "$r" "panel control disabled"

say "3) power refuses to suspend on a non-TV host"
r=$(ctrl "status power")
expect "power service published" "$r" "power:v1"
r=$(cat "$LOG")
expect "standby disabled for safety" "$r" "standby disabled"

say "4) settings reads the real database read-only"
r=$(ctrl "status settings")
expect "settings service published" "$r" "settings:v1"
expect "opened read-only" "$(cat "$LOG")" "open (read-only)"

say "5) clock service published"
r=$(ctrl "status ticker")
expect "clock service present" "$r" "clock:v1"

say "6) UI state survives a hot reload"
ctrl "loglevel 3" >/dev/null
ctrl "reload ui_console" >/dev/null
sleep 0.3
r=$(cat "$LOG")
expect "reload logged" "$r" "ui_console: reloaded"
expect "state handed over" "$r" "restored 1 bytes of state"

say "7) unload removes it from the table"
ctrl "unload ui_console" >/dev/null
r=$(ctrl list)
expect_missing "ui_console gone" "$r" "ui_console"

say "8) load + start again"
expect "load ok" "$(ctrl "load ui_console")" "ok"
expect "start ok" "$(ctrl "start ui_console")" "ok"
r=$(ctrl list)
expect "ui_console back" "$r" "ui_console"

say "9) stop/start a provider"
expect "stop ticker" "$(ctrl "stop ticker")" "ok"
expect "start ticker" "$(ctrl "start ticker")" "ok"

say "10) missing module fails cleanly"
expect "load fails" "$(ctrl "load does_not_exist")" "failed"

say "11) a module that requires a service is refused without it"
# display declares requires = { OA_SVC_PLAT }, and nothing here publishes plat, so a fresh host must
# refuse to start it. This used to assert nothing at all.
REQ_OUT=$(printf 'quit\n' | timeout 10 "$BIN" --modules "$BUILD/modules" --sock "" --config "$CFG" \
          --log 3 --load display 2>&1 || true)
expect "display refused without plat" "$REQ_OUT" "missing required service 'plat'"

say "12) cec and audio owners reload on the running host"
expect "plat already started" "$(ctrl "status plat")" "state   started"
expect "load audio" "$(ctrl "load audio")" "ok"
expect "start audio" "$(ctrl "start audio")" "ok"
expect "audio published" "$(ctrl "status audio")" "audio:v1"
expect "load cec" "$(ctrl "load cec")" "ok"
expect "start cec" "$(ctrl "start cec")" "ok"
expect "cec started" "$(ctrl "status cec")" "state   started"
expect "cec service" "$(ctrl "status cec")" "cec.owner:v1"
expect "reload cec" "$(ctrl_retry "reload cec")" "ok"
expect "cec still started" "$(ctrl "status cec")" "state   started"
expect "reload audio" "$(ctrl_retry "reload audio")" "ok"
expect "audio still started" "$(ctrl "status audio")" "state   started"
expect "host still lists power" "$(ctrl list)" "power"

say "13) suspend/resume hooks"
expect "manual quiescence hooks" "$(ctrl_retry suspend)" "ok"
expect "power is quiescent without entering RAM" "$(ctrl "status power")" "state   suspended"
expect "cec suspended" "$(ctrl "status cec")" "state   suspended"
expect "audio suspended" "$(ctrl "status audio")" "state   suspended"
expect "resume" "$(ctrl resume)" "ok"
expect "cec resumed" "$(ctrl "status cec")" "state   started"
expect "audio resumed" "$(ctrl "status audio")" "state   started"
expect "standby refuses disabled host power" "$(ctrl standby)" "standby failed"
expect "power reload keeps coordinated service available" "$(ctrl "reload power")" "ok"

say "14) clean shutdown"
ctrl "quit" >/dev/null || true
wait "$PID" 2>/dev/null && rc=0 || rc=$?
PID=
[ "$rc" = 0 ] && ok "exit code 0" || bad "exit code $rc"

if [ "$FAILS" -eq 0 ]; then
    say "all tests passed"
    exit 0
fi
say "$FAILS test(s) failed"
sed -n '1,60p' "$LOG"
exit 1
