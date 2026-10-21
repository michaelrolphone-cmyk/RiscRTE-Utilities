# T039 candidate package 0.1.31

Status: additive implementation captured; exact-head CI qualification pending.
No package pass or installation readiness is claimed by this document.

This separate additive candidate descends from Utilities PR #94 at
`9a7a508d0a43572684c7feb64958eb31ee1fd583`. Existing files, PR #88/#94,
the accepted `0.1.30-dev1` identity, frozen gates and product/catalog pins
remain unchanged. The native manifest is separate at
`Apps/candidates/t039/ble_buttons.json` and preserves all sixteen capability/API
requirements, app identity, public ABI and provider behavior.

The numeric version `0.1.31` is intentional: the pinned production Runtime
`0aab2fd3b03b4190d638e23c79a465afeecf5dfb` accepts numeric major.minor.patch
only. Prerelease suffixes are rejected. Candidate/unreleased status belongs in
the surrounding evidence, not in a bypass of Runtime identity validation.
All fetched remote BLE Buttons manifest variants and tags were checked before
selection; none used `0.1.31`. No tag or release is created by this work.

The separate `scripts/package_t039_candidate.py` builder binds this manifest to the exact PR #94 native target, original
failed frozen parity result, native/temporal/queue regression matrices,
production Runtime structural admission, production manifest validation and
transported artifact custody. Only a passing run for the exact source commit establishes this software evidence.

A passing software candidate is not release approval, deployment, installation
acceptance, hardware/RF/real-host key acceptance, physical-device-fit evidence,
or permission to update product/catalog pins. There is no publisher in this
lane. Physical and final product integration acceptance remain open.

## Reproduction and distinct source identities

The candidate workflow `.github/workflows/t039-ble-package.yml` checks out the
package source at its exact event head. It separately rebuilds all original
PR #94 controls in a clean checkout at its immutable commit. The original
qualification-only contract is neither widened nor relabeled: its evidence
stays under `evidence/`, complete with `NOT_FOR_INSTALLATION.txt`, no version,
and its original parity failure. A new sibling `app/` contains the exact native
ELF plus the separate `0.1.31` manifest. All existing repository files must match
PR #94; only this lane's seven new files may differ. Every compiled Utilities
input is checked again against the candidate commit's Git objects.

The builder produces a deterministic `ble_buttons-0.1.31-candidate.zip` containing
the ELF, app manifest, candidate notice and pinned license notices. This is a
candidate payload bundle, not a product installer or published RTE release.
The manifest deliberately excludes `size_bytes` and `sha256`: production
Runtime rejects those extra app keys. Exact payload integrity is recorded in a
separate sidecar and source-bound package receipt.

The original structural ELF inspector/admission harness is reused unchanged
against the explicitly selected BLE Runtime, not its historical default. It
runs normally and with ASan/UBSan, retains missing-import, unsafe-lifecycle,
driver-role and aligned-allocation rejection controls, and measures existing
package/file/structural caps. These are not device PSRAM reservations.

The new metadata harness compiles pinned production JSON, identity and
AppDeclarations helpers unchanged. It checks the exact sixteen requirements,
numeric version, safe identity and filename; 40 negative controls execute in
each of normal and ASan/UBSan modes. The top-level metadata predicate is bound
to the extracted pinned `Runtime.cpp::appPolicies` source. This does not call
full private `appPolicies`, bind owner grants/providers, install, relocate or
execute Xtensa instructions.

After GitHub artifact upload/download, the package verifier checks every file,
the original qualification inventory, candidate source/manifest/ELF bindings,
parser/structural admission records, and exact ZIP entries. The producer's
receipt digest is supplied independently through the workflow output, never
trusted from a replacement receipt inside the downloaded artifact. Failed or
missing controls stop the build. No publication or workflow-dispatch route exists.
