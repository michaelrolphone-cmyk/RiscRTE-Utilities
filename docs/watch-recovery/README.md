# Exact Watch reconstruction source

This bounded bundle restores `f6215dc1e24b80cb378ad7cc355f5bf6c0bb53eb`, the exact Utilities source selected by the Watch 1.0.18/1.0.19 reconstruction recipe. Its tree is `0e8f3b43532bc953532eeea4bc60a8cb169dc351`, equal to public code checkpoint `c47405888a09ae9225d6a261b0421742025ae5ee`. Later commits fix CI pins and ancillary version inventory without changing these target inputs.

All 135 selected production inputs match the original recorded Watch17 qualification. The complete ancillary tree is not claimed identical to the lost original commit `5500c11778c638c9906f7b27813b72f7a00a089f`. This distinction is intentional; rebuilt full Watch18/19 images and paired payloads match their original recorded digests.

Run `python scripts/check_watch_recovery_source.py` in a full-history clone. It verifies the fixed bundle, imports it into an isolated fresh Git repository seeded only with public prerequisite `9ea379d7886f1a177e93651c1909d57a471bcd88`, then checks the exact recovered commit/tree and every recorded production input. To use the exact source locally, fetch `docs/watch-recovery/utilities.bundle` and check out the recovered commit. This is source custody, not a new release or binary.
