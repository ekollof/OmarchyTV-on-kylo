# Runtime skin selection and fallback — 2026-11-01

Implemented OpenAurora host policy, not an inferred vendor hardware API/effect.
The shared socket/stdin handler accepts `skin NAME` (asynchronous selection of
ui_NAME) and `skin` (copied active snapshot plus last transaction state). Names
are bounded letters/digits/underscore, no paths. Example replies:

    pending omarchy
    requested omarchy active omarchy state ready fallback 0 error 0
    pending missing
    requested missing active vendor state ready fallback 1 error 5

The engine adds standalone ui.skin v1: copied name/owner/entered/ready/error.
ui.engine remains v2. Entry plus an actual completed frame are required for
success, independently of whether ui.ok_file is enabled. Failed entry is visible.
The host identifies the active module by owner id, unloads normally and loads
the candidate. Missing/start/enter/first-frame failure drains that candidate
before loading ui_vendor. A NULL-returning failed init may still retain a busy
DSO; it is tracked too. Busy cleanup can remain pending indefinitely until its
owner releases resources. No --force bypass, overlapping fallback or retry loop
after failed vendor fallback. A second request while pending rejects EBUSY.

First-frame timeout is own policy: ui.skin_timeout_ms default5000 after start,
valid100..60000; invalid values use default. It does not force cleanup. Suspend/
standby reject while selection is pending; shutdown cancels without new startup.
Query state is the last transaction result plus current active skin, not a
continuous liveness guarantee after generic/manual module operations.

After autoload, main selects configured ui.skin when a compatible engine is
present, otherwise an existing skin or vendor. Missing and invalid configuration
fall back. Processes without the engine are unaffected. Autoload providers and
ui_engine; the selector can load the skin. Runtime override is memory only;
no config/firmware/boot hook is written. This is not process-crash recovery.

Verification: full strict host249 checks, focused ASan/UBSan selection/UI async,
and seven native ARM contract groups on TV pass. Cases cover startup, actual and
custom skins, bad/competing requests, start/enter/timeout, busy start/failed-init
cleanup, suspended engine and shutdown. Busy cases prove vendor is not loaded
before cleanup releases. `tests/skin_socket.sh` (explicit socat integration,
not a make-check dependency) runs real host processes/Unix socket commands:
missing/invalid startup, both-way switching, entry failure and clean quit pass.

Actual GPU run PID8795, foreground upload followed by separate size/hash checks:

    UI rail completed image revisions=11 over2000ms (completion-paced; not a frame-order measurement)
    UI selection mode=vendor-instant animation receipt=0 completed screen=1 identity receipt=17179869200
    UI reload id=24 completed with fresh receipt
    UI host-only suspend/resume completed receipt=17179869206
    UI control requested omarchy active omarchy state ready fallback 0 error 0
    UI control requested vendor active vendor state ready fallback 0 error 0
    UI control requested missing active vendor state ready fallback 1 error 5
    UI backdrop raw BGRA=24,23,22,153 expected24,23,22,153
    graphics host drained and unloaded provider
    restored bytes=16588800 yoffset=1080 MAU=0 region_match=1 snapshot_match=1
    probe_exit=0

This proves the control handler/module replacement on real rendering, not exact
scanout cadence, independent panel appearance, native IR, app/settings actions,
TV standby or cold takeover. Omarchy is still a skeleton. Vendor screens and
audio/CEC were not changed. No new independent owner visual confirmation.

Isolated ARM: openaurora/build-tv-v3/skin-selection, maximum GLIBC2.17. Exact
identities are final-tested.md5; stage.md5 owns the reusable ignored bundle at
artifacts/workbench/skin-selection-stage-20261101. Engine MD5
1952912ce1149e530d0f9efc87867bf5, probe faac5568fd850e8ef672b20be7cc50c3,
unit8c31bed0ec39b26dc398edd3e08a6cf0. Source20 modules versus preserved matching
production16/installed ABI1 are separate. No persistent deployment.

Before/after full backing MD5 remains41735daa69dc137743b59efe12db4a19, pan0,1080.
Same boot71f7eb63-727f-4b9a-a9c7-2b04dc3f9197, stock1785/zombie1967, hook/hosts
hashes, SSH22/telnet2323, Samba REJECT and safety markers. All staging/backups/
readiness/helpers/unit configs removed. Raw: analysis/skin-selection-2026-11-01.
