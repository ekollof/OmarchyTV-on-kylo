# Content show removes its own full-screen components — 2026-11-03

This resolves the primary+50 question left by TAB-WRITEBACK. It is a source
ownership finding; no renderer change or new live observation is claimed.

Same Aurora MD564c252340d3af6c5d66072611b7bad7a. show-path.txt in
analysis/content-lifecycle-2026-11-03 freshly decompiles/disassembles
SVGFullScreenDialog::destroyComponents d6d330. It visits its **own** five pointers
+140/+144/+148/+14c/+150. Each nonnull component is destroyed if created, its
deleting destructor is called, and its pointer is cleared. There is no parent
TabDialog argument or parent-header mutation in this routine.

The verified PictureSettingsDialog primary vtable has+4c=init e0b1a4 and
+50=this destroyComponents implementation. Its show e07a1c calls them in that
order and then marks the content showing1/focused0. This is a real cleanup of
the content object's full-screen components after initialization, **not paint**.
Its list/other member objects are not those five pointer fields. TabDialog's
own+140 title and tab buttons belong to a separate object and lifecycle.

This supports composing the parent header separately from content and avoids
introducing a second child title based on method-name guesses. That composition
is an ownership inference from the source, not captured-frame execution proof.
Keep UI-TITLE-LIVE-GRAPH/font/raster acceptance limits; this does not resolve them.
The fresh onRightLeftPress excerpt is supporting routing evidence, not a new
complete key reducer. The incidental getVideoPreview excerpt is not a next task.
