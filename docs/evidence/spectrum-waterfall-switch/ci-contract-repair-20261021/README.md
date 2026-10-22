# Spectrum packaging-contract integration repair

The exact-head [CI run 39620790516](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39620790516) at `a5ff718923d57188f41502618f33f8e66296bb43` completed all three target ELF builds and pinned Runtime loader admission, then rejected the host matrix during packaging.

## Reproduced cause and minimal repair

The producer now emits 65 scenarios, but the packaging consumer still required exactly 61. All 650 normal/sanitized host executions across five display profiles passed. The consumer is updated to require the four already-produced full-history switch/delay cases for history indices 0 and 227. The exact-count, exact-case, duplicate, return-code and sanitizer gates remain fail-closed.

Only the packaging scenario contract and its tests change. No application behavior, target compiler/link flags, external/product pins, accepted binaries or manifests change. Prior source and evidence remain in the parent commits.

## Verification

- The unmodified downloaded artifact reproduces the old 240x240 host-matrix rejection.
- Read-only replay through the corrected consumer passes target validation and all seven host/geometry/existing-control receipts without changing the artifact.
- All 30 synthetic packaging tests pass, including 24 missing, duplicate and failed stress-case mutations covering all four cases in both sanitizer modes.
- The unit-test log's `Published` lines come from mocked publication tests; no actual tag or release was created.
- Fresh post-repair CI is pending; the local replay is not a substitute for an exact new-head CI pass.

## Prior linker abort remains separate

The earlier [run 39612929536](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39612929536) aborted in the retained legacy link with `collect2` signal 6 and a double-free/corruption diagnostic. The fresh exact-head run passed that same command. All 1,324 common staged target-input files match byte-for-byte; the legacy link command, retained recipe, compiler version log and baseline ELF also match. The previous failed build lacks the complete toolchain-hash receipt, so full installed-tool identity across runs is not claimed. This evidence supports an intermittent build-tool failure, not an application runtime diagnosis; its underlying cause remains unproven and no linker workaround is introduced.

`qualification.json` records immutable artifact hashes, target module hashes, source pins, observed host coverage and remaining limits. Hardware, live audio behavior and physical acceptance have not been qualified by this repair.
