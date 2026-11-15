# Canonical clock helper adoption

## Source identity

Utilities base: `ce33416f6981270958da63780f18961b2f2ec9e0`.
Vendored `sdk/app/RiscRuntimeClockV1.h` is byte-identical to Runtime
`37fca3e2244bf71ac64bd8cd7e62eb94f4c09a9d:sdk/app/RiscRuntimeClockV1.h`:
SHA-256 `76584fdab224b184918d9f9cf675d5c4443adef7161e8518d456bc25277dc30f`.
`sdk/runtime-clock-source.json` is additive provenance; historical Runtime/profile
pins are not changed. This is a header-only helper, not an API-table or native ABI
change. Both existing SDK staging paths use `runtime_clock_sdk.py`, verify the
vendored hash and reject a conflicting same-name header rather than mix copies.

## Exact adopted consumers

- `lib/CanvasApp/canvas.c::now()`: presentation completion timeout, presentation
  token timestamp, and close/drain timeout callers all use this existing wrapper.
  The wrapper preserves its null/retained callback fence, cached timestamp, and
  `exiting` on clock-read failure. Helper output is committed only on success.
- `Apps/contexts.c::portable_contexts_training(true)`: learning start and redraw
  timestamps. Failed time reads still return false without committing timestamps;
  existing learning flags/state order is not rewritten.
- `Apps/contexts.c::cs_service_tick()`: one timestamp for learning timeout and
  progress/redraw cadence, including the optional native-time-toolbar branch.
  Read failure still marks learning failed and stops foreground training.

Zero is a valid reading; modulo-32-bit elapsed arithmetic and deadline thresholds
are unchanged. A present cheap clock callback is authoritative: false never
retries via health. Old table prefixes or a null suffix retain health fallback.

No genuine health metrics/policy consumers or authority-only checks were replaced.
In particular, the shared System `PortableContextsClient` health/policy/checkpoint
read is unchanged. Owning tests distinguish that existing health call from a
forbidden fallback after a failed cheap deadline read. No service grant, provider,
product profile, native kernel, published artifact, or physical device changed.

## Qualification

Run the maintained owning suite:

```
python scripts/test_runtime_clock.py --runtime <Runtime> \
  --system-apps <System> --canonical-header <frozen-sdk>/app/RiscRuntimeClockV1.h
```

Qualified against Runtime `273bdbccf65647a803ab0586a418f8283732baec` SDK and System
`f53d6456f270a15248df23243030fc0e4b21cf93` source. Six owning configurations pass:
Canvas, Contexts, Contexts with native toolbar, each normal and ASan/UBSan. Eight
helper configurations pass: C/C++, current and derived pre-clock SDK declaration,
each normal and ASan/UBSan. The legacy declaration removes the suffix; it is not a
claim of execution against an untested historical Runtime binary. An exactly
allocated legacy table prefix is also exercised under sanitizers. Cases include
null/version checks, zero/wrap, absent/null callback, clock failure without retry,
health failure, unchanged output on failure, Canvas retained fence, Contexts
failure/start/timeout behavior, exact source hash/bytes and staging conflicts.
Leak detection is disabled for the sandbox; address and undefined-behavior checks
remain enabled. `tests/test_contexts_contract.py` also passes all four cases.

Host/source qualification does not establish a physical performance or wake pass.

An additional 20 existing Canvas real-entrypoint cases passed, normal and
ASan/UBSan, using `test/canvas/app_test.c` with production `canvas.c`: Model Viewer
watch/x4/x4-native normal/retain, x4-native SD/current and legacy, and Hollow Trail
x4 normal/retain. Frame files were not retained for this clock-only regression;
no new visual qualification is claimed.
