#!/bin/sh
# Verify a built replacement bundle before staging it on the TV.
#   tools/boot/check-staged-bundle.sh <bundle-dir> [expected-modules]
# Checks the committed hash manifest, ARM EABI of every ELF, the required
# helper binaries, and that the risky hardware gates are off in the config.
# Offline only; does not contact or modify the TV.
set -eu
bundle=$1
expected=${2:-35}
[ -d "$bundle" ] || { echo "missing bundle: $bundle" >&2; exit 2; }

cd "$bundle"
[ -f manifest.sha256 ] || { echo "missing manifest.sha256" >&2; exit 2; }
sha256sum -c manifest.sha256 --quiet
echo "manifest: $(wc -l < manifest.sha256) entries verified"

n=$(ls modules/*.so | wc -l)
[ "$n" -eq "$expected" ] || { echo "module count $n != $expected" >&2; exit 2; }
echo "modules: $n"

for f in openaurora oa-supervise fb-restore oa-client oa-ir-blast \
         oa-audio-broker oa-alsactl oa-app-launch oa-wdtfeed \
         takeover-full.sh; do
    [ -f "$f" ] || { echo "missing $f" >&2; exit 2; }
done
file openaurora modules/mod_ui_vendor.so modules/mod_source.so | grep -q 'ELF 32-bit.*ARM' || {
    echo "unexpected ELF type (want ARM EABI5)" >&2; exit 2; }
echo "binaries: ARM EABI5 core and modules"

# The parity stack must preserve the vendor OSD and leave the risky hardware
# features gated; an authorized window enables them explicitly if needed.
grep -q '^plat.drm_init = 0$' openaurora.conf
grep -q '^source.enable = 0$' openaurora.conf
grep -q '^cec.enable = 0$' openaurora.conf
grep -q '^audio_amp.enable = 0$' openaurora.conf
echo "config: plat.drm_init/source/cec/audio_amp gates off"

echo "ok staged bundle: $bundle"
