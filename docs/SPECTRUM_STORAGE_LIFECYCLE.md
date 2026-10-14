# Audio Spectrum storage lifecycle

## Verified source path

The reported baseline is Utilities `7c060d9b83695cc110958f20fac76fc1bb32389f`
(`work/shared-watch-spectrum-20261013`) with Runtime `4e340c7`. The branch's
shared-input workflow pins System Apps `fd4b1e52fe2a49c3c4c22f9ccd4c98a91f88afa0`.

`Apps/audio_spectrum.c:app_main` restores temporal events before fingerprints.
`Apps/spectrum_temporal_app.inc:event_restore` previously kept the native
`storage.app-data@2` grant in `event_files.api` for the entire app invocation.
`Apps/context_fingerprint_app.inc:cfa_restore` calls
`portable_contexts_models_save`, whose Contexts client loads/checkpoints shared
fingerprints and rules. Runtime's `src/bootstrap/Runtime.cpp` admits only one
app-data/shared-data context per invocation. The second acquisition therefore
fails. The checked `native_custody_adapter.inc` has no clean-refusal exception
for these storage capabilities, so it retains the invocation.

## Repair

Audio Spectrum reuses `Apps/rf_app_data_scope.h`, already present in the same
baseline. `event_files.api` points to a stable app-owned proxy, not a retained
native Runtime table. The initial admission check remains eager, but its grant
is immediately released. This preserves the previous unavailable/invalid-store
behavior instead of falsely presenting an unavailable store as readable.

Each later `stat`, `read`, or `replace` acquires and releases its own native
lease. Consequently temporal events, neural checkpoints, local fingerprints,
and shared Contexts checkpoints can alternate without overlapping native
app-data/shared-data leases. Runtime grant exclusivity and the checked adapter's
retention policy are unchanged.

A retained operation or refused release preserves the exact grant, marks the
scope terminal, and prevents subsequent native calls. `temporal_retained`
checks both the temporal file state and scope state. Startup checks this fence
immediately after `event_restore`, before calling fingerprint restoration.
Ordinary exit no longer releases an app-lifetime event-data grant.

Namespace 2, filenames, codecs, saved examples, model layouts, and revision
checks are unchanged. The AppData ABI explicitly preserves revision identity
across release/reacquire; intervening writes can still invalidate revisions,
and the existing stale/reconciliation logic remains authoritative.

## Regression evidence

The successful repair run is
https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/38491307941.
It reproduced the conflict before patching, passed the new lifecycle tests,
passed all 26 existing Spectrum fixtures in both ordinary and ASan/UBSan
configurations, then committed the verified source as
`5d8b116c43887931d9c2d3db1eeb31f20ef806b5`.

With System Apps checked out at `fd4b1e52fe2a49c3c4c22f9ccd4c98a91f88afa0`:

```sh
python scripts/test_spectrum_storage_lifecycle.py --system-apps /path/to/system
```

The new fixture includes the actual Audio Spectrum app and runs it against an
exclusive-slot Runtime model and a modeled shared-checkpoint hook. It covers
startup, later reads/writes around shared checkpoints, stale revisions,
no-space errors, retained stat/read/replace, refused release, dirty acquisition,
and the startup terminal fence. Both ordinary and ASan/UBSan builds run.
The `--expect-conflict` option reproduces the old startup failure when the same
fixture and runner are placed in the original Utilities source checkout.

The existing complete Spectrum regression suite uses the legacy SDK pin from
the baseline workflow, System Apps `fdc15395928a1fa8eee62a2823b3c8777dcf0fa0`:

```sh
python scripts/test_audio_spectrum.py --system-apps /path/to/legacy-system
```

Its temporal and neural fixtures count native acquisitions/releases rather
than assuming a single lease for the entire invocation. Their existing
storage-content, retry/discard, capture, and retained-cleanup assertions remain.

These are host tests, not physical-device qualification or a full firmware
Runtime/adapter integration test. A rebuilt Audio Spectrum artifact must still
be checked on the device for launch, capture, save/reload, and normal exit.
