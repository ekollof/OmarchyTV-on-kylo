#!/bin/sh
# Print the cold profile for one hardware bring-up stage.
#
# Each stage adds exactly one owner or one risky initialization, so a failure
# points at that stage rather than at the whole cold init:
#
#   0  takeover display + UI         no plat DRM init, no forced OSD SetPar,
#                                    no panel/SPI/tuner/audio
#   1  + panel standby/power-on
#   2  + plat DRM init and forced OSD reinit (the 4K-window risk)
#   3  + SPI
#   4  + NIM-DTV tuner init
#   5  + audio I2S MCLK and the audio modules
#
# Stage 0 preserves the vendor/appmgr-initialized OSD exactly like the proven
# takeover path. Stage 2 is isolated because forcing the OSD var/SetPar on a
# true boot shrank the visible UI to the top-left quadrant (2026-11-09).
# Source/audio-source/CEC/amp remain gated in every stage.
#
# Usage: stage-profile.sh STAGE [BASE]
set -eu

stage=${1:-0}
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base=${2:-$here/openaurora-cold.conf}

case $stage in
0|1|2|3|4|5) ;;
*)
    echo "stage-profile: invalid stage '$stage' (use 0..5)" >&2
    exit 2
    ;;
esac

panel=0
spi=0
tuner=0
i2s=0
drm=0
cold=0
reinit=0
snapshot=1
[ "$stage" -ge 1 ] && panel=1
[ "$stage" -ge 2 ] && drm=1 && cold=1 && reinit=1 && snapshot=0
[ "$stage" -ge 3 ] && spi=1
[ "$stage" -ge 4 ] && tuner=1
[ "$stage" -ge 5 ] && i2s=1

base_autoload=plat,display,panel,ir,settings,service_db,image_decode,epg,pvr,channels,i18n,ticker,ui_clock,ui_anim,ui_engine,ui_vendor,ui_theme,ui_profile,ui_menu,ui_list,ui_navigation,lua,power
case $stage in
0|1|2) autoload=$base_autoload ;;
3) autoload=$base_autoload,spi ;;
4) autoload=$base_autoload,spi,tuner ;;
5) autoload=$base_autoload,spi,tuner,audio,audio_curve,audio_mixer,audio_amp_profile,audio_amp ;;
esac

{
    echo "# Generated cold-boot stage $stage from $(basename "$base")."
    echo "# Stage 0 = takeover display/UI; each later stage adds one owner/init."
    sed \
        -e "s|^autoload = .*|autoload = $autoload|" \
        -e "s|^plat.drm_init = .*|plat.drm_init = $drm|" \
        -e "s|^display.restore_snapshot = .*|display.restore_snapshot = $snapshot|" \
        -e "s|^display.cold_init = .*|display.cold_init = $cold|" \
        -e "s|^display.osd_reinit = .*|display.osd_reinit = $reinit|" \
        -e "s|^panel.standby = .*|panel.standby = $panel|" \
        -e "s|^panel.power_on_start = .*|panel.power_on_start = $panel|" \
        -e "s|^spi.enable = .*|spi.enable = $spi|" \
        -e "s|^tuner.hw_init = .*|tuner.hw_init = $tuner|" \
        -e "s|^audio.i2s_mclk = .*|audio.i2s_mclk = $i2s|" \
        "$base"
}
