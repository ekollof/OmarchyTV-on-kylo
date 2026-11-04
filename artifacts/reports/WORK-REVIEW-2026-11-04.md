# Review of the later UI/launcher/display work — 2026-11-04

Reviewed cf78486..4ba1599 against source, the shipped ARM instructions and
the focused native-reference fixtures. This fixes implementation and evidence
gaps; it does not close general SVG raster, downloaded-PNG, settings, stock
apps-grid appearance or independent-startup acceptance.

## Scrollbar corrections

Fresh read-only Ghidra decompilation **and ARM** are preserved in
../analysis/work-review-2026-11-04/scrollbar-firmware.txt and
scrollbar-constructor.txt. Source ELF MD5 remains
64c252340d3af6c5d66072611b7bad7a.

The field at+6c is SVGWindow's **four-state UFA/FA/UFI/FI index**, not
horizontal/vertical orientation. Constructor10c3598 assigns resources
62/61/64/63 at+84/+88/+8c/+90. Cache slots are+74/+78/+7c/+80. The former
two-entry arrays and two-state fixtures omitted inactive states and could
index beyond their arrays. The owned skin now has four entries and an explicit
widget_state word; values outside0..3 return without indexing/callbacks.
The native skin corpus covers32 cases; render covers36 across all four states.

calculateBarSizeAndPosition10c3af0 uses low32-bit add/sub/mul before signed
comparison and signed division. Signed C expressions overflowed before the
divide, and visible+first could reverse a branch through undefined behavior.
Explicit unsigned wrap/reinterpret helpers now preserve ARM semantics,
including INT32_MIN/-1. Geometry's branch order ensures its divisors are
nonzero; no crash or zero-divide behavior has been invented. The native
geometry corpus grows to14,576 total state/geometry cases with signed extremes.
Both scrollbar native groups now execute the shipped660-byte divider rather
than a C division stub. This reuses the earlier verified shipped_divide loader.

renderBar10c3bac calculates geometry from the **first** window query, then
queries again to place/size the surface. The implementation had queried twice
and calculated from the second result. Constant fixtures hid that error.
A changing-window fixture now independently compares the two-query trace and
resulting rectangles against firmware. Render/min-cap additions/subtractions
also use explicit ARM wrap. The background-rendered flag's public comment is
corrected: an already-set flag suppresses background rendering.

## Launcher advertising exclusion

The new adapter queried and copied three advertising/tracking URL fields,
and both row conversion helpers propagated them. No delivery or tracking
network operation was found in this path, but copying that data into the
replacement contradicts the owner's explicit scope.

The SQL projection now omits those columns. The recovered900-byte source
region is reserved/zero, keeping ordinary field offsets without implementing
the advertising data model. copyLauncherInfoToItem assigns empty strings to
the three excluded destinations, and LauncherItem construction default-
constructs the corresponding three UStrings, even with a foreign input object.
Ordinary app names, thumbnails, app launch URLs and TV metadata remain.

The native fixture retains the vendor's actual copy operands, checks them,
then normalizes **only the three excluded fields** in the expected result.
Ordinary fields/call order/path/refcount continue to compare unchanged. This
is an explicit privacy deviation, not full byte-for-byte firmware parity.
Host tests use populated excluded DB columns and poisoned reserved input
bytes, checking that none reaches the returned rows or UI objects.
image-launcher-firmware.txt preserves fresh source evidence for these mappings.

The launcher-items CLI also checks strtol's ERANGE. On32-bit ARM an
overflowing decimal saturates to INT32_MIN/MAX, so checking only the returned
value's range accepted invalid parent IDs. Positive/negative overflow cases
now pass both host and a separate ARM/qemu service/CLI integration run
(arm-cli-runtime.txt).

## Image-create byte and callback boundary

SVGImage::imageCreate10c9430 tests its+0xb0 flag with ldrb at10c94d4.
The abstraction returns a word, and the implementation treated nonzero upper
bytes as true. It now truncates to uint8_t. Both host and native fixtures
populate adjacent bytes with0xffffff and vary the actual flag byte0/255.
The point-create overload and surface-create helper also validate required
callbacks before producing partial effects, matching the prior rectangle
overload's defensive boundary. No native failure policy is inferred from
these caller-validation guards.

