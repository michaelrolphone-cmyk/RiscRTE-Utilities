# Utilities independent build claim

## Previous checkpoint

The initial independent build and historical release-byte profile were completed by [PR #2](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/2) on 2026-10-01. The pinned standalone build, host fixtures, release-byte parity and readiness record remain part of the external repository.

## Completed current-master refresh

[PR #6](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/6) merged as `9b4d2fef8a969011c13f37e8081255a068fc93d5` from Utilities base `4343b808f762b67bb14c39fb50f28d32819af07e`, against Reader master `3d9bc4f373679f5ae8dd184db6a8d0afa5a40231`. Claim [issue #5](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/issues/5) closed after verification.

Exact-head CI [37061519781](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/37061519781) and postmerge CI [37061677495](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/37061677495) passed focused host fixtures, all three independent ELF builds, and 3/3 required published-byte comparisons. No new versions or release artifacts were created.

Current-master parity does not grant U1 ZIP/cutover, release publishing, catalog, source removal, deployment or flashing authority. Hardware/runtime gaps remain in [readiness](MIGRATION_READINESS.md).
