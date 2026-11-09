#!/bin/sh
# Prepare or apply the staged cold-boot deployment on the set.
#
# Dry-run by default: prints every change and touches nothing. --apply performs
# the deployment. --restore only re-enables the stock path (oa_safe) and
# disables the native gate; it does not need a bundle.
#
# Deployment installs the bundle at /buffer/openaurora, installs the boot-hook
# template, selects /conf/oa_stage, creates /conf/oa_boot and removes
# /conf/oa_safe. The hook and bundle are backed up first. The set does not
# switch until the next power-on.
#
# OA_ROOT prefixes all paths for host tests (empty on the set).
#
# Usage:
#   deploy-staged-cold.sh --bundle DIR --stage N [--apply]
#   deploy-staged-cold.sh --restore [--apply]
#   deploy-staged-cold.sh --help
set -eu

R=${OA_ROOT:-}
conf=$R/conf
buffer=$R/buffer
install_dir=$buffer/openaurora
hook=$conf/remotelf/aurora.elf
ts=$(date +%Y%m%d-%H%M%S 2>/dev/null || echo unknown)

bundle=
stage=
apply=0
restore=0

usage()
{
    sed -n '2,22p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-2}"
}

while [ $# -gt 0 ]; do
    case $1 in
    --bundle)
        [ $# -ge 2 ] || usage
        bundle=$2
        shift 2
        ;;
    --stage)
        [ $# -ge 2 ] || usage
        stage=$2
        shift 2
        ;;
    --apply)
        apply=1
        shift
        ;;
    --restore)
        restore=1
        shift
        ;;
    --help|-h)
        usage 0
        ;;
    *)
        echo "unknown argument: $1" >&2
        usage
        ;;
    esac
done

run()
{
    if [ "$apply" = 1 ]; then
        "$@"
    else
        printf 'would:'
        printf ' %s' "$@"
        printf '\n'
    fi
}

require_recovery()
{
    [ -f "$conf/coredumpnoreset" ] || {
        echo "refusing: $conf/coredumpnoreset is missing" >&2
        exit 1
    }
    [ -x "$buffer/wdtctl" ] || {
        echo "refusing: $buffer/wdtctl is missing or not executable" >&2
        exit 1
    }
}

restore_stock()
{
    echo "restore: force vendor UI and disable the native gate"
    run touch "$conf/oa_safe"
    run rm -f "$conf/oa_boot"
    if [ "$apply" = 1 ]; then
        sync
        echo
        echo "Stock is selected for the next boot. Reboot to apply;"
        echo "or wait for oa-supervise to fall back if a native session is running."
        echo "USB recovery remains the last resort (recovery-stability stick)."
    else
        echo
        echo "Dry run: nothing changed. Add --apply to restore stock."
    fi
}

deploy()
{
    [ -n "$bundle" ] || { echo "--bundle is required" >&2; usage; }
    [ -n "$stage" ] || { echo "--stage is required" >&2; usage; }
    case $stage in
    0|1|2|3|4|5) ;;
    *) echo "invalid stage '$stage' (use 0..4)" >&2; exit 2 ;;
    esac

    stage_conf=openaurora-stage$stage.conf
    for f in openaurora oa-supervise oa-wdtfeed oa-boot-gate.sh run-aurora.sh \
             aurora-boot-hook.sh "$stage_conf" modules manifest.sha256; do
        [ -e "$bundle/$f" ] || {
            echo "refusing: bundle is missing $f" >&2
            exit 1
        }
    done
    [ -x "$bundle/oa-wdtfeed" ] || { echo "refusing: oa-wdtfeed is not executable" >&2; exit 1; }
    [ -x "$bundle/oa-boot-gate.sh" ] || { echo "refusing: oa-boot-gate.sh is not executable" >&2; exit 1; }
    [ -x "$bundle/run-aurora.sh" ] || { echo "refusing: run-aurora.sh is not executable" >&2; exit 1; }

    case $bundle in
    "$install_dir"|"$install_dir/"*)
        echo "refusing: --bundle must not be the install path $install_dir" >&2
        exit 1
        ;;
    esac

    if command -v sha256sum >/dev/null 2>&1; then
        (cd "$bundle" && sha256sum -c manifest.sha256 >/dev/null) || {
            echo "refusing: bundle manifest verification failed" >&2
            exit 1
        }
    fi

    require_recovery

    echo "deploy: stage $stage from $bundle to $install_dir"
    if [ -e "$install_dir" ]; then
        run mv "$install_dir" "$install_dir.pre-staged-$ts"
    fi
    run cp -a "$bundle" "$install_dir"
    if [ "$apply" = 1 ] && command -v sha256sum >/dev/null 2>&1; then
        (cd "$install_dir" && sha256sum -c manifest.sha256 >/dev/null) || {
            echo "installed manifest verification failed" >&2
            exit 1
        }
    fi

    echo "deploy: install the boot-hook template"
    run cp -p "$hook" "$hook.pre-staged-$ts"
    run cp -p "$install_dir/aurora-boot-hook.sh" "$hook"
    run chmod 700 "$hook"
    if [ "$apply" = 1 ]; then
        sh -n "$hook" || {
            echo "installed hook failed syntax check; restoring the backup" >&2
            cp -p "$hook.pre-staged-$ts" "$hook"
            exit 1
        }
    fi

    echo "deploy: arm stage $stage"
    run sh -c "printf '%s\n' '$stage' > '$conf/oa_stage'"
    run sh -c ": > '$conf/oa_boot'"
    run rm -f "$conf/oa_safe"

    if [ "$apply" = 1 ]; then
        sync
        echo
        echo "Armed for stage $stage. The next power-on runs the replacement."
        echo "Logs survive the /conf recovery:"
        echo "  /buffer/tvhack-boot.log      hook state/decision + feeder beats"
        echo "  /buffer/openaurora-app.log   entry, supervisor and app output"
        echo "  /buffer/oa-boot-history.log  prepare/start/clear history"
        echo "  /conf/oa_last_ok             written only after the stable window"
        echo "  /conf/oa_fail                nonzero or 'exec vendor fallback' = failed"
        echo "If the set resets/hangs, the watchdog feeder stops and the attempt"
        echo "is counted; three unproven boots disable the native path."
        echo
        echo "Restore: sh $0 --restore --apply"
    else
        echo
        echo "Dry run: nothing changed. Add --apply to deploy."
    fi
}

if [ "$restore" = 1 ]; then
    restore_stock
else
    deploy
fi
