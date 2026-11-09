#!/bin/sh
# Host dependency check for reproducing the tvhack/openaurora work.
#
#   tools/check-host.sh          full host + analysis toolchain
#   tools/check-host.sh --root   only the NAND -> /conf root-payload path
#
# This script never installs anything, never reads the TV and never copies
# vendor material. It only reports what is present. See
# docs/reproduce/dependencies.md for upstream sources and install commands.
set -u

mode=full
case "${1:-}" in
    "") ;;
    --root) mode=root ;;
    -h|--help)
        echo "usage: $0 [--root]"
        exit 0
        ;;
    *)
        echo "usage: $0 [--root]" >&2
        exit 2
        ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fail=0

check_cmd()
{
    name=$1
    cmd=$2
    ver=${3:-}
    req=${4:-required}
    if command -v "$cmd" >/dev/null 2>&1; then
        if [ -n "$ver" ]; then
            v=$("$cmd" $ver 2>&1 | head -1)
        else
            v=present
        fi
        printf '  ok    %-24s %s\n' "$name" "$v"
    else
        if [ "$req" = required ]; then
            printf '  MISS  %-24s %s not found\n' "$name" "$cmd"
            fail=1
        else
            printf '  skip  %-24s %s not found (optional)\n' "$name" "$cmd"
        fi
    fi
}

check_qemu()
{
    for c in qemu-arm-static qemu-arm; do
        if command -v "$c" >/dev/null 2>&1; then
            printf '  ok    %-24s %s\n' "qemu-arm" "$("$c" --version | head -1)"
            return
        fi
    done
    printf '  MISS  %-24s install qemu-user-static\n' "qemu-arm"
    fail=1
}

if [ "$mode" = root ]; then
    echo "== root payload tools (NAND -> /conf)"
    check_cmd dd dd --version
    check_cmd python3 python3 --version
    check_cmd mkfs.ubifs mkfs.ubifs -V
    check_cmd sha256sum sha256sum --version
    check_cmd md5sum md5sum --version
    if [ "$(id -u)" -eq 0 ]; then
        printf '  ok    %-24s %s\n' "root/sudo" "running as root"
    elif command -v sudo >/dev/null 2>&1; then
        printf '  ok    %-24s %s\n' "root/sudo" "$(command -v sudo)"
    else
        printf '  MISS  %-24s need root or sudo for mount/extract/mkfs\n' \
            "root/sudo"
        fail=1
    fi
    check_cmd sqlite3 sqlite3 --version optional
    check_cmd unsquashfs unsquashfs -version optional
    check_cmd ssh-keygen ssh-keygen "" optional
    check_cmd dropbearkey dropbearkey "" optional
else
    echo "== host build/analysis tools"
    check_cmd git git --version
    check_cmd make make --version
    check_cmd cc cc --version
    check_cmd objdump objdump --version
    check_cmd readelf readelf --version
    check_cmd nm nm --version
    check_cmd strings strings --version
    check_cmd python3 python3 --version
    check_cmd node node --version
    check_qemu
    check_cmd mkfs.ubifs mkfs.ubifs -V
    check_cmd unsquashfs unsquashfs -version
    check_cmd dd dd --version
    check_cmd sqlite3 sqlite3 --version
    check_cmd ssh ssh -V
    check_cmd ssh-keygen ssh-keygen
    check_cmd java java -version
    check_cmd gdb gdb --version optional
    check_cmd ccache ccache --version optional
    check_cmd dropbearkey dropbearkey "" optional
    check_cmd curl curl --version optional
    check_cmd wget wget --version optional

    echo "== host build headers"
    if printf '#include <sqlite3.h>\n' | cc -E - >/dev/null 2>&1; then
        printf '  ok    %-24s %s\n' "sqlite3.h" "found"
    else
        printf '  MISS  %-24s install the sqlite3 development headers\n' "sqlite3.h"
        fail=1
    fi
fi

