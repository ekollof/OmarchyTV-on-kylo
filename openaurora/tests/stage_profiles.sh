#!/bin/sh
# Host-only tests for the cold-boot stage profile ladder.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
gen=$root/tools/boot/stage-profile.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-stage-profile.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

for stage in 0 1 2 3 4 5; do
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

expect_autoload()
{
    stage=$1 module=$2 present=$3
    line=$(sed -n 's|^autoload = ||p' "$tmp/stage$stage.conf" | head -1)
    case ",$line," in
    *",$module,"*) got=1 ;;
    *) got=0 ;;
    esac
    if [ "$got" != "$present" ]; then
        echo "stage $stage: $module present=$got, want $present" >&2
        exit 1
    fi
}

# Every stage keeps the UI base and closed gates.
for stage in 0 1 2 3 4 5; do
    expect "$stage" source.enable 0
    expect "$stage" audio.source_enable 0
    expect "$stage" cec.enable 0
    expect "$stage" audio_amp.enable 0
    expect "$stage" ui_menu.file /buffer/openaurora/menus/sub_launcher.json
    expect "$stage" audio_mixer.broker /buffer/openaurora/oa-audio-broker
    expect_autoload "$stage" plat 1
    expect_autoload "$stage" display 1
    expect_autoload "$stage" ui_vendor 1
    expect_autoload "$stage" source 0
    expect_autoload "$stage" audio_source 0
    expect_autoload "$stage" cec 0
done
echo "ok stage profiles: all stages keep the UI base and closed gates"

# Stage 0/1 preserve the vendor-initialized OSD (takeover display path).
for stage in 0 1; do
    expect "$stage" plat.drm_init 0
    expect "$stage" display.cold_init 0
    expect "$stage" display.osd_reinit 0
    expect "$stage" display.restore_snapshot 1
    expect_autoload "$stage" spi 0
    expect_autoload "$stage" tuner 0
    expect_autoload "$stage" audio 0
done
expect 0 panel.standby 0
expect 0 panel.power_on_start 0
expect 0 spi.enable 0
expect 0 tuner.hw_init 0
expect 0 audio.i2s_mclk 0
echo "ok stage profiles: stage 0 takes over the vendor OSD without reinit"

# Stage 1 adds only the panel sequence.
expect 1 panel.standby 1
expect 1 panel.power_on_start 1
expect 1 spi.enable 0
expect 1 tuner.hw_init 0
expect 1 audio.i2s_mclk 0

# Stage 2 adds the forced DRM/OSD reinit (the top-left-quadrant risk).
for stage in 2 3 4 5; do
    expect "$stage" plat.drm_init 1
    expect "$stage" display.cold_init 1
    expect "$stage" display.osd_reinit 1
    expect "$stage" display.restore_snapshot 0
    expect "$stage" panel.standby 1
done
expect 2 spi.enable 0
expect 2 tuner.hw_init 0
expect 2 audio.i2s_mclk 0
expect_autoload 2 spi 0
expect_autoload 2 tuner 0
expect_autoload 2 audio 0
echo "ok stage profiles: stage 2 isolates the forced OSD reinit"

# Stages 3..5 add SPI, tuner and audio one at a time.
expect 3 spi.enable 1
expect 3 tuner.hw_init 0
expect 3 audio.i2s_mclk 0
expect_autoload 3 spi 1
expect_autoload 3 tuner 0
expect_autoload 3 audio 0

expect 4 spi.enable 1
expect 4 tuner.hw_init 1
expect 4 audio.i2s_mclk 0
expect_autoload 4 spi 1
expect_autoload 4 tuner 1
expect_autoload 4 audio 0

expect 5 spi.enable 1
expect 5 tuner.hw_init 1
expect 5 audio.i2s_mclk 1
expect_autoload 5 spi 1
expect_autoload 5 tuner 1
expect_autoload 5 audio 1
expect_autoload 5 audio_mixer 1
echo "ok stage profiles: stages 3..5 add SPI, tuner and audio in order"

# Invalid stages are rejected.
if sh "$gen" 6 >/dev/null 2>&1; then
    echo "stage 6 was accepted" >&2
    exit 1
fi
echo "ok stage profiles: invalid stage is rejected"

echo "all stage profile tests passed"