## Apps lifecycle and shared asset cache

- The same-context stop/start retained grid_count, so explicit cards or a
  different DB could leave old grid rows. Count now resets before loading.
- The snapshot omitted grid_sel. New5-byte snapshots preserve it; restore
  accepts legacy4-byte snapshots and validates the added index. Old binaries
  do not read the extended snapshot, so this is backward read compatibility,
  not a guarantee that downgrading a live snapshot to old code works.
- The64-card cap applied before sorting, dropping later-inserted items that
  should sort first. Bounded insertion retains the globally earliest64 by
  order/id while scanning all rows. Overlong asset names are rejected instead
  of truncating to a colliding name. A70-item reverse-order fixture uses an
  independent SQL ORDER BY reference.
- The immediate scrollbar raster adapter's clear callbacks do not erase
  surfaces. Painting a track before hiding a full-height bar left a spurious
  track. A non-overflowing page now omits the entire scrollbar.
- The engine's64-image cache could not hold64 grid cards alongside other
  screens. The bound is128; an integration case preloads32 other-screen
  assets and browses all five pages, checking the expected card images.

Regression cases also verify page selection across real DSO reload and
switching to one explicit card via same-context stop/start. These are layout,
state and asset-selection tests. They do not establish vendor navigation,
live DB refresh, downloaded PNG decoding or stock pixel parity.

## Supervised display and watchdog evidence

The no-plat module previously still linked vendor HAL libraries and accepted
display.drm_init=1, despite being described as an initialized-layer-only
binding. It now has no vendor-HAL dynamic dependency, rejects that option
before open, cannot invoke global mdev discovery, and reports its own metadata
name display_fb. Its wrapped-device tests cover composition/commit failures
and the early HAL-init rejection. A loader-only listing on the real TV resolves
libm/libc/the interpreter without HAL libraries.

This binding still changes framebuffer/region/pan state when actually used,
and ordinary stop does not restore a complete prior hardware snapshot. The
earlier stock screenshot proved stock output returned, not full state equality.
A future visible test needs an explicit restoration owner. This review did
not draw, initialize HAL, send keys, stop stock services or perform takeover.

The claim that a historical FreshWDT return1 proves rejection or makes native
watchdog maintenance impossible was unsupported. It is withdrawn at the
source comment and in OA-CANDIDATE/canonical docs. The actual MCU authority,
return contract and paired lifecycle still need firmware investigation; the
earlier cold reset is not permission to retry. No independent-startup or
watchdog-disable plan is established by this review.

## Verification and current limits

Evidence lives in ../analysis/work-review-2026-11-04/:

- host.txt: full strict host checks; sanitizers.txt: focused ASan/UBSan for
  images, scrollbar, launcher DB/conversions/services, apps lifecycle/cache
  and both display variants. Required c11/Wall/Wextra/Werror/pedantic retained.
- native-reference.txt: fresh isolated build-tv-v3/work-review reference,
  executable SHA256 printed before qemu execution; all42 groups pass,
  including14,576 geometry/state,32 skin,36 render,80 image cases and the
  two8-case ordinary launcher conversions with explicit privacy differences.
- ARM build logs and stage manifests identify native binaries; vendor assets,
  ELF/toolchain and build products remain local.
- TV runs execute only focused CPU units, an ordinary read-only LauncherItem
  query and the no-plat DSO loader listing. The live DB returns54 rows,
  parent0=27 and parent1(including deleted)=24. Upload size/hash are verified
  in a separate connection before execution. Staging is removed and exact
  executable-path inspection finds no helper. coredumpnoreset is retained.

tvssh.sh needed WoL for access and again for the final cleanup reconnect.
One local SSH connection stalled before authentication and was terminated;
the bounded retry completed, with tv-cleanup-final.txt confirming removal and
no-reset retention. Do not treat the incomplete tv-cleanup.txt as success.
Stock Aurora live PID1785 and zombie1967 were observed in the initial run.
No screen/settings/network/service/boot/persistent write
was made. There is no new display acceptance claim. The remaining major gates
are live widget/rendering/PNG integration, stock visual/interaction parity,
hardware ownership and cold startup; the successful original candidate
screenshot remains only the narrowly described historical scanout test.
