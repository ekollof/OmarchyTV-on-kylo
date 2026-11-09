#!/bin/sh
# Boot-attempt gate for the opt-in openaurora boot.
#
# The persistent hook calls `prepare` before starting the replacement and
# `start STAGE` immediately before exec. A failed or hung boot cannot clear
# its token, so the next boot (including one caused by the MCU watchdog)
# counts it and, after three unproven attempts, removes the enable file.
# oa-supervise calls the equivalent of `clear STAGE` once the child has been
# stable for its stable window.
#
# STATE_DIR is the directory holding oa_boot/oa_safe/oa_try/...: "/conf" on
# the set, a temporary directory in host tests.
#
# Usage: oa-boot-gate.sh STATE_DIR prepare
#        oa-boot-gate.sh STATE_DIR start STAGE
#        oa-boot-gate.sh STATE_DIR clear STAGE
#        oa-boot-gate.sh STATE_DIR status
set -u

conf=${1:-/conf}
cmd=${2:-status}
boot=$conf/oa_boot
safe=$conf/oa_safe
bootfail=$conf/oa_bootfail
try=$conf/oa_try
last=$conf/oa_last_ok
stage_file=$conf/oa_stage
max=3

read_count()
{
    n=$(cat "$1" 2>/dev/null || echo 0)
    case $n in
    ''|*[!0-9]*) echo 0 ;;
    *) echo "$n" ;;
    esac
}

write_count()
{
    printf '%s\n' "$2" >"$1" 2>/dev/null || true
}

case $cmd in
prepare)
    n=$(read_count "$bootfail")
    if [ -f "$try" ]; then
        n=$((n + 1))
        write_count "$bootfail" "$n"
        rm -f "$try"
        echo "oa-boot-gate: previous native boot did not reach stability ($n/$max)"
    fi
    if [ "$n" -ge "$max" ] && [ -f "$boot" ]; then
        rm -f "$boot"
        echo "oa-boot-gate: removing $boot after $n unproven boots" >&2
    fi
    if [ -f "$safe" ] || [ ! -f "$boot" ]; then
        echo "oa-boot-gate: native blocked (safe=$([ -f "$safe" ] && echo 1 || echo 0) boot=$([ -f "$boot" ] && echo 1 || echo 0) fail=$n)"
        exit 1
    fi
    echo "oa-boot-gate: native allowed (fail=$n)"
    exit 0
    ;;
start)
    stage=${3:-0}
    case $stage in
    0|1|2|3|4) ;;
    *) echo "oa-boot-gate: invalid stage '$stage'" >&2; exit 2 ;;
    esac
    printf 'stage=%s\n' "$stage" >"$try" 2>/dev/null || true
    printf '%s\n' "$stage" >"$stage_file" 2>/dev/null || true
    echo "oa-boot-gate: attempt started (stage=$stage)"
    exit 0
    ;;
clear)
    stage=${3:-0}
    rm -f "$try"
    write_count "$bootfail" 0
    printf 'stage=%s time=%s\n' "$stage" "$(date +%s 2>/dev/null || echo unknown)" >"$last" 2>/dev/null || true
    echo "oa-boot-gate: cleared (stage=$stage)"
    exit 0
    ;;
status)
    echo "boot=$([ -f "$boot" ] && echo 1 || echo 0) safe=$([ -f "$safe" ] && echo 1 || echo 0) fail=$(read_count "$bootfail") try=$([ -f "$try" ] && echo 1 || echo 0) stage=$(cat "$stage_file" 2>/dev/null || echo -)"
    exit 0
    ;;
*)
    echo "usage: $0 STATE_DIR prepare|start STAGE|clear STAGE|status" >&2
    exit 2
    ;;
esac
