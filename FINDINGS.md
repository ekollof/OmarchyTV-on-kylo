# Current findings and correction map

Initial offline audit:2026-10-31. Subsequent dated reports add implementation
and TV evidence, including UI-SCENE-REVIEW on2026-11-01. This guides conclusions;
topic reports retain detailed provenance. See [report index](artifacts/reports/README.md).

## Evidence rules

“Implemented,” “loads,” “read back,” “historically visible,” and “works independently
at cold boot” are separate claims. Tests only support their exercised scope.
A failed probe may show a bad request or missing init, not an unusable API.

Use the TV's own firmware dump. Medion/sibling-kernel findings in old chronology
are not target evidence. Ghidra signatures and decompilation can be wrong:
check instruction arguments/return paths and relocations. Aurora's regular ELF
symbol table is stripped but rich dynamic symbols remain; nm -D works.
Symbol/function counts depend on the counting method and do not establish an ABI.

## Known red herrings and replacements

| superseded claim or tempting detour | current conclusion and evidence |
|---|---|
| Readiness must track the newest queued UI serial, or a retained old receipt is enough after reload | Track the first accepted request of that UI/provider generation. Any later actual successful receipt in that namespace can acknowledge it; old serials cannot. Chasing each new request can starve readiness under repaint/coalescing. UI-ASYNC. |
| The TV's installed OAI1 bundle includes the recovered rail-highlight animation | Its /buffer/openaurora/ui inventory still lacks rail_hl01..11. RAIL-ASSETS exports verified vendor frames to durable ignored workbench and tests a private temporary complete bundle on the real GPU; installed assets are unchanged. Visible frame order/30ms cadence remain open. |
| The original rail test proves a full asset bundle loops all eleven frames | It loaded only highlights. MAX_ICONS24 silently admitted just nine frames alongside title/five glyphs/nine cards. RAIL-ASSETS derives capacity29 from maximum content, reproduces the old failure and checks every frame/wrap with a full normal bundle. GPU image receipts do not establish scanout frame order/cadence. |
| A unique completion ticket also makes image cache revisions safe after consumer reload | GPU upload revisions must increase independently. scene_display uses host-owner-id revision namespaces and increments activation revisions when reusing an open provider; an old cached revision cannot identify a newly allocated canvas. SCENE-DISPLAY. |
| GPU enqueue success can replace synchronous display.present success or acknowledge UI readiness | scene_display publishes separate display.async request/latest receipts; its legacy present rejects ENOTSUP. Coalesced serials do not get success receipts. ui_fb must consume actual completions before writing readiness; that integration is still pending. SCENE-DISPLAY. |
| A client DSO's static ticket counter guarantees public ticket uniqueness across reload | Unloading can reset that counter. mod_graphics maps internal tickets to host owner-id/sequence tokens; actual DSO reload tests reject old tickets. GRAPHICS-MODULE. |
| Reaping a signaled GPU child certifies EGL cleanup or permits automatic recovery | Raw wait status proves process death. Graphics quiescence latches unproven cleanup and retains the faulted provider; native abnormal release/recovery remains a separate firmware investigation. Device-free fixture only; no actual GPU was killed. GRAPHICS-MODULE. |
| A sealed image cannot be mapped on the TV, or qemu's ancillary leak proves a kernel leak | Actual target mmap_region rejects VM_SHARED when sealing made the writable count negative, even for PROT_READ. MAP_PRIVATE passes with all seals retained. Excess-descriptor cleanup passes on TV; the small-control-buffer leak occurred only under ARM/qemu. SCENE-BROKER records both distinct findings. |
| Closing inherited fd>=3 in a directly Mali-linked helper is necessarily safe | Library constructors run before main and can own descriptors by then. SCENE-HELPER now tests an EGL-free bootstrap that closes inherited fds before loading the renderer. The renderer remains loaded until child exit; no async host service exists yet. |
| A10ms controller tick requires100Hz scanout; pbuffer readback timings describe direct GPU presentation | Controller sampling, display refresh and renderer pacing are distinct. Native EGL window/cache diagnostic achieved its50Hz target and owner-observed smooth full-screen effects, with complete restoration. Production async service/worker remains open. EGL-WINDOW. |
| Successful GPU rendering/readback means smooth native display, or the driver says a shared segment was created so remove it | Offscreen Mali tests pass, but1080p round trips cost about60ms/frame before display commit. Native-window/swap ownership is still open. Existing stock-owned key33787/shmid32769 merely had attachment timestamps updated; never remove it. EGL-SCENE. |
| A quad with vertical sides implies bilinear UV interpolation; geometry alone reproduces the animation | Vendor draws GL_TRIANGLE_FAN with w=1, uses GL_LINEAR texel sampling and an alpha shader/blend. Correct fan interpolation, filtering and fade are implemented; real fb1 commits/restoration pass, but general CPU transforms are slow. UI-SCENE-REVIEW. |
| Sending dialog progress to display guarantees frames appear; a v1 pointer clear also clears v2 | ui_fb now paints on every progress event without unrelated timers and clears both aliases before provider dlclose. Interrupt/failed-resume/provider removal regressions pass. UI-SCENE-REVIEW. |
| GetOPUGain can back up and exactly restore raw gain | Kernel getter reads cached fields, then reply handler remaps exactly100 to0x8000. Those raw values cannot be distinguished by this query. No DSP/audio ACK exists. SPEAKER-GAIN. |
| SetOPUGain accepts the UI's0..100 speaker-volume scale | Direct channel operands are bounded0..0x8000 and bypass Mod_Cell_SetVol's AQ/balance/ratio path. UI speaker out gain is a separate ALSA control; lower codec/client tests do not establish audible application. SPEAKER-GAIN. |
| posix_spawn return0 proves helper exec/startup succeeded; qemu return0 for missing exec is a bug | Target libc forks, returns PID/0 and can later exit127 after exec failure. Direct ARM instructions and minimal reproduction confirm it. Module handles delayed EOF/reaping; X_OK only catches obvious missing paths. MIXER-MODULE. |
| Moving I/O to a worker makes its service RPC nonblocking | Owner open/send/gain/receive RPCs still wait for the worker. Loop-thread bind_session now refuses all arguments until async completion exists. Quiesce no longer waits 500ms. WORK-REVIEW. |
| Animation service timer_add automatically belongs to the provider | Service calls inherit the consumer's current owner. ui_anim now registers its timer through its own owned post; resume restores the timer. WORK-REVIEW. |
| An empty post queue proves a busy shutdown gate cannot finish | A worker can post later. Busy gates may wait indefinitely; no-work errors concern non-quiescence holds. WORK-REVIEW/SHUTDOWN. |
| SSH closed, no root/RCE, TV offline | Historical pre-root/outage states. Current last verified rooted stock UI is in MEMORY and STABILITY. |
| Web OmarchyTV currently boots by default | Historically proven; now web/native boot disabled, oa_safe present, oa_boot absent. MEMORY owns state. |
| Every aurora.elf path names vendor code | artifacts/payload/aurora.elf is a shell boot hook. vendor:/com.nvt.idtv/aurora.elf is the ARM application. Check file type/provenance. |
| Any recovery image/stick builder restores current protections | Only the hashed recovery-stability pair is the tested current pair. Raw mtd13/pre-stability files are older. Artifact map distinguishes them. |
| Failed USB attempts proved malformed .scr syntax | Cause unestablished. Exact older 584-byte script succeeded. LED blinking is not execution proof. STABILITY. |
| Native replacement can be reduced to an HDMI launcher | No. Tiers order full parity work; only telemetry/ads are excluded by owner. ARCHITECTURE/TODO. |
| Revisit script.txt, IRsim or broken Robotest to obtain root | Root already exists; AURORA-INFLUENCE records those old dead ends. IR driver injection paths also do not prove active app startup paths. |
| fb0 BGRA but fb1 RGBA | All measured 32bpp OSD layers are BGRA. Old tvshot red/blue decoding was wrong; pre-fix captures need provenance. UI-PARITY. |
| Correcting BGRA means swapping every RGBA buffer or saved PNG | OAI1/blit inputs intentionally use RGBA; display converts to BGRA. Screenshot PNGs are already RGB. tvshot's separate RGB565 decoder and legacy splash writer were corrected offline. UI-PARITY. |
| 1.62/255 image error proves UI parity | One selected framebuffer comparison, not interactive behavior, all screens or visible independent startup. UI-PARITY/TODO-AUDIT. |
| Captured background RGB(14,14,13) proves the vendor portal is opaque | It does not establish alpha or composition over video. The owner observes our opaque backdrop differs from the transparent vendor rail. ui_fb clears opaque and canvas draws force alpha255; firmware uses SVG fill/opacity and separate background images. UI-TRANSPARENCY records initial decompile evidence; correction/real-video acceptance remain open. |
| Static screen parity permits omitting animations or UI flair | Vendor transitions, focus/selection feedback, scrolling and other UI/UX effects are required. List/dialog controller behavior is offline in mod_ui_anim; the text-slide caller is wired in mod_ui_fb with the vendor repeat. The full-screen progress mapping is recovered and encoded (`oa_fs_apply`), but no renderer consumes it; rail button mapping and frame sequences remain open. |
| `setFullScreenAnimationProgress` has no recovered mapping | `SVGDialog::startAnimation` @0x10c5009 maps dialog modes 0x39..0x40 through the table at 0x153b80c to HAL types 1..8 and starts a 1000 ms controller; `onAnimationProgress` feeds `min(1,1+(p-1)^3)`; `destroy` forces 1.0. The HAL transforms one 4-vertex clip-space quad: type 1 scales the right vertices (keystone), 2/3 shift x, 4/5 shift y, 7/8 scale. `SVGDialog::create` tries the static mode table 57..64 in order and starts the first whose animation widget exists. Display v2 now warps an offscreen scene and ui_fb consumes it; the widget binding for our stand-in dialog is still a config selection. UI-ANIMATIONS. |
| The category rail animates its item positions | `MainLauncherMenuDialog` moves items synchronously; the five category buttons get `setMainlauncherHighlightId` in `init`, and `renderForeground` creates the highlight image at the dialog's `mainlauncher_highlight` rect. `DialogMainMenu.svg` button windows are `(6,y,160,160)`; the capture places the 210x210 highlight at `(focused_x-5, focused_y+10)`. Implemented: 11 frames at 30 ms looping, playhead reset on focus. The 0.3 s scale records on 536/537 are launcher tiles, not the rail. UI-ANIMATIONS. |
| 0x96 in the animation timer is a 150 ms frame delay | It is the message id; the recovered controller starts immediately and schedules later ticks after 10 ms. UI-ANIMATIONS. |
| Animation time is ordinary monotonic milliseconds with uint32 rollover | FROST_Time uses a 90,000,000-ms period and a 12,343 correction quantum with strict comparisons. ui.clock and ui.anim are tested offline. Absolute live vendor phase remains unverified. UI-ANIMATIONS. |
| Theme linear mode means every vendor transition is linear | Dialog progress is min(1, 1+(p-1)^3) over1000ms; list selection truncates a linear blend. Text slide has its own pause/rate (800ms,50px per1000ms); the category rail uses the recovered11-frame30ms APNG loop. UI-ANIMATIONS/RAIL-ASSETS; visible cadence remains unaccepted. |
| The text-slide repeat is a repeating timer | `SVGTextBox::onAnimationProgress` @0x10c8e48 arms one 2000 ms timer when progress > 0.99 (0x10c8ee8) in state 3 (`setTimer(target,2000,200)` → `CabotTimerStart(handle,2000)`); the next pass restarts at margin 0. mod_ui_fb re-arms only after that pass completes. Only the 10 ms framework sample repeats. UI-ANIMATIONS. |
| svg.bin has length-prefixed names and no vector/animation data | It is a typed binary BinSVG stream with NUL-terminated ids, vector primitives and animation records. Outer record lengths come from the index. The bounded reader walks the entire target pack. UI-ANIMATIONS/UI-ASSETS. |
| BinSVG hashes require guessed widget names | Exported svg_name_list resolves all 702 names/IDs; CRC32(name) matches every index entry. All 39 animation descriptors are named; ad-banner effects remain excluded. UI-ANIMATIONS. |
| WIDGET_STATE uses FA/UFA/FI/UFI array order | Internal order is UFA/FA/UFI/FI (0/1/2/3); category code remaps {1,0,3,2} into its FA/UFA/FI/UFI resource arrays. Active and focus are independent. UI-ANIMATIONS. |
| Kernel fbs are created only by Aurora | KER_OSD_DrvInit registers five framebuffers. Registration, node publication and presentation differ. OSD-KERNEL-INIT. |
| 131/132 acquire/commit a framebuffer | Palette set/get, 12-byte {start,count,32-bit pointer}. Do not repeat descriptor permutations. OSD-BUFFER-CONTRACT. |
| Dumb-buffer mmap failures or mode_valid=0 rule out DRM | Driver CREATE_DUMB allocates only 4096 bytes; actual Aurora OSD uses private GEM create and ADDFB2/SETPLANE. OSD-BUFFER-CONTRACT. |
| Hal_Video_DrmInit/_kms_init or FlushPlaneInfo commits the OSD | ID discovery/cache path, not explicit SETPLANE. Separate OSD Update_Enable commits. DRM-PLANE-CONTRACT. |
| display.pan=0 means omit FBIOPAN_DISPLAY | It disables rotation only; every present must commit. Fixed/tested offline; not deployed/accepted on panel. OSD-BUFFER-CONTRACT. |
| Pixel readback/present success/ready receipt proves scanout | None proves hardware is displaying that buffer. Cold ownership, enable/crop/alpha/resume acceptance stays open. RECOVERY/TODO. |
| Doubled framebuffer metadata guarantees doubled mapped storage | Initial front requests S; Sync advertises 2*align4096(S). OSD commit retrieves GEM-derived address/size. Keep metadata, crop and backing extent distinct. OSD-BUFFER-CONTRACT. |
| Successful MAU0 query explains old invisible output | It only shows primary memory selected at query time. Earlier failure cause remains unproved. OSD-MAU-SELECTION. |
| Missing alpha/show/resume calls must be added to mod_display | Normal-plane blend-enable does no register write; SetPar sets global alpha. PanDisplay handles first show/region enable, and registered kernel PM restores cached state. Validate the existing fbdev path first. OSD-KERNEL-INIT. |
| ndp_osd_restore_plane_mode_set restores all OSD planes | It conditionally replays logical plane0 only. It is not an all-plane resume API. OSD-KERNEL-INIT. |
| dp_order/stacking is the established visibility fix | Previous invocation reset the set; no cause established. Do not retry as a guess. DRM-PLANE-CONTRACT. |
| Watchdog requires cold NDAL STBC calls | That path reset the set. pcbconfig/wdtctl packet path was recovered and tested; no further takeover currently authorized. PANEL-TAKEOVER historical evidence plus RECOVERY. |
| Watchdog disabled means recovery guaranteed | Separate supervisor has fake-hardware tests; it cannot recover every kernel/hardware/supervisor failure. Real cold takeover unverified. RECOVERY. |
| PWM Open five words / Set three words / all return void | Open six words, Set four; Init/Open/Set/Terminate scalar returns verified in ARM instructions. Close no channel. PWM-CONTRACT-RECHECK/header. |
| PWM nonzero means brightness changed; sweep channels until it works | A nonzero shortcut may skip ioctl. Board constants exist; trace profile/driver and ownership first. Panel independent acceptance open. |
| Reusable HAL counts 121/643/160 imply missing APIs | Defined unique dynamic function names recount: 321 Hal_* in NDAL3, 80 NDAL_* in NDAL_EX. Counts are navigation only. PLAT/DEV/ICE implementations stay inside Aurora. |
| TV uses musl / compiler glibc <2.34 guarantees loadability | TV is glibc2.23. Check actual required versions/closure, then preflight. Musl target is stub/sanity only. ARCHITECTURE/RUNTIME-CHECK. |
| RTLD_LAZY / loader exit0 means all imports work | Missing V4L2 imports were reported despite exit0. --check parses diagnostics; rebuilt closure passes. RUNTIME-CHECK. |
| IR key map recovered means UART protocol solved | NEC/RC/ICE/DFB/DIAL differ. RC field, repeats/releases and integration remain unverified; reading ttyS2 consumes stock input. IR-KEYMAP/TODO-AUDIT. |
| Standalone dialserver means no Aurora needed for TV services | Transport runs independently; execution/state still use vendor engine. DIAL/WS/DLNA replacement remains work. |
| mem/WoL test proves native standby lifecycle | Raw suspend/wake only. Host service/CLI coordination is now tested offline; device/wake/hardware acceptance remains open. POWER-STANDBY-CONTRACT. |
| idtvSuspend/Resume are optional empty middleware hooks | They own wake-network/PHY policy, audio/tuner/VBI, UART synchronization and device restoration. POWER-STANDBY-CONTRACT. |
| MCPU_MonitorTask_Reset disables the hardware watchdog; Uart_SuspendSend sends an MCU packet | The first changes mutex-protected vendor monitor state; the UART send pair waits/signals a sender semaphore. Neither is a guessed MCU packet API. POWER-STANDBY-CONTRACT. |
| Ethernet 0x89f0 opcode 1 immediately powers down the link | It stores next-suspend policy. Value 0 keeps PHY powered; value 1 requests PHY shutdown during driver PM. Other opcodes can write hardware directly. POWER-STANDBY-CONTRACT. |
| HAL call did not crash, therefore audio/source/platform works | Audible output/routing/ownership and omitted routing operations remain open. TODO-AUDIT and current modules. |
| AD82586C symbols/config references identify this TV's main amp | Verified live descriptor/object/vtable selects one ESMT AD82587_D (id1). AD82586C is another supported driver; subwoofer helper names are not main amp selection. AUDIO-ROUTING. |
| Decoder main volume, speaker gain and amp volume are interchangeable | Separate controls/scales: decoder opcode33, per-speaker EasySet items42/45/4f/4a, amp I2C register3. Registered speaker0 does not prove native audible/mute parity. AUDIO-ROUTING. |
| AD82587 mute is another normal SoC GPIO; vendor address68 is seven-bit | Mute resolves to physical208/STBC8, reset to normal GPIO114. I2C bus3 uses seven-bit34 (vendor eight-bit68). Fixed register read passed; no MCU/reset/write acceptance. AUDIO-BOARD. |
| Both READ flags in vendor I2C messages prove a malformed transaction | Target PreInit adapter takes the first buffer as subaddress and second flag as direction. Both messages completed on real hardware. Generic adapter assumptions do not describe this custom path. AUDIO-BOARD. |
| Hal_Iic_Read success proves all data transferred | NDAL only checks negative return; disabled bus can return0. New fixed probe requires count2 and returned1b/05/05 on the actual amp. AUDIO-BOARD. |
| UI volume is a percentage to write directly to amp register03 | The traced path selects speaker gain through101-entry AQ DSP tables. New decoder matches all101 live entries; amplifier and decoder-main-volume controls are separate. Complete policy/integration remains open. AUDIO-VOLUME. |
| PLAT_AOUT_SetMainVolume success means volume was applied | ARM instructions only range-check r1<=100; no device write. Use the recovered speaker-gain path, not this misleadingly named wrapper. AUDIO-VOLUME. |
| Correct scalar audio ABI means return0 proves a gain/table command succeeded | Message-backed Hal_Audio_Send returns0 even after sendmsg failure. Kernel receiver failure does not reach that wrapper. Preserve explicit transport/completion/lifetime checks. AUDIO-GAIN-TRANSPORT. |
| ALSA speaker gain readback certifies DSP application | Setter caches the request before lower calls and returns0 regardless; getter reads that cache. Real-TV metadata/value query passed, but audible/DSP acceptance remains open. AUDIO-GAIN-TRANSPORT. |
| Fresh-process ALSA hw:0 lookup failure means no speaker control exists | Vendor ALSA requires ALSA_CONFIG_PATH=/vendor/etc/alsa.conf, set by run.sh. The unchanged diagnostic then returned one integer control, range0..100/value0. --check does not validate resource configuration. AUDIO-GAIN-TRANSPORT. |
| Mixer table pointer must remain valid indefinitely, or full send means operation success | Target netlink invokes the callback synchronously; the receiver copies the table before return. Callback results are discarded. Retain through the syscall; dispatch is not an operation/DSP/audio ACK. AUDIO-NETLINK. |
| Any fresh netlink port can receive a mixer reply; HAL socket can be duplicated freely | Reply helper requires a real process id matching the bound port; HAL same-pid binding can conflict. Native integration needs coordinated ownership. Fresh-process GetOPUGain passed. AUDIO-NETLINK. |
| _cmd_capability_p or GermBeacon is a harmless query | The first sets bypass capability; the second loads/reloads command text. Neither was executed as a query. AUDIO-NETLINK. |
| Hal_Audio_Ctrl/Protect/Mixer/Proxy have void returns; audio should repeat init | Instructions show scalar returns. Shared vendor_audio.h and single plat prelude ownership are implemented/tested offline; routing/partial-init restart remain open. CEC-AUDIO-CONTRACT. |
| TV sends Initiate ARC C0 to the amplifier | Audio System is ARC receiver and sends C0/C5; TV is ARC transmitter and sends C1/C2/C3/C4. Old CEC-SPEC direction/conformance claims corrected against 1.4. CEC-AUDIO-CONTRACT. |
| CEC audio volume 127 is maximum volume | 127 means unknown, 101..126 reserved, 0..100 percent. cec.audio stores Report Audio Status as external UI state and mod_audio refuses local volume and mute while System Audio Mode is on. CEC-AUDIO-CONTRACT, OWNER-WORKER. |
| Normal CEC frames use MCU UART / ttyS2 | Private V4L2 controls feed ntkdriver_nondovi CEC. MCU enable/wake/authority is separate; UART reads would steal input. CEC-TRANSPORT-CONTRACT. |
| NDAL Process_Command success proves a message was accepted | It returns ioctl success but drops a separate driver-result byte. Driver filtering also rejects some spec-valid frames while upper layers process them anyway. CEC-TRANSPORT-CONTRACT. |
| Bug-for-bug parity requires copying CEC spec violations | Owner explicitly requires spec compliance first. Directed OSD-name and broadcast System Audio Mode regressions override the vendor filter. The injected cec.audio route follows 1.3a System Audio and 1.4 ARC; native bus acceptance remains open. CEC-LIVE, OWNER-WORKER. |
| Missing /dev/ntkcec means no native character device is registered | Live /proc/devices lists major 229; temporary node open failed with EPERM under the stock exclusive owner. No independent raw initialization has been accepted. CEC-LIVE. |
| Steam Deck must use LA4; cache proves fresh replies | Live known device is LA8/PA1000; unseen LA4 later mirrors metadata. Three query transmissions accepted, but reply payloads/freshness were not captured. CEC-LIVE. |
| CEC operands only need 14 bytes of receive storage | VPL receive copies 16 bytes even on failure paths. Tested raw/VPL marshalling requires 16-byte storage and explicit status/metadata validation. CEC-TRANSPORT-CONTRACT. |
| NDAL CEC Init / Init(NULL) implements safe module reload | Kernel close stores fd 0 and passes a null close argument; open treats fd != -1 as already open. Native hardware ownership/lifetime must be recovered separately. CEC-TRANSPORT-CONTRACT. |
| vu_ioctl/vu_open are ordinary Linux system calls | They are PreInit's kernel emulation API. The same driver also registers a real Linux cdev; the new raw backend uses independent native fds. Offline authority and workers are in OWNER-WORKER. Hardware open remains unaccepted. CEC-OWNERSHIP. |
| Host exit can dlclose after one ignored quiescence failure | oa_host_run retries stop/unload after completion posts, module timers and module watches. A remaining module with no such path returns an error and stays loaded. SHUTDOWN. A failed CEC close does not post, so that wait is intentional. OWNER-WORKER. |
| Failed CEC hardware close can be followed by discarding the fd/session | The raw backend and owning worker retain fd and state. Each quiesce generation retries close once. The worker does not exit or call on_settle while close fails. Only a successful hardware close, or the client's final OS close for audio, consumes the fd. CEC-OWNERSHIP, OWNER-WORKER. |
| One yielding quiesce is enough for an idle owner reload | A yield loop can return before the worker is scheduled, leaving cleanup_pending and blocking later suspend. Quiesce no longer waits 500 ms (see the corrected row above); an open CEC session still returns immediately, so a failed close does not stall shutdown's poll. OWNER-WORKER lifecycle follow-up. |
| Flattened language inventory contains proven IDs/Dutch slot | 56,715 text lines are an inventory; original IDs/grouping/alignment not recovered. UI content work remains. |
| Text-slide margin returns to zero at/after the duration | Forward direction ends at **distance**; `margin = (int)(distance×frac)` with `frac` clamped only above. Only the reverse flag (`distance − margin`) ends at zero. Re-read of instructions at 0xef0264, 2026-11-01; helper constants pause 800 ms, divisor 50, ×1000 confirmed from 0xeefc54/0xeefc8c/0xef0260. Implemented as `oa_anim_slide_duration`/`oa_anim_slide_margin` (`ui.anim` v2), host/ASan-UBSan/ARM-qemu tested. UI-ANIMATIONS. |
| Load average ~15 means the TV is struggling or something is leaking | Measured 2026-11-01: `runnable 1/496`, `CPU 97.6% idle`, and `D total = 15` — all of them vendor **kernel threads** (`ppid=2`): `_NerveMTask_`, `TCON_ARM_TASK`, `PLAT_WDT`, `DRV_DBG_UART`, `DRV_TVADC`, `MHL_MID_EvtThre`, `DRV_VD`, `_VPL_SRC_KADC`, `SRC_ModeTask`, `SRC_MonitorTask`, `VPL_SCLR_CTRL_M`, `DRV_VBI`, `PQ_MPFW_TASK`, `PQ_SHELL_DBG_TA`, `KER_CI_Timer_Ta`, all in `msleep`. This 4.9 vendor kernel maps `msleep()` to `schedule_timeout_uninterruptible`, so every sleeping driver thread counts as load permanently. Aurora's 239 threads were 0 in D; none were ours. Flat 13-15 all session incl. fresh boots. Not a health signal on this set — judge by runnable count, CPU idle and UI/ssh response. |

