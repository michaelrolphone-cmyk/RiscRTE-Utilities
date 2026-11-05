# Native Countdown 0.2.0 development release

The standalone `native-countdown-0.2.0-dev1` release packages the Countdown ELF already built and qualified by the exact-source dual-app native Alarms/Countdown recipe. It adds no product features or production source changes.

- Manifest: `Apps/native/countdown.json`, application ID `countdown`, file `countdown.elf`, version `0.2.0`.
- Intended distinct catalog identity: `RiscRTE-Utilities:Apps/native/countdown.json`, device package ID `app:countdown-native`.
- The legacy Countdown manifest/catalog row and all Alarms release identities are unchanged.
- All 15 manifest providers and deployment-owned bindings are required; preferences use namespace 1. No optional `app.catalog` grant is added.
- The existing qualifier validates six exact target modules, selector-off byte identity, four geometry/async host matrices, eight full-feature temporal/control matrices, provider captures, full compiled source closures, and normal/sanitized production loader/memory admission for both apps.
- The Countdown preparer first revalidates the complete shared qualification, then binds its own manifest to the exact Countdown ELF and existing production admission. The publisher revalidates every gate before and after release-asset upload.
- Read-only qualification runs on the exact branch/PR commit. The distinct publisher requires a successful qualification job and a push to `main`; its repository token is ephemeral and restricted to that job. It never runs the Alarms publisher.
- Direct download assets are Countdown-only. The evidence archive retains both apps' controls and the internal Alarms preparation record for provenance. That record does not publish or replace an Alarms release.

## Scope and limits

This is software qualification only. No target Xtensa instructions, physical device, installation path, deployment-linked memory fit, or hardware behavior is qualified. The `.59` firmware and companion payload are unchanged. This development app is not a replacement firmware bundle.

## Release gate

Run the full `Native Countdown qualification` workflow. Packaging requires a clean exact source and succeeds only after the existing full-feature target/host/temporal recipe and shared preparer succeed. Existing releases are immutable: changed source or bytes require a new release version. The publisher creates a draft, uploads all required files, downloads and verifies bytes and archive evidence, then makes the development release public. Catalog binding follows independent public-byte verification and uses exact repository, tag, version, ELF and manifest sizes and hashes.
