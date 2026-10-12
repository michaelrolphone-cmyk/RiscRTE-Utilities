# X4 RF startup custody repair

RF's event collection retained one `storage.app-data@1` grant for its entire
invocation. Opening Contexts during `cfa_restore()` then required
`storage.shared-data@1` for the same namespace. Native Runtime 0.2.28 permits
only one app/shared-data context per invocation. Its clean refusal reached the
production custody adapter's fail-closed barrier, freezing before RF acquisition.

The production RF startup helpers, Contexts/custody adapter fragments and actual
RF-only Contexts service reproduce the failure with a modeled Runtime exclusive
grant. Before the change, normal and ASan/UBSan runs retained once at the shared
read. Opening Contexts first avoided this one startup conflict but would not
protect later model checkpoints, so startup reordering alone was rejected.

`rf_app_data_scope.h` keeps a stable app-owned API table, acquiring and releasing
the native grant for each stat/read/replace. The existing broadcast/background
fence runs first. Opaque expected revisions, names, buffers, outcomes and namespace
3 remain unchanged. App-data ABI tokens survive release/reacquire. A refused
release, backend RETAINED result, or dirty failed acquisition keeps the grant and
prevents all later I/O. Startup stops before further services or timing reads when
event/fingerprint restoration reports retention. No schema/data migration or
automatic retry is introduced.

Startup diagnostics name preferences, signatures, events, fingerprints, DSP and
receiver stages; they contain timings only, not saved labels, models or RF samples.
They do not yield or reorder startup services. The host renderer fixture now stops
issuing scripted user events after accepted launch, including final frame drain.

## Checks

- `test_rf_startup.py`: exact production target flags and real shared adapter;
  normal, reentry, unread settings, missing storage, airplane mode and no logger,
  each normal and ASan/UBSan. Optional Contexts is absent in this fixture.
- `test_rf_startup_contexts.py`: real Contexts RF service and production custody
  fragments with modeled Runtime exclusivity; startup and later RF/Contexts
  checkpoints, initially unopened and preopened, normal and ASan/UBSan.
- `test_rf_app_data_scope.py`: exact bytes/revisions, clean unavailable/malformed
  interfaces, stale and uncertain writes, failed release/acquisition and terminal
  no-I/O behavior, normal and ASan/UBSan.

Pinned integration: Utilities baseline 5c91c3517aa9f992e24fc453855701ec5a992355,
System c2889adff2a06bcd6c6207f11fbd0795fbbd99fa, Runtime
1c1ed8661ea30d47e7e9707c41d492ce3a2b4d70. Native firmware changes are unnecessary.
These tests model Runtime/peripheral calls and do not prove physical RF capture or
hardware behavior. The old broad RF scene suite has pre-existing sample-count/UI
expectation drift and is not claimed green.
