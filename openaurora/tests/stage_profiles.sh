#!/bin/sh
# Host-only tests for the cold-boot stage profile ladder.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
gen=$root/tools/boot/stage-profile.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-stage-profile.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

for stage in 0 1 2 3 4; do
    sh "$gen" "$stage" >"$tmp/stage$stage.conf"
done

expect()
{
    stage=$1 key=$2 want=$3
    got=$(sed -n "s|^$key = ||p" "$tmp/stage$stage.conf" | head -1)
    if [ "$got" != "$want" ]; then
        echo "stage $stage: $key = '$got', want '$want'" >&2
        exit 1
    fi
}

# Every stage keeps the DRM/OSD/UI base.
for stage in 0 1 2 3 4; do
    expect "$stage" plat.drm_init 1
    expect "$stage" display.cold_init 1
    expect "$stage" display.osd_reinit 1
    expect "$stage" display.restore_snapshot 0
    expect "$stage" source.enable 0
    expect "$stage" audio.source_enable 0
    expect "$stage" cec.enable 0
    expect "$stage" audio_amp.enable 0
    expect "$stage" ui_menu.file /buffer/openaurora/menus/sub_launcher.json
    expect "$stage" audio_mixer.broker /buffer/openaurora/oa-audio-broker
done
echo "ok stage profiles: all stages keep the UI base and closed gates"

# Stage 0 is DRM/OSD/UI only.
expect 0 panel.standby 0
expect 0 panel.power_on_start 0
expect 0 spi.enable 0
expect 0 tuner.hw_init 0
expect 0 audio.i2s_mclk 0
echo "ok stage profiles: stage 0 enables no hardware owner beyond DRM/OSD"

# The ladder adds exactly one owner per stage.
expect 1 panel.standby 1
expect 1 panel.power_on_start 1
expect 1 spi.enable 0
expect 1 tuner.hw_init 0
expect 1 audio.i2s_mclk 0

expect 2 panel.standby 1
expect 2 spi.enable 1
expect 2 tuner.hw_init 0
expect 2 audio.i2s_mclk 0

expect 3 panel.standby 1
expect 3 spi.enable 1
expect 3 tuner.hw_init 1
expect 3 audio.i2s_mclk 0

expect 4 panel.standby 1
expect 4 spi.enable 1
expect 4 tuner.hw_init 1
expect 4 audio.i2s_mclk 1
echo "ok stage profiles: stages 1..4 add panel, SPI, tuner and I2S in order"

# Invalid stages are rejected.
if sh "$gen" 5 >/dev/null 2>&1; then
    echo "stage 5 was accepted" >&2
    exit 1
fi
echo "ok stage profiles: invalid stage is rejected"

echo "all stage profile tests passed"
