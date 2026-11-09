#!/bin/sh
# Print the cold profile for one hardware bring-up stage.
#
# Stage 0 is the first power-on test: real DRM/OSD/UI only. Each later stage
# adds exactly one hardware owner, so a reset or hang points at that stage
# rather than at the whole cold init:
#
#   0  DRM + OSD reinit + UI          (no panel sequence, SPI, tuner or audio)
#   1  + panel standby/power-on
#   2  + SPI
#   3  + NIM-DTV tuner init
#   4  + audio I2S MCLK
#
# Source/audio-source/CEC/amp remain gated in every stage.
#
# Usage: stage-profile.sh STAGE [BASE]
set -eu

stage=${1:-0}
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base=${2:-$here/openaurora-cold.conf}

case $stage in
0|1|2|3|4) ;;
*)
    echo "stage-profile: invalid stage '$stage' (use 0..4)" >&2
    exit 2
    ;;
esac

panel=0
spi=0
tuner=0
i2s=0
[ "$stage" -ge 1 ] && panel=1
[ "$stage" -ge 2 ] && spi=1
[ "$stage" -ge 3 ] && tuner=1
[ "$stage" -ge 4 ] && i2s=1

base_autoload=plat,display,panel,ir,settings,service_db,image_decode,epg,pvr,channels,i18n,ticker,ui_clock,ui_anim,ui_engine,ui_vendor,ui_theme,ui_profile,ui_menu,ui_list,ui_navigation,lua,power
case $stage in
0|1) autoload=$base_autoload ;;
2) autoload=$base_autoload,spi ;;
3) autoload=$base_autoload,spi,tuner ;;
4) autoload=$base_autoload,spi,tuner,audio,audio_curve,audio_mixer,audio_amp_profile,audio_amp ;;
esac

{
    echo "# Generated cold-boot stage $stage from $(basename "$base")."
    echo "# Stage 0 = DRM/OSD/UI base; each later stage adds one hardware owner."
    sed \
        -e "s|^autoload = .*|autoload = $autoload|" \
        -e "s|^panel.standby = .*|panel.standby = $panel|" \
        -e "s|^panel.power_on_start = .*|panel.power_on_start = $panel|" \
        -e "s|^spi.enable = .*|spi.enable = $spi|" \
        -e "s|^tuner.hw_init = .*|tuner.hw_init = $tuner|" \
        -e "s|^audio.i2s_mclk = .*|audio.i2s_mclk = $i2s|" \
        "$base"
}