echo "== UBI extraction"
find_ubi_tool()
{
    # $1 = command name. Prefer the directory of UBIREADER, then the repo-local
    # venv (dependencies.md), the original workbench venv and PATH.
    name=$1
    if [ -n "${UBIREADER:-}" ]; then
        d=$(dirname "$UBIREADER")
        [ -x "$d/$name" ] && { echo "$d/$name"; return 0; }
    fi
    for c in "$root/.venv/bin/$name" \
             "$root/artifacts/workbench/venv/bin/$name"; do
        [ -x "$c" ] && { echo "$c"; return 0; }
    done
    command -v "$name" 2>/dev/null
}
ubi_images=$(find_ubi_tool ubireader_extract_images)
ubi_files=$(find_ubi_tool ubireader_extract_files)
ubi_info=$(find_ubi_tool ubireader_display_info)
if [ -n "$ubi_images" ]; then
    printf '  ok    %-24s %s\n' "ubireader_extract_images" "$ubi_images"
elif [ "$mode" = full ]; then
    printf '  MISS  %-24s install ubi_reader or set UBIREADER\n' \
        "ubireader_extract_images"
    fail=1
else
    printf '  skip  %-24s not needed for --root\n' "ubireader_extract_images"
fi
if [ -n "$ubi_files" ]; then
    printf '  ok    %-24s %s\n' "ubireader_extract_files" "$ubi_files"
else
    printf '  MISS  %-24s install ubi_reader\n' "ubireader_extract_files"
    fail=1
fi
if [ -n "$ubi_info" ]; then
    printf '  ok    %-24s %s\n' "ubireader_display_info" "$ubi_info"
else
    printf '  MISS  %-24s install ubi_reader\n' "ubireader_display_info"
    fail=1
fi

if [ "$mode" = full ]; then
    echo "== Ghidra"
    gh=${GHIDRA_HOME:-/opt/ghidra}
    if [ -x "$gh/support/analyzeHeadless" ]; then
        v=$(sed -n 's/^application.version=//p' "$gh/Ghidra/application.properties" 2>/dev/null)
        printf '  ok    %-24s %s\n' "ghidra" "${v:-present} ($gh)"
    else
        printf '  MISS  %-24s set GHIDRA_HOME (no %s/support/analyzeHeadless)\n' \
            "ghidra" "$gh"
        fail=1
    fi

    echo "== ARM cross toolchain"
    arm=${ARM_CC:-$root/artifacts/workbench/gcc-arm-10.2-2020.11-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc}
    if [ -x "$arm" ]; then
        printf '  ok    %-24s %s\n' "arm-none-linux-gnueabihf" \
            "$("$arm" --version | head -1)"
    elif command -v arm-none-linux-gnueabihf-gcc >/dev/null 2>&1; then
        printf '  ok    %-24s %s\n' "arm-none-linux-gnueabihf" \
            "$(arm-none-linux-gnueabihf-gcc --version | head -1)"
    else
        printf '  MISS  %-24s set ARM_CC or install the pinned 10.2-2020.11 toolchain\n' \
            "arm-none-linux-gnueabihf"
        fail=1
    fi

    echo "== optional VMLINUX tooling"
    if command -v vmlinux-to-elf >/dev/null 2>&1; then
        printf '  ok    %-24s %s\n' "vmlinux-to-elf" "$(command -v vmlinux-to-elf)"
    elif [ -x "$root/artifacts/workbench/velf/bin/vmlinux-to-elf" ]; then
        printf '  ok    %-24s %s\n' "vmlinux-to-elf" \
            "$root/artifacts/workbench/velf/bin/vmlinux-to-elf"
    else
        printf '  skip  %-24s not installed (only needed for kernel reconstruction)\n' \
            "vmlinux-to-elf"
    fi
else
    echo "== analysis tooling (skipped in --root mode)"
    echo "   run tools/check-host.sh without --root before building or Ghidra work."
fi

cat <<'EOF'

== firmware and vendor material (not checked)
Your own NAND dump, vendor libraries, Ghidra project databases, payload images
and keys are not part of this repository. Extract/build them from your own TV:
docs/reproduce/root-payload.md and docs/reverse-engineering/tooling.md.
Install/version help: docs/reproduce/dependencies.md.
EOF

if [ "$fail" -eq 0 ]; then
    if [ "$mode" = root ]; then
        echo "root path check passed"
    else
        echo "host check passed"
    fi
else
    if [ "$mode" = root ]; then
        echo "root path check failed"
    else
        echo "host check failed"
        echo "hint: tools/check-host.sh --root checks only the NAND -> /conf path"
    fi
    exit 1
fi
