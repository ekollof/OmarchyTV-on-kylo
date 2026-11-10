#!/bin/sh
# Host-only checks for the hardware-only cold ladder. No UI/display module is
# allowed: each stage is one documented hardware owner from the recovered
# DEV_INI_Init order, with the missing stages left out rather than faked.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
gen=$root/tools/boot/stage-hw-profile.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/oa-hw-stage.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

for stage in hw0 hw1 hw2 hw3 hw4; do
    sh "$gen" "$stage" >"$tmp/$stage.conf"
done

expect()
{
    stage=$1 key=$2 want=$3
    got=$(sed -n "s|^$key = ||p" "$tmp/$stage.conf" | head -1)
    if [ "$got" != "$want" ]; then
        echo "$stage: $key = '$got', want '$want'" >&2
        exit 1
    fi
}

expect_autoload()
{
    stage=$1 want=$2
    got=$(sed -n 's|^autoload = ||p' "$tmp/$stage.conf" | head -1)
    if [ "$got" != "$want" ]; then
        echo "$stage: autoload = '$got', want '$want'" >&2
        exit 1
    fi
}

for stage in hw0 hw1 hw2 hw3 hw4; do
    expect "$stage" plat.drm_init 1
    expect "$stage" plat.driver_wait 1
    expect "$stage" host.ready_after_autoload 1
    expect "$stage" source.enable 0
    expect "$stage" audio.source_enable 0
    expect "$stage" audio_amp.enable 0
    expect "$stage" cec.enable 0
    line=$(sed -n 's|^autoload = ||p' "$tmp/$stage.conf" | head -1)
    for ui in display ui_engine ui_vendor ui_omarchy ui_menu ir settings service_db; do
        case ",$line," in
        *",$ui,"*)
            echo "$stage: forbidden module $ui is autoloaded" >&2
            exit 1
            ;;
        esac
    done
done

expect_autoload hw0 plat
expect_autoload hw1 plat,panel
expect_autoload hw2 plat,panel,audio
expect_autoload hw3 plat,panel,audio,spi
expect_autoload hw4 plat,panel,audio,spi,tuner

for stage in hw0; do
    expect "$stage" panel.standby 0
    expect "$stage" panel.power_on_start 0
    expect "$stage" audio.i2s_mclk 0
    expect "$stage" spi.enable 0
    expect "$stage" tuner.hw_init 0
done

for stage in hw1 hw2 hw3 hw4; do
    expect "$stage" panel.standby 1
    expect "$stage" panel.power_on_start 1
done
for stage in hw2 hw3 hw4; do
    expect "$stage" audio.i2s_mclk 1
done
for stage in hw3 hw4; do
    expect "$stage" spi.enable 1
done
expect hw4 tuner.hw_init 1

if sh "$gen" hw5 >/dev/null 2>&1; then
    echo "hw5 was accepted" >&2
    exit 1
fi
if sh "$gen" 0 >/dev/null 2>&1; then
    echo "numeric stage 0 was accepted by the hardware ladder" >&2
    exit 1
fi

echo "ok hardware-only stages: plat -> panel -> I2S MCLK -> SPI open -> NIM-DTV, no display/UI"
echo "all hardware-only stage profile tests passed"
