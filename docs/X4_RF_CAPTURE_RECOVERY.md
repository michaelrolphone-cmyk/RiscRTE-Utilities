# X4 resident RF capture recovery

Waterfall's selected X4 resident profile advances from 0.2.17 to 0.2.18.
The change is confined to `PORTABLE_RADIO_CONTINUOUS_CAPTURE`. The non-continuous
profile retains its existing behavior and produces byte-identical target ELF
bytes when compiled before/after with otherwise identical flags.

## Evidence and scope

The X4 0.1.52 installed Waterfall is the metadata-compacted 0.2.17 app built
from Utilities `bda1c2ec01d2c18e39f822fbcf6cc8ac3b12aa9e` and System
`09922405ee21639f845359386abaae4a258d2a95`. Installed native Runtime is 0.1.100,
source `615fb236b591bc6974a35ae23c7b2b785c0a5016`; the source receipt retains its original public SDK input at
Runtime `83cc1f8f661fb39c5028ce669887629de9423bf7`. No native revision change is
needed or made. Installed IQ provider 0.2.0 is from Drivers
`4088b6892c2e2654b0342f04a7d191068e4a8e2e`.

The existing active-capture idle inhibitor is present in the installed profile.
It is not missing. A production-source host probe establishes the app's
permanent-stop response: three successful coherent bursts, then an injected
clean DUMP_TIMEOUT, clear `running` and `capture_requested`, dropping that
inhibitor and requiring a new Start. The probe fixes transport initialization
and initializes the real signature analyzer so its first three bursts genuinely
reach DSP/history, rather than failing earlier in a test fixture.

This does **not** establish which event caused the reported physical stop.
No device trace or physical reproduction identifies an initiating timeout,
busy lease, PLL/PBUS failure, native stall, or another cause. In particular,
a blocked synchronous provider call cannot be recovered by code that runs only
after that call returns. Initial device fault and physical validation remain
open; this is a bounded software recovery improvement, not a proven fix for
all possible causes of the observed symptom.

## Behavior

- Clean PLL_FAILED, PBUS_FAILED, DUMP_TIMEOUT and BUSY keep the explicit capture
  request live and schedule at most three further attempts.
- Delays are 250, 500 and 1,000 ms, measured after the previous call completes.
  Unsigned elapsed time handles millis wrap and a zero timestamp.
- The wait does not block or yield inside capture. The existing app input loop
  stays live. No provider, policy read, sample processing or diagnostic occurs
  on skipped retry polls. Policy is rechecked before each actual attempt.
- Successful coherent data resets the consecutive retry budget. Failed attempts
  do not add a frame or present stale samples as a new capture. Existing gap,
  background and event-discontinuity handling runs on the error.
- Four consecutive errors (initial error plus three failed retries) stop with
  explicit Start required. Bad arguments, NOT_RUNNING, unknown results,
  invalid format/data and denied/unreadable policy remain fail-closed.
- Stop, Freeze and typed suspension cancel pending retries. Reentry through
  explicit Start resets the retry state; idle/wake restoration does not restart
  foreground capture.
- CLEANUP_RETAINED keeps the existing suspend-only drain before normal I/O.
  It is never treated as a clean retry, and Start remains required after cleanup.

The existing transition-only provider result/detail logs remain. Small retry,
recovered and stopped messages add retry ordinal, delay, valid-burst count and
failure count. There is no per-successful-burst log, new allocation, background
task, device capability, native API, sample dump or calibration claim. Counts
saturate rather than wrapping. Native Runtime, System/Home, IQ provider,
Bluetooth, GameBoy, boot/wake code and product images are unchanged.

## Reproduction and checks

Run against the original build receipt and exact System checkout:

```
python scripts/test_rf_capture_recovery.py \
  --build-receipt ORIGINAL_BUILD/waterfall/x4-native-app.json \
  --system-apps EXACT_SYSTEM \
  --baseline-source EXACT_BASELINE_UTILITIES \
  --output /tmp/rf-capture-recovery
```

The optional baseline argument verifies its revision and source digest against
the receipt, then requires the precise original permanent-stop assertion.
Candidate tests use the same production flags and hash-verified staged headers.
Normal and ASan/UBSan runs cover all retryable codes, delay boundary/completion,
retry exhaustion, stop/freeze/suspension, policy revocation, fatal/unknown errors,
malformed data, retained cleanup, explicit restart, timer wrap, repeated failure
episodes, quiet steady-state logging and diagnostic refusal. Existing idle
admission tests run in both modes too.

The resident integration fixture runs the real RF `app_main`, renderer, input
polling and shared adapter. Runtime dispatch and physical transports are explicit
host fixtures; a separate System suite can exercise the real host/Runtime.
Do not describe any of these checks as device execution or hardware qualification.

Run the resident app/adapter scenarios with:

```
python scripts/test_rf_resident_recovery.py \
  --build-receipt ORIGINAL_BUILD/waterfall/x4-native-app.json \
  --system-apps EXACT_SYSTEM \
  --output /tmp/rf-resident-recovery
```

They cover a 61-second idle jump, local suppression of Runtime policy dispatch
through a retry wait, continued real input polling, recovery, explicit Stop,
Freeze and Back during retry, shared policy resumption after Stop, explicit
Start, clean exit and repeated module invocation. Both normal and ASan/UBSan
variants run; the non-continuous controller/adapter suite remains separate.
