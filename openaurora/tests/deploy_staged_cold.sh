#!/bin/sh
# Host-only tests for the staged cold-boot deployment script. OA_ROOT makes
# every /conf and /buffer access a temporary directory.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
deploy=$root/tools/boot/deploy-staged-cold.sh
gen=$root/tools/boot/stage-profile.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-deploy.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

setroot=$tmp/set
mkdir -p "$setroot/conf/remotelf" "$setroot/buffer/openaurora/modules"
: >"$setroot/conf/coredumpnoreset"
printf '#!/bin/sh\nexit 0\n' >"$setroot/conf/remotelf/aurora.elf"
chmod 700 "$setroot/conf/remotelf/aurora.elf"
printf '#!/bin/sh\nexit 0\n' >"$setroot/buffer/wdtctl"
chmod 755 "$setroot/buffer/wdtctl"
echo old >"$setroot/buffer/openaurora/old-marker"

bundle=$tmp/bundle
mkdir -p "$bundle/modules"
for f in openaurora oa-supervise oa-wdtfeed; do
    printf '#!/bin/sh\nexit 0\n' >"$bundle/$f"
    chmod 755 "$bundle/$f"
done
cp "$root/tools/boot/oa-boot-gate.sh" "$bundle/"
cp "$root/tools/boot/run-aurora.sh" "$bundle/"
cp "$root/tools/boot/aurora-boot-hook.sh" "$bundle/"
for s in 0 1 2 3 4; do
    sh "$gen" "$s" >"$bundle/openaurora-stage$s.conf"
done
: >"$bundle/modules/x"
(
    cd "$bundle"
    sha256sum openaurora oa-supervise oa-wdtfeed oa-boot-gate.sh run-aurora.sh \
        aurora-boot-hook.sh openaurora-stage*.conf modules/x >manifest.sha256
)
chmod 755 "$bundle/oa-boot-gate.sh" "$bundle/run-aurora.sh"

export OA_ROOT=$setroot

# Dry run prints the plan and changes nothing.
out=$(sh "$deploy" --bundle "$bundle" --stage 0)
printf '%s\n' "$out" | grep -q "would: cp -a"
test -f "$setroot/buffer/openaurora/old-marker"
test ! -e "$setroot/conf/oa_boot"
test ! -e "$setroot/conf/oa_stage"
echo "ok deploy: dry run changes nothing"

# Apply installs the bundle, hook and stage flags with backups.
sh "$deploy" --bundle "$bundle" --stage 0 --apply >"$tmp/apply.log"
test -f "$setroot/conf/oa_boot"
test ! -e "$setroot/conf/oa_safe"
grep -qx "0" "$setroot/conf/oa_stage"
test ! -e "$setroot/buffer/openaurora/old-marker"
test -f "$setroot/buffer/openaurora/aurora-boot-hook.sh"
cmp "$bundle/aurora-boot-hook.sh" "$setroot/conf/remotelf/aurora.elf"
test -x "$setroot/conf/remotelf/aurora.elf"
ls "$setroot/conf/remotelf/"aurora.elf.pre-staged-* >/dev/null
ls "$setroot/buffer/"openaurora.pre-staged-* >/dev/null
grep -q "Armed for stage 0" "$tmp/apply.log"
echo "ok deploy: apply installs bundle, hook and stage flags with backups"

# Restore forces stock and disables the native path.
sh "$deploy" --restore --apply >"$tmp/restore.log"
test -f "$setroot/conf/oa_safe"
test ! -e "$setroot/conf/oa_boot"
grep -q "Stock is selected" "$tmp/restore.log"
echo "ok deploy: restore forces stock and disables the gate"

# Missing recovery flag refuses the deployment.
rm -f "$setroot/conf/coredumpnoreset"
if sh "$deploy" --bundle "$bundle" --stage 0 --apply >/dev/null 2>&1; then
    echo "deployed without coredumpnoreset" >&2
    exit 1
fi
: >"$setroot/conf/coredumpnoreset"
echo "ok deploy: missing recovery flag is refused"

# A tampered manifest refuses the deployment.
cp -a "$bundle" "$tmp/bad"
echo tamper >>"$tmp/bad/openaurora"
if sh "$deploy" --bundle "$tmp/bad" --stage 0 --apply >/dev/null 2>&1; then
    echo "deployed with a bad manifest" >&2
    exit 1
fi
echo "ok deploy: manifest mismatch is refused"

# A missing stage config refuses the deployment.
cp -a "$bundle" "$tmp/nostage"
rm -f "$tmp/nostage/openaurora-stage3.conf"
if sh "$deploy" --bundle "$tmp/nostage" --stage 3 --apply >/dev/null 2>&1; then
    echo "deployed without the selected stage config" >&2
    exit 1
fi
echo "ok deploy: missing stage config is refused"

# Invalid stage is rejected.
if sh "$deploy" --bundle "$bundle" --stage 9 --apply >/dev/null 2>&1; then
    echo "accepted stage 9" >&2
    exit 1
fi
echo "ok deploy: invalid stage is rejected"

echo "all staged cold-boot deploy tests passed"
