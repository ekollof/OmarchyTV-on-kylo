# Visible PNG/JPEG apps-grid evidence

Interpretation: [VISIBLE-IMAGES](../../reports/VISIBLE-IMAGES-2026-11-04.md).

- `candidate-visible.png`: 1920×1080 fb1 capture from the supervised candidate;
  live DB, empty `ui.dir`, `ui.download_dir=/buffer/downloaded_images`.
- `candidate-run.log`: five modules, `image_decode v2`, `display_fb`, and 51
  `painted apps grid: 18 items, first 0, scrollbar 0/600` lines.
- `openaurora.conf`: the exact candidate configuration.
- `stock-restored_0.png`: post-restore fb1 frame after stopping the candidate
  and restoring the saved pixels/offset.
- `jpeg-card-compare.txt`: hashes/dimensions of the two visible downloaded
  JPEGs and the cropped-card comparison metrics.

Local only, not committed: the fetched JPEG originals
(`/tmp/opencode/foxxum_1685`, `foxxum_3753`) are vendor assets and stay out of
history.

The test ran under the preceding TV-access authorization. A later owner
override revoked TV access before this README was written; no TV command was
issued after the override. Cleanup had already completed: candidate process
gone, bundle/ready files removed, vendor PIDs 1967/1785 alive, no-reset flag
present. Full display ownership/restoration and cold startup are not claimed.
