#!/bin/sh
# Optional REA second-opinion layer for tvhack.
#
# Bootstraps a pinned rea-agents install under artifacts/workbench/rea
# (gitignored) and runs static analysis on hashed copies. It never opens the
# canonical Ghidra projects, never writes to the repo, and never touches the TV.
#
#   tools/re/rea.sh doctor
#   tools/re/rea.sh kernel <module.ko> [rea args...]
#   tools/re/rea.sh function <binary> <address> [rea args...]
#   tools/re/rea.sh objdump <binary> <symbol> [rea args...]
#   tools/re/rea.sh raw <rea args...>
#   tools/re/rea.sh version
#
# Environment:
#   REA_VERSION                     pinned version (default 6.9.0)
#   REA_PREFIX                      install prefix (default artifacts/workbench/rea)
#   REA_TARGETS                     copy cache (default /tmp/opencode/rea-targets)
#   REA_OBJDUMP_COMMAND             defaults to the ARM cross objdump if present
#   REA_GHIDRA_STARTUP_TIMEOUT_MS   default 900000
#
# REA is a lead generator, not the source of truth. Verify exact instructions
# in the canonical Ghidra/objdump workflow before recording a contract.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
REA_VERSION=${REA_VERSION:-6.9.0}
REA_PREFIX=${REA_PREFIX:-"$ROOT/artifacts/workbench/rea"}
REA_TARGETS=${REA_TARGETS:-"${TMPDIR:-/tmp}/opencode/rea-targets"}
REA_BIN="$REA_PREFIX/node_modules/.bin/rea"
CROSS_BINUTILS="$ROOT/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin"

usage()
{
    sed -n '2,23p' "$0" | sed 's/^# \{0,1\}//'
}

die()
{
    echo "rea.sh: $*" >&2
    exit 1
}

ensure_rea()
{
    command -v node >/dev/null 2>&1 || die "node is required"
    command -v npm >/dev/null 2>&1 || die "npm is required"
    if [ ! -x "$REA_BIN" ]; then
        echo "rea.sh: installing rea-agents@$REA_VERSION under $REA_PREFIX" >&2
        mkdir -p "$REA_PREFIX"
        npm_config_cache="$REA_PREFIX/npm-cache" \
            npm install --prefix "$REA_PREFIX" --ignore-scripts --no-audit --no-fund \
            "rea-agents@$REA_VERSION" >&2
    fi
    have=$("$REA_BIN" --version 2>/dev/null | head -1 || true)
    [ "$have" = "$REA_VERSION" ] ||
        die "expected rea-agents $REA_VERSION, found '${have:-none}' (remove $REA_PREFIX to reinstall)"
}

set_env()
{
    if [ -z "${REA_OBJDUMP_COMMAND:-}" ] &&
       [ -x "$CROSS_BINUTILS/arm-none-linux-gnueabihf-objdump" ]; then
        REA_OBJDUMP_COMMAND="$CROSS_BINUTILS/arm-none-linux-gnueabihf-objdump"
        export REA_OBJDUMP_COMMAND
    fi
    REA_GHIDRA_STARTUP_TIMEOUT_MS=${REA_GHIDRA_STARTUP_TIMEOUT_MS:-900000}
    export REA_GHIDRA_STARTUP_TIMEOUT_MS
}

copy_target()
{
    src=$1
    [ -f "$src" ] || die "not a regular file: $src"
    mkdir -p "$REA_TARGETS"
    hash=$(sha256sum "$src" | awk '{print $1}')
    base=$(basename "$src")
    dst="$REA_TARGETS/$(printf '%s' "$hash" | cut -c1-12)-$base"
    if [ ! -f "$dst" ]; then
        tmp="$dst.tmp.$$"
        cp -p "$src" "$tmp"
        chmod 444 "$tmp"
        mv -f "$tmp" "$dst"
    fi
    copy_hash=$(sha256sum "$dst" | awk '{print $1}')
    [ "$copy_hash" = "$hash" ] || die "copy hash mismatch for $dst"
    printf '%s\n' "$dst"
}

cmd=${1:-}
[ $# -gt 0 ] && shift || true

case "$cmd" in
    help|--help|-h|"")
        usage
        exit 0
        ;;
    version)
        printf '%s\n' "$REA_VERSION"
        exit 0
        ;;
esac

ensure_rea
set_env

case "$cmd" in
    doctor)
        exec "$REA_BIN" doctor --format json
        ;;
    kernel)
        [ $# -ge 1 ] || die "kernel requires a .ko path"
        target=$1
        shift
        copy=$(copy_target "$target")
        exec "$REA_BIN" inspect-kernel-module "$copy" --format json "$@"
        ;;
    function)
        [ $# -ge 2 ] || die "function requires a binary and an address"
        target=$1
        address=$2
        shift 2
        copy=$(copy_target "$target")
        exec "$REA_BIN" function "$copy" "$address" \
            --provider ghidra --format json "$@"
        ;;
    objdump)
        [ $# -ge 2 ] || die "objdump requires a binary and a symbol"
        target=$1
        symbol=$2
        shift 2
        [ -n "${REA_OBJDUMP_COMMAND:-}" ] ||
            die "no ARM objdump found; set REA_OBJDUMP_COMMAND"
        copy=$(copy_target "$target")
        exec "$REA_BIN" inspect-with-objdump "$copy" \
            disassemble --symbol "$symbol" --format json "$@"
        ;;
    raw)
        [ $# -ge 1 ] || die "raw requires rea arguments"
        echo "rea.sh: raw uses the path you supply; use copies, never a canonical project or device" >&2
        exec "$REA_BIN" "$@"
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
