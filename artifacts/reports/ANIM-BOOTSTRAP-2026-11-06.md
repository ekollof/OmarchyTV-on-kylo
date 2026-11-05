# Vendor animation bootstrap recovered — 2026-11-06

UI-ANIMATIONS previously recorded that `UIAnimationController` starts disabled
and that its bootstrap caller was unrecovered. That gap is now closed.

## Firmware path

`DemoApp::checkUIDesign` (0x00cc5e64) constructs `SVGDesignChecker` with the
app's `SVGContext` and calls `checkAnimation()`; its result is passed to
`UIFramework::setAnimationEnable`. The call chain is:

```
DemoApp::checkUIDesign                 0xcc5e64
  SVGDesignChecker::checkUsabilityStyle 0xd6bfe4
    SVGContext::getSVGSupplier -> resource 79 (DesignStyle.svg)
    RectangleContainer::getRectangle("animation")
  SVGDesignChecker::checkAnimation      0xd6c4e0  -> byte this+0x0a
  UIFramework::setAnimationEnable       0x113bf58
  UIFrameworkTask::setAnimationEnable   0x1146d54
  UIAnimationController::setAnimationEnable 0x11397e8
```

`checkUsabilityStyle` also checks the marker rectangles `longButton`,
`launcherMenu`, `progressBarInplaceOfSlideBar`, `mixedProgressBarSlideBar`,
`slideProgressBarWithBall`, `dynamicCenterAlign`, `settingsCircularNavigationEnabled`
and `tabJumpEnabled`; the animation flag is the `animation` rectangle at
`this+0x0a`.

The TV's BinSVG resource 79 is `DesignStyle.svg` (CRC32 `0x131669a4`, record
size 30644) and contains:

```
animation -480 -270 1920 1080
```

So `checkAnimation()` returns 1 and the vendor enables the controller on this
set. The 300 ms launcher/sub-launcher records and the 1000 ms dialog controller
are therefore active in stock, not merely lab metadata.

## Implementation

`ui_vendor` now derives the enable decision from `ui.theme`:

```c
UIAnimationController enabled =
    theme_has_animation(c) ? 1 : 0;   /* DesignStyle.svg -> "animation" */
```

`ui.anim_enable` remains an explicit override for tests or other themes. The
decision is re-evaluated when the theme provider appears/disappears and during
rebind.

## Verification

New `tests/ui_anim_bootstrap.c` builds synthetic DesignStyle packs with and
without the marker and loads the real `ui_theme`, `ui_anim`, `ui_vendor`,
`ui_engine` and recorder display:

```
ok ui_anim bootstrap: DesignStyle.svg animation marker and explicit override
```

Cases: marker present → enabled; marker absent → disabled; absent marker with
`ui.anim_enable=1` → enabled; marker present with `ui.anim_enable=0` → disabled.

## What this does and does not prove

Proven: the vendor bootstrap/enablement predicate for this TV and our matching
policy, with a real host module graph.

Not yet implemented: our custom rasterizer has no vendor `SVGButton` graph, so
the 300 ms `#scalerect`/`#glyph` launcher/sub-launcher animation is not drawn on
the portal/apps screens. The 1000 ms dialog controller remains behind
`ui.launch_anim=1`; vendor Select has no transition (SELECT-PATH), so that path
must not be enabled on Select. The sub-launcher grid/transform origin is still
undecoded (SUBL AUNCHER-LAYOUT-2); do not guess it.

Next: use the now-known enablement to port the 300 ms widget animation only
after the sub-launcher grid and transform origin are decoded, then compare a
real vendor transition.
