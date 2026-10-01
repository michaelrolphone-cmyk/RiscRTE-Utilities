# Utilities independent build claim

Status: released at validated implementation checkpoint (2026-10-01 03:58 UTC)
Owner: Utilities independent build maintenance task
Started: 2026-10-01 03:50 UTC
Base: c9e992a0be59d658d4cb044308c17d2d0bfd7ec2
Reader source: a5e2db59077cc889079668dc9cd7428b08bc32a1 (read-only)
Branch: parity/utilities-pinned-build-20261001
Scope: pinned standalone build, byte parity, focused fixtures and readiness docs. No open competing PR or claim found. No Reader/U1/GameBoy writes, release, cutover, deletion, deploy or flash. Release at validated checkpoint or blocker.

Implementation validated locally: all 3 actual ELF hashes match published payloads; 10 pipeline tests and 6 app interaction/failure fixtures passed. [PR #2](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/pull/2) implementation-head CI [36812887364](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/36812887364) passed all stages and uploaded development artifacts. Final checkout-ref/documentation-head CI and merge status are recorded in the PR/Actions. No further implementation writer is claimed by this record. Future work must recheck open PRs and claims; hardware, U1 and removal gaps remain in MIGRATION_READINESS.md.
