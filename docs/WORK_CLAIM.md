# Utilities independent build claim

## Previous checkpoint

The initial independent build and historical release-byte profile were completed by [PR #2](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/2) on 2026-10-01. The pinned standalone build, host fixtures, release-byte parity and readiness record remain part of the external repository.

## Current claim

The active current-master refresh is tracked in [claim issue #5](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/issues/5), branch `parity/utilities-reader-3d9bc4-20261002`, based on Utilities main `4343b808f762b67bb14c39fb50f28d32819af07e`. Scope is published GPS 1.0.1, LoRa 1.0.1 and Battery Status 1.0.2 parity with Reader master `3d9bc4f373679f5ae8dd184db6a8d0afa5a40231`, including exact artifact provenance and target CI validation. The issue is the durable claim status; current master parity does not grant U1 ZIP/cutover, release publishing, catalog, source removal, deployment or flashing authority.

The claim record is closed by merge only after exact-head target CI passes, source and base are rechecked, and post-merge target CI is verified. Hardware runtime gaps remain in [readiness](MIGRATION_READINESS.md).