## What is established

- Root boot/access and privacy maintenance survived the tested recovery boot;
  internet consent must remain 1. Standby still closes services.
- ABI 2 worker ownership/descriptor checks, registry regressions and state-aware
  reload have host/sanitizer/ARM evidence. Suspended reload defers hardware start;
  restore failures reject/clean the candidate. Required-service/lookup dependencies
  now order lifecycle transitions and errors reach CLI. An internal sleep-cycle
  engine adds final entry/ledger restoration and nine fake-entry regression groups;
  seven wrapped-power groups also cover service/CLI and notification integration.
  Native device/wake/hardware acceptance and binary rollback remain open.
- Full-table status memory corruption was reproduced with ASan and fixed through
  bounded reply formatting. Full list/status socket replies pass host/sanitizer/
  ARM-qemu tests. Unix transport now has bounded nonblocking queues and five
  backpressure/partial-write/disconnect/input/cleanup regression groups on those
  targets; console stdout remains synchronous and is separate open work.
- Rebuilt core and eleven matching modules passed read-only preflight on real
  hardware from /tmp. The persistent installation remains ABI 1.
- Vendor OSD allocation and commit are recoverable from offline firmware; current
  evidence now replaces blind draw/allocation/stacking probes.

## What is still unknown

Cold scanout and resume ownership; complete IR behavior; independent panel/audio/
source/power/settings/services parity; remaining broadcast/CEC/apps/PQ/network
features; complete interactive UI and native OmarchyTV. See TODO for individual
acceptance tasks, not the “next experiment” paragraphs in historical reports.

## Useful provenance

- Raw OSD evidence: artifacts/analysis/osd-2026-10-31; supplied function names,
  hashes and instruction addresses are in the corresponding current reports.
- Runtime evidence: artifacts/analysis/runtime-check-2026-10-31.
- Firmware/assets/recovery identities: artifacts/README.md.
- The original chronology is preserved under artifacts/archive. Its confident
  prose and old checkmarks do not override this correction map.
