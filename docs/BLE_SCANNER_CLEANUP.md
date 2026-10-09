# BLE Scanner cleanup recovery

The scanner previously turned a single refused controller close into an
unrecoverable parked invocation. The sensor provider retained a valid token, but
the app set `uncertain` and never called close again. This was reproduced through
the actual app, shared adapter and generic sensor provider: one transport refusal
caused one close attempt followed by twelve scheduler waits.

Scanner 0.2.16 keeps the existing token and capability grant and retries only
the refused close, waiting 50 ms between attempts. It separately retries a
refused provider-grant release. No scan, status, rendering, input, preference or
background operation occurs during either retry loop. Normal work resumes only
after cleanup succeeds. A permanent refusal retains the invocation and all
remaining ownership; it never reports a clean exit. Cleanup does not restart a
scan, change radio preferences or discard results or saved sensor names.

Run the focused regression with:

```sh
python3 scripts/test_ble_cleanup_retry.py --system-apps /source/System \
  --drivers /source/Drivers --presentation-sdk /source/PaperSystem/Apps --output /temporary/ble-cleanup
```

The recorded System source is df1d7d1b248de13f773690139750ad2f963967f3 and
Drivers source is aa0b9cc08b9a3d8a7a8a9410d1507467cbd980f9. To reproduce the old
failure, extract `Apps/ble_scanner.c` from the recorded baseline and pass it as
`--app-source`, with `--scene stop --normal-only`.

The 112 executions cover Stop, starting a second scan, Back, sensor naming, app exit, radio controls and
idle sleep, for controller-close and provider-grant-release failure, with zero,
one, three or permanent refusals, in ordinary and ASan/UBSan builds. All unrelated
peripheral operations are guarded during retained cleanup. The permanent test
escapes after twelve waits without calling app fini; production has no such escape.
The baseline failure and final case logs are in `evidence/ble-cleanup-retry`.

These tests execute production application, adapter and sensor-provider C code.
HCI, Runtime, display, touch, alarm and physical sleep endpoints are doubles.
The injected provider-grant-release refusals come from the Runtime test double;
they do not demonstrate recovery from a production Runtime/Graph quiesce refusal.
They do not qualify physical controller timing, hardware sleep or sensor
interoperability. No frozen product image or live feed is changed.

The canonical source retains the existing paper renderer and native/resident paths.
The standalone host regression selects its 240-pixel branch; the X4 resident
checkpoint is separately qualified before product integration. The presentation
SDK supplies the existing PaperPresentation/PaperFrame contracts only. The selected
Watch legacy-source target is recorded independently; this does not claim whole-tree
equivalence between the Watch and native/resident source histories.

## Continuous integration

The BLE Scanner workflow runs all 112 cleanup cases using the existing public
System pin `a7f08a9db7342a69ef5b9bc03e3b1ea60dafbb7c` and Drivers pin
`aa5ce0140102bf4d1039e5f208588a6793d06b28`. The same System checkout's `Apps`
directory supplies the presentation SDK. This reuses the established scanner
CI inputs and changes no provider or presentation versions. Case logs, compile
commands, and `results.json` are included in the workflow artifact.

The inventory check names Scanner 0.2.16, matching both current manifests.
The native paper preservation regression still checks the exact bytes of
`Apps/native-utc-utilities.json` and `Apps/native-utc-alarms.json` from historical
commit `45cffd33f6d258017cf784ed6b4534654c221736`, with SHA-256 values
`87ac5ab88ff2a8bf60f94ff6331b16f843247310ea28cc12db1dc17350d2f3fc` and
`ab6d0716a9a49d4891c13c4d0ec80c9a3d4bad372d04784a2e7bbf37ffc0815b`,
respectively. These fixed byte identities let that test run from a shallow
checkout without relying on locally retained Git history.

The Native UTC workflow's System checkout is aligned with the existing source
lock at `1d589d90bf27c7ffb76420de46564088ddb3714f`, also used by the Alarm ABI
workflow. The exact revision, clean-tree, and source-digest checks remain enabled.
