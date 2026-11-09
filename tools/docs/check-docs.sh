#!/bin/sh
# Documentation hygiene checks for the RE docs and the canonical report index.
#
#  - every relative link in the docs/README/report index resolves;
#  - docs/reverse-engineering/report-map.md matches the current report index;
#  - every artifacts/reports/*.md (except README.md) is indexed.
#
# Run from anywhere; exits non-zero on a stale or broken doc set.
set -u

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cd "$root" || exit 1

tmp=$(mktemp "${TMPDIR:-/tmp}/oa-docs-check.XXXXXX")
trap 'rm -f "$tmp" "$tmp.fail" "$tmp.map" "$tmp.files" "$tmp.links"' EXIT HUP INT TERM
: >"$tmp.fail"

check_links()
{
    for f in "$@"; do
        [ -f "$f" ] || continue
        d=$(dirname "$f")
        grep -oE '\]\([^)]+\)' "$f" | sed 's/^](//;s/)$//' |
        while read -r l; do
            case "$l" in
            http*|mailto:*) continue ;;
            esac
            t=${l%%#*}
            [ -z "$t" ] && continue
            if [ ! -e "$d/$t" ]; then
                echo "BROKEN $f -> $l"
                : >"$tmp.fail"
            fi
        done
    done
}

check_links README.md AGENTS.md docs/README.md docs/reverse-engineering/*.md \
    artifacts/reports/README.md

sh tools/docs/gen-report-map.sh "$tmp.map" >/dev/null || {
    echo "report-map generator failed"
    : >"$tmp.fail"
}
if [ -f "$tmp.map" ] &&
   ! cmp -s "$tmp.map" docs/reverse-engineering/report-map.md; then
    echo "STALE docs/reverse-engineering/report-map.md (run tools/docs/gen-report-map.sh)"
    : >"$tmp.fail"
fi

ls artifacts/reports/*.md | sed 's|.*/||' | grep -v '^README.md$' | sort >"$tmp.files"
grep -oE '\]\([A-Za-z0-9._-]+\.md\)' artifacts/reports/README.md |
    sed 's/^](//;s/)$//' | sort -u >"$tmp.links"

while read -r f; do
    if ! grep -qxF "$f" "$tmp.links"; then
        echo "UNINDEXED artifacts/reports/$f"
        : >"$tmp.fail"
    fi
done <"$tmp.files"

if [ -s "$tmp.fail" ]; then
    echo "docs checks failed"
    exit 1
fi
echo "docs checks passed ($(wc -l <"$tmp.files") reports)"
