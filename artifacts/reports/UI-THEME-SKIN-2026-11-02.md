# First skin consumer of ui.theme — 2026-11-02

ui_vendor now holds ui.theme v1 and selects its existing DialogLauncher
animation mode from the actual resource's named rectangles. The earlier
hardcoded present[8] list is replaced with provider lookup and the recovered
first-present rule. This consumer currently drives the existing explicit
ui.launch_anim=1 diagnostic path; it is not a new vendor launcher transition.
Settings/row/textbox geometry and visible animation parity remain open.

## Firmware and selection

Aurora MD5 64c252340d3af6c5d66072611b7bad7a. UI-ANIMATIONS already established
SVGDialog::create's table57..64 and startAnimation's mapping to HAL1..8. Fresh
read-only/noanalysis ARM/decompile and both tables are preserved in
artifacts/analysis/ui-theme-skin-2026-11-02/firmware.txt, because the earlier
create extract was not present in durable analysis files.

create10c5728 copies153b7ec, then10c590e..10c5966 looks up standard names and
starts the first whose rectangle exists. startAnimation10c5008 uses153b80c
to map57..64 to types1..8. Standard names in that order:

    animation_flip_fade_in
    animation_slide_right_fade_in
    animation_slide_left_fade_in
    animation_slide_up_fade_in
    animation_slide_down_fade_in
    animation_fade_in
    animation_grow_fade_in
    animation_bounce_fade_in

DialogLauncher.svg (index129/hash5c4ba740) has only the up marker. Its raw
rectangles are captured in dialog-launcher.tsv; the actual provider/skin path
selects type4. Marker presence, not element order or rectangle dimensions,
sets the mode. An available theme missing the resource or all markers selects
no effect. No-provider setups retain the previously verified stock type4
fallback for compatibility. ui.fs_type remains an explicit lab override.

MainLauncherMenuDialog::onSelectPress only changes focus; the existing
ui.launch_anim switch remains default0. This work does not enable a guessed
launcher transition or claim that the stand-in SCREEN_OPEN matches a full
vendor dialog. The existing draw/easing/controller implementation is unchanged.

## Consumer lifetime

ui_vendor acquires the optional provider at start/deferred rebind and releases
it at stop. service/removed clears its interface before a forced provider
removal can leave a dangling pointer; late publication and suspended replacement
are handled by the existing owned rebind timer. Resume retries missing theme
binding. Normal consumer holds prevent provider stop/unload/reload.

Mode resolution borrows the resource only during selection and saves one scalar
before starting the animation. No view/string is retained. An active animation
therefore keeps its selected mode if the theme disappears; next launch uses
the newly bound snapshot or documented fallback. Failed skin startup releases
its provider hold and animation binding while preserving a colliding peer.

## Verification

tests/ui_theme_skin.c loads real theme, engine and vendor DSOs, with a recorder
display and injected animation callbacks. It covers all8 modes, multiple markers
in reverse stream order, absent resource/markers, overrides, late start,
consumer hold, forced removal during an active animation, rebind, suspended
replacement and failed skin-start cleanup. Optional corpus paths exercise the
live pack through provider and skin, selecting mode60/type4. These are policy,
ownership and ABI tests, not a screen simulation used as visual acceptance.

Full strict host suite, focused ASan/UBSan, ARM/qemu and native TV checks pass.
Native set in build-tv-v3/theme (full identities in analysis sizes/hashes files):

| artifact | bytes | MD5 |
|---|---:|---|
| tests/ui_theme_skin |479640|e876bcf71623a2ad6333402a7184827f|
| mod_ui_vendor.so |161632|72c2d2ba71d5751a8542aff979354ab9|
| mod_ui_engine.so |57432|e307de666ee1ca3f6df3af656e85fc63|
| mod_ui_theme.so |47116|33609e021a52fab6c3e9b61b0459d6d4|
| mod_nav_display.so (test recorder) |24072|41e355212a5e9a1c36b1b49e92ec6c4d|

Test/vendor require at most GLIBC2.17. Separate successful size/hash verification
preceded execution on TV. Test reads live theme files and creates only its own
temporary synthetic records; it sends no TV keys, touches no display/devices
and changes no settings. /tmp/oa-theme-skin-20261102 and fixture directories
were removed and independently checked; no helper or job remains. Same stock
boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197/live1785/zombie1967, hook/hosts,
SSH22/telnet2323, Samba REJECT and safety markers. No persistent deployment.

Next derive actual settings rowItems/textbox geometry and style selection,
then adopt these provider views in the real widget renderer. UI-MASK-LAYOUT's
list-row-layout.txt preserves row renderBackground10bec08; UI-TEXT-SETUP's
README resolves renderItems d54400's text1/labelLong/etc literals but its older
raw /tmp extracts are gone. Follow the group transforms before substituting
any fitted label origins. The first theme consumer closes provider adoption
for marker policy only; it does not close those pending geometry/render gates.
