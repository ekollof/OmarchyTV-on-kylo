#!/bin/sh
# Print a hardware-only cold-start profile for one documented injection point.
#
# The order follows the recovered vendor sequence:
#   PLAT_DRV_INI_Init -> DEV_PANEL_Init -> DEV_AMP_Init (I2S MCLK only)
#   -> DEV_SPI_Init/Open -> DEV_NIM_DTV_Init
# Stages without a documented replacement (PCB layout, DEV GPIO/IIC,
# dev_src_init, NIM factories, DEV_AMP_ID, MTD) are not added as if they
# passed. No display/UI module is ever autoloaded.
#
#   hw0  plat HAL sequence only
#   hw1  + panel PWM/backlight/power
#   hw2  + audio I2S MCLK request
#   hw3  + SPI file/device open
#   hw4  + NIM-DTV tuner task
#
# Usage: stage-hw-profile.sh hw0..hw4 [BASE]
set -eu

stage=${1:-hw0}
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
base=${2:-$here/openaurora-cold-hw.conf}

case $stage in
hw0|hw1|hw2|hw3|hw4) ;;
*)
    echo "stage-hw-profile: invalid stage '$stage' (use hw0..hw4)" >&2
    exit 2
    ;;
esac

n=${stage#hw}
panel=0
audio=0
spi=0
tuner=0
[ "$n" -ge 1 ] && panel=1
[ "$n" -ge 2 ] && audio=1
[ "$n" -ge 3 ] && spi=1
[ "$n" -ge 4 ] && tuner=1

autoload=plat
[ "$panel" = 1 ] && autoload=$autoload,panel
[ "$audio" = 1 ] && autoload=$autoload,audio
[ "$spi" = 1 ] && autoload=$autoload,spi
[ "$tuner" = 1 ] && autoload=$autoload,tuner

{
    echo "# Generated hardware-only cold stage $stage from $(basename "$base")."
    echo "# No display/UI; each later stage adds one documented hardware owner."
    sed \
        -e "s|^autoload = .*|autoload = $autoload|" \
        -e "s|^panel.standby = .*|panel.standby = $panel|" \
        -e "s|^panel.power_on_start = .*|panel.power_on_start = $panel|" \
        -e "s|^audio.i2s_mclk = .*|audio.i2s_mclk = $audio|" \
        -e "s|^spi.enable = .*|spi.enable = $spi|" \
        -e "s|^tuner.hw_init = .*|tuner.hw_init = $tuner|" \
        "$base"
}
