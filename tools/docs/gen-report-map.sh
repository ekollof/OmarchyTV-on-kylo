#!/bin/sh
# Generate docs/reverse-engineering/report-map.md from the canonical report index.
# Read-only over artifacts/reports/README.md; the output is a navigation aid,
# not a replacement for the dated reports or the full index.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
index=$root/artifacts/reports/README.md
out=${1:-$root/docs/reverse-engineering/report-map.md}
tmp=$(mktemp "${TMPDIR:-/tmp}/oa-report-map.XXXXXX")
trap 'rm -f "$tmp" "$tmp.pairs" "$tmp.sorted"' EXIT HUP INT TERM

grep -oE '^\| \[[^]]+\]\([^)]+\)' "$index" |
    sed -E 's/^\| \[([^]]+)\]\(([^)]+)\)/\1|\2/' >"$tmp.pairs"

awk -F'|' '
function group(n) {
    if (n ~ /^PQ-/) return "PQ and kernel tasks";
    if (n ~ /^SERVICE-DB/) return "Settings, DB and model";
    if (n ~ /^(AUDIO|AMP|MIXER|SPEAKER)-/) return "Audio and amplifier";
    if (n ~ /^CEC-/) return "CEC";
    if (n ~ /^(OSD|DISPLAY|SCENE|EGL|GPU|GRAPHICS|FB|DRM|VISIBLE|CONTROLLED)-/) return "Display, OSD, DRM and GPU";
    if (n ~ /^(PANEL|PWM)-/) return "Panel and TCON";
    if (n ~ /^(SOURCE|SOURCES|VIN)-/) return "Source, VIN and V4L2";
    if (n ~ /^(TUNER|SPI|NDAL|EASI|EFSL)-/) return "Tuner, NIM, DTV and SPI";
    if (n ~ /^IR-/) return "IR";
    if (n ~ /^(SETTINGS|HOTEL|PICTURE)-/) return "Settings, DB and model";
    if (n ~ /^(APP|APPS)-/) return "App manager and apps";
    if (n ~ /^(LAUNCHER|SUBLAUNCHER|TAB|SKIN)-/) return "Launcher, sub-launcher and tab policy";
    if (n ~ /^(UI|SVG|STOCK|NATIVE|PORTAL|LUA|FONT|NAVIGATION|Navigation|Menu|RAIL|ANIM|LIVE-SUBLAUNCHER)-/) return "UI, theme and native skin";
    if (n ~ /^(EPG|PVR|CHANNEL|CHANNELS|BROADCAST|EIT|OTA)-/) return "EPG, PVR, channels and broadcast";
    if (n ~ /^(POWER|SHUTDOWN)-/) return "Power and standby";
    if (n ~ /^(BOOT|BOOTSPLASH|COLD|STAGED|TAKEOVER|PLAT|PLATFORM|MODULE|LIVE|SERVICE|IMAGE|FULL|OWNER|SCHED|QUIESCENCE)-/) return "Boot, platform and services";
    if (n ~ /^VENDOR-/) return "Vendor inventory";
    if (n ~ /^(AGENT|WORK|TODO|OPENAURORA|REPLACEMENT|HW|REVIEW|DOCUMENTATION|AURORA|RTL|UPSTREAM|ARM|QEMU|FIXTURE|FAKE|TVSIM|HOST|RUNTIME|STABILITY|TELEMETRY)-/) return "Reviews, method and status";
    return "Other and miscellaneous";
}
function rankof(g) {
    if (g == "PQ and kernel tasks") return 1;
    if (g == "Boot, platform and services") return 2;
    if (g == "Display, OSD, DRM and GPU") return 3;
    if (g == "Panel and TCON") return 4;
    if (g == "Audio and amplifier") return 5;
    if (g == "CEC") return 6;
    if (g == "Source, VIN and V4L2") return 7;
    if (g == "Tuner, NIM, DTV and SPI") return 8;
    if (g == "IR") return 9;
    if (g == "Settings, DB and model") return 10;
    if (g == "App manager and apps") return 11;
    if (g == "Launcher, sub-launcher and tab policy") return 12;
    if (g == "UI, theme and native skin") return 13;
    if (g == "EPG, PVR, channels and broadcast") return 14;
    if (g == "Power and standby") return 15;
    if (g == "Vendor inventory") return 16;
    if (g == "Reviews, method and status") return 17;
    return 18;
}
{
    g = group($1);
    print rankof(g) "|" g "|" $1 "|" $2;
}' "$tmp.pairs" | sort -t'|' -k1,1n -k3,3 >"$tmp.sorted"

{
    echo "# Report map"
    echo
    echo "Generated from [artifacts/reports/README.md](../../artifacts/reports/README.md)"
    echo "by \`tools/docs/gen-report-map.sh\`. The canonical index has the dated"
    echo "one-line summaries; this map groups the same reports by subsystem for"
    echo "faster lookup. Links are relative to this file."
    echo
    echo "The full chronological index remains the authority for report dates,"
    echo "superseded claims and descriptions."
    echo
    awk -F'|' '
    {
        if ($2 != last) {
            if (last != "") print "";
            print "## " $2;
            last = $2;
        }
        print "- [" $3 "](../../artifacts/reports/" $4 ")";
    }' "$tmp.sorted"
} >"$out"

echo "wrote $out"
