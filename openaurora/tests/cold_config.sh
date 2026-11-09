#!/bin/sh
# Static contract for the cold-start profile. This does not start a module,
# touch a device or claim TV acceptance; it prevents the cold/takeover flags
# from drifting apart.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cold="$here/tools/boot/openaurora-cold.conf"
takeover="$here/tools/boot/openaurora-full-replacement.conf"
native="$here/tools/boot/openaurora-native.conf"
fail() { echo "cold-config: $*" >&2; exit 1; }

grep -qx 'plat\.drm_init = 1' "$cold" || fail "cold plat.drm_init"
grep -qx 'display\.restore_snapshot = 0' "$cold" || fail "cold restore_snapshot"
grep -qx 'display\.cold_init = 1' "$cold" || fail "cold display.cold_init"
grep -qx 'panel\.standby = 1' "$cold" || fail "cold panel.standby"
grep -qx 'panel\.power_on_start = 1' "$cold" || fail "cold panel.power_on_start"
grep -qx 'audio\.i2s_mclk = 1' "$cold" || fail "cold audio.i2s_mclk"
grep -qx 'spi\.enable = 1' "$cold" || fail "cold spi.enable"
grep -qx 'source\.enable = 0' "$cold" || fail "cold source.enable"
grep -qx 'cec\.enable = 0' "$cold" || fail "cold cec.enable"

autoload=$(sed -n 's/^autoload = //p' "$cold")
[ -n "$autoload" ] || fail "cold autoload missing"
case ",$autoload," in *",ui_vendor,"*) ;; *) fail "cold ui_vendor missing" ;; esac
case ",$autoload," in *",ui_omarchy,"*) fail "cold must use ui_vendor" ;; esac
case ",$autoload," in *",spi,"*) ;; *) fail "cold spi not autoloaded" ;; esac
spi_pos=$(printf '%s\n' "$autoload" | awk -F, '{for(i=1;i<=NF;i++) if($i=="spi") print i}')
tuner_pos=$(printf '%s\n' "$autoload" | awk -F, '{for(i=1;i<=NF;i++) if($i=="tuner") print i}')
[ -n "$spi_pos" ] && [ -n "$tuner_pos" ] && [ "$spi_pos" -lt "$tuner_pos" ] ||
    fail "cold spi must precede tuner (spi=$spi_pos tuner=$tuner_pos)"

grep -qx 'plat\.drm_init = 0' "$takeover" || fail "takeover plat.drm_init"
grep -qx 'display\.restore_snapshot = 1' "$takeover" || fail "takeover restore_snapshot"
grep -qx 'panel\.power_on_start = 0' "$takeover" || fail "takeover power_on_start"
grep -qx 'plat\.drm_init = 0' "$native" || fail "native plat.drm_init"
grep -qx 'panel\.power_on_start = 0' "$native" || fail "native power_on_start"

echo "ok cold config: stock UI, cold flags, spi before tuner, takeover flags unchanged"
