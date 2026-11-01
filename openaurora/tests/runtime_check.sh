#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
BUILD=${BUILD:-build}
CC=${CC:-cc}
CFLAGS=${CFLAGS:--std=c11 -O2 -g -Wall -Wextra -Werror -pedantic}
task_check_tmp=$(mktemp -d /tmp/openaurora-check.XXXXXX)
trap 'rm -rf "$task_check_tmp"' EXIT INT TERM
mkdir "$task_check_tmp/modules" "$task_check_tmp/deps"
fixture="$task_check_tmp/modules/mod_fixture.so"
compile() {
    # CFLAGS is an argument list, not shell code; fixtures are native host code.
    # CC may be a command plus arguments (e.g. "ccache cc"), so it is split.
    $CC $CFLAGS -D_GNU_SOURCE -Iinclude -fPIC -shared tests/check_fixture.c -o "$fixture" "$@"
}
check() {
    "$BUILD/openaurora" --check --modules "$task_check_tmp/modules" \
        --sock "$task_check_tmp/must-not-exist.sock" > "$task_check_tmp/output" 2>&1
}
reject() {
    if check; then cat "$task_check_tmp/output"; echo "FAIL checker accepted $1"; exit 1; fi
    if ! rg -q "$2" "$task_check_tmp/output"; then
        cat "$task_check_tmp/output"; echo "FAIL missing diagnostic for $1"; exit 1
    fi
}
compile
check
test ! -e "$task_check_tmp/must-not-exist.sock"
echo '  ok   valid module passes; constructors/startup do not run; no control socket'
compile -DTEST_OLD_ABI
reject 'old ABI' 'module ABI/descriptor-size mismatch'
compile -DTEST_NO_NOTE
reject 'missing note' 'missing ABI note'
compile -DTEST_NO_ENTRY
reject 'missing entry' 'missing exported oa_module_entry'
printf 'not an ELF file\n' > "$fixture"
reject 'invalid ELF' 'invalid/incompatible ELF'
echo '  ok   ABI mismatch, legacy/no-note, missing entry and invalid ELF reject'
compile -DTEST_UNDEFINED
reject 'unresolved function' 'undefined symbol: oa_check_missing_function'
echo '  ok   unresolved function rejects even when glibc trace exits zero'
$CC $CFLAGS -DTEST_DEPENDENCY -Iinclude -fPIC -shared tests/check_fixture.c \
    -o "$task_check_tmp/deps/libcheck_dependency.so"
compile -DTEST_NEEDED -L"$task_check_tmp/deps" -Wl,--no-as-needed -lcheck_dependency \
    -Wl,-rpath,"$task_check_tmp/deps"
check
mv "$task_check_tmp/deps/libcheck_dependency.so" "$task_check_tmp/deps/hidden.so"
reject 'missing dependency' 'libcheck_dependency.so.*not found'
echo '  ok   real DT_NEEDED dependency passes with RPATH, rejects when missing'
$CC $CFLAGS -DTEST_DEPENDENCY -Iinclude -fPIC -shared tests/check_fixture.c \
    -Wl,--version-script=tests/check_v1.map -o "$task_check_tmp/deps/libcheck_dependency.so"
compile -DTEST_NEEDED -L"$task_check_tmp/deps" -Wl,--no-as-needed -lcheck_dependency \
    -Wl,-rpath,"$task_check_tmp/deps"
check
$CC $CFLAGS -DTEST_DEPENDENCY -Iinclude -fPIC -shared tests/check_fixture.c \
    -Wl,--version-script=tests/check_v2.map -o "$task_check_tmp/deps/libcheck_dependency.so"
reject 'wrong symbol version' 'OA_CHECK_1.*not found'
echo '  ok   present library with incompatible symbol version rejects'
rm "$fixture"
reject 'empty directory' 'no mod_.*so modules'
if "$BUILD/openaurora" --check --modules "$task_check_tmp/absent" > "$task_check_tmp/output" 2>&1; then
    echo 'FAIL missing directory accepted'; exit 1
fi
echo '  ok   empty/missing module directory rejects'
