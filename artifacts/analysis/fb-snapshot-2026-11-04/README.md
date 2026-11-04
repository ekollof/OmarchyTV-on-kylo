# fb snapshot and cold-bundle evidence

Interpretation: [FB-SNAPSHOT-COLD-BUNDLE](../../reports/FB-SNAPSHOT-COLD-BUNDLE-2026-11-04.md).

- `host-check.txt`: full strict host `make check` (354 ok).
- `sanitizer.txt`: focused ASan/UBSan `check-fb-snapshot check-display`, clean.
- `cold-bundle-build.txt`: ARM `make cold-bundle` log.
- `cold-bundle-manifest.txt`: `sha256sum -c` over the 29 bundle entries.
- `cold-bundle-files.txt`: bundle file list and top-level hashes.

The bundle was built entirely offline. It was not staged or executed on the
TV. TV access is currently revoked by the owner's AGENTS override.
