# power: durable source and evidence checkpoint

Published 2026-10-21. Source commit: `76bde74fee5455dca7d58110b26d813fd9009740`. Original tested checkpoint: `1f80de804698972780b9f6be5cef1665dda03680`. Identical complete source tree: `49c774c3c9190fe87aa4b5f6228f86c83b819f6f`.

This branch preserves existing work, not a firmware release or completed product feature. No new target build, device test, merge, release, or deployment was performed by publication. Historical results and acceptance gaps are in qualification-report.md. Prior local-only/publication-held wording is retained as history and does not describe the new GitHub checkpoint.

## Evidence

`evidence.tar.gz` contains the exact original incremental Git bundle, binary-safe patch, technical source/command receipts, logs (including preserved failures), provenance and available rendered screenshots. SHA-256: `f1a5a980cbb40c0a079e7e607796de18bc36a30432b846b09a96c47c4bdd757b`; 62779 bytes; 28 files.

The archive's source-checkpoint.json lists every included technical file with SHA-256 and every excluded generated binary/object or duplicated archive. Existing historical SHA256SUMS may name omitted generated files; the new inventory specifies what is captured here. No private task-tracker snapshots are included. Exact original commit restoration requires its original parent; restore stacked parent bundles first. Remote API commit metadata differs, but its source Git tree hash is identical.

## Dependency and qualification boundary

Original parent: `ca7fe82f43adc4ad8a8fa477a5336c8f5c693770`. Proposed PR base: `tasker/watch-power-frozen-base-20261021`. Read source-checkpoint.json and the qualification report before integration. Host/sanitizer evidence does not establish target, radio, physical display/touch, product composition, or deployment acceptance.

## Supplemental historical evidence

`checksums-verified.log` is preserved alongside the unchanged archive (SHA-256 `b72f33bc263135677db23c83a4c4c110cea55b5783f5a898283ddc9ce815de59`). Historical publication-held wording and checksum receipts describe the original observation. Checksums may name omitted generated or tracker files; those files are not newly included by this receipt. This checkpoint does not authorize a merge or release.
