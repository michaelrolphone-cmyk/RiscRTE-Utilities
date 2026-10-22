# Frozen Watch charging qualification integration repair

This additive repair is based on PR91 head
`12a9ede9fd93ca4793d18dd69f37df5f5aa33640`, not current main. PR91 and PR90,
including their original source checkpoints and recovery archives, are unchanged.
No production C, fixture assertion, app manifest, product pin or accepted binary
is changed.

## Diagnosis

Failed [run 39621395883](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39621395883)
checked out that exact Utilities head and System
`4cf36b1c46641b00d88535eb9a0e9b0797928aff`. The failure in
`battery_power_test.c` was inherited from PR90, not introduced by PR91:

- PR90 source `76bde74fee5455dca7d58110b26d813fd9009740` and PR91 contain the
  identical power fixture, SHA-256
  `4b19e7589081d636174fdfcf7a74d9ebf806044ad8b9080e0fcffdc4a730faf4`.
- That correction uses `script_step()`, an unsigned elapsed-time step supplied
  by the recorded frozen System `96742a20af660808cae6cd3a36eadd12a83a460f`
  fixture. This keeps telemetry transitions, input and timeout in the same
  25 ms script-clock domain, independent of raster touch-poll frequency.
- The workflow still selected older System `4cf36b1`, whose fixture is based
  on poll count and does not declare `script_step()`. Its missing declaration
  also causes the subsequent int/unsigned comparison diagnostic.
- Adding a declaration/cast or substituting poll counts would hide the cohort
  mismatch instead of preserving the time-domain assertions.

The workflow and target-builder dependency now both select the recorded frozen
System `96742a20`. The workflow additionally runs the existing charging suite and
uploads both suites' ordinary-mode frames. The artifact name uses the actual
checked-out Utilities head, not the synthetic PR merge SHA. Three regression
tests guard the dependency, suite inclusion and evidence identity.

## Verification and scope

Local preparation on the PR91-derived tree passed:

- 20 unchanged power scenarios in ordinary and ASan/UBSan modes: 40 runs.
- 14 unchanged charging scenarios in ordinary and ASan/UBSan modes: 28 runs.
- Existing utility app and failure fixtures, including the calculator oracle.
- All 89 Python unit tests, including the three integration regression guards.
- Diff whitespace validation and an empty production Apps/lib/sdk diff.

The checked-out System source was clean and exactly `96742a20`. Local sanitizer
runs used `ASAN_OPTIONS=detect_leaks=0`; this is not a LeakSanitizer pass. Logs
are included next to this report. No local target compiler was available.
New GitHub exact-head target/transport results must be evaluated separately;
these local results alone are not a remote CI pass.

The target build is explicitly a **non-distributable frozen power-profile
structural/import/export qualification ELF**, not the broader Watch 1.1.46
producer recipe. Its artifact name includes `qualification-only`; its build
record sets `distributable` and `shipped_battery_reproduction` to false. No new
release, product integration, accepted-byte replacement or hardware result is
claimed.

## Immutable accepted reference

The previously recorded Watch 1.1.46 Battery 1.1.3 remains 155,448 bytes,
SHA-256 `fdcb36ebee771f968c6febe53e1817a712276cd5c54752ad8250b8ecf32c1c6c`.
Its inherited producer remains `24270c36cd4bc1edb7bb2e8d860501eaa5105d43`,
with Utilities `ca7fe82f43adc4ad8a8fa477a5336c8f5c693770` and System
`96742a20af660808cae6cd3a36eadd12a83a460f`. That comparison is untouched.
The frozen power source still hashes to
`d955bea31acedaeeb646c0db6c26abf58fb1d0d6fe53963bb6784f0718ae8563` and the
adapter to `98f705ebcbe3ea7fdcb270354d93108555b0d96b95eb86c54281d5f35318b249`.

Original charging recovery is retained at
`docs/evidence/publication-20261021/charging/evidence.tar.gz`, SHA-256
`6b394024b71a955e7b22ba5534c2e0396aa3b9742bd8187f45c093b0d834c9b3`.
Physical charging/input-state correspondence, screen controls, full product
composition and shipped-byte reproduction remain open. No hardware action,
merge, release, tag, catalog or product-pin change is part of this repair.
