# Provider-neutral application lifecycle qualification

Task f13f9b6e-b651-4244-ba6a-5637b00a9339 (8 points). This extends existing
Alarms and canvas consumers; it does not introduce an application-owned physical
lifecycle, a second Runtime, or a new production ABI.

## Corrected shared behavior

A transient `RISC_REALTIME_IO` during the native scheduler's `READ_RTC` phase
previously made the scheduler permanently blocked until explicit refresh, even
when the backend recovered. The actual headless consumer reproduced one failed
read followed by 80 owner ticks with no second read. Broad `refresh()` retry was
rejected: it would reset the time anchor and could reinterpret invalid or
backward time as a new valid basis.

The corrected shared service returns `ALARM_PENDING` for that typed IO outcome
only while the read is free of active occurrence, pending persistence, uncertain
haptic/audio output, and clock-editor recovery/permit custody. A later ordinary
cooperative step rereads; no anchor, durable record or output is changed by the
failed read. Invalid/unset/backward time still fails, and authority/context loss
still retains custody and forbids further calls. The existing single-owner,
non-reentrant service contract is unchanged. This is not a hard retry deadline.
A pending read invalidates an old sleep ticket and cannot create a fresh ready
sleep ticket. Runtime normalization of malformed backend data to IO is not
acceptance of that data: no clock value is consumed until a valid read succeeds.

No recipe-specific opt-in hides this repair. Native-points output can change;
`build_native_schedules_alarm.py` preserves the exact historical source and
before ELF, records corrected source/flags/commands/output/size separately, and
continues to enforce byte-identical Watch output. The fixed historical BASE is
not refreshed. The migration report additionally requires the exact reviewed service
source hash; future changes require renewed source qualification rather than inheriting
a blanket native parity waiver. The build receipt does not label changed native bytes equivalent.
Package manifests are retained as selected recipe identities, not a new release
or installed replacement; source commit and ELF digest identify the candidate.

## Actual boundaries exercised

| Boundary | Evidence |
| --- | --- |
| Headless launch, action, return | Existing actual Runtime/ProviderGraph owner/control/scheduler baseline, unchanged app body |
| Recoverable provider read loss | Actual service IO then recovery; same mapped owner/control/service composition |
| Cancellation before readiness | Unavailable clock, existing owner exit request, no alarm configuration mutation, clean release |
| Terminal authority loss | Actual context error; no later backend/owner calls, reentry refusal and retained app/control/service images |
| Canvas resume and clean return | Existing actual canvas, profile and display graph; three presentations, quiesce/resume, clean release |
| Canvas cancellation | Unsubmitted frame cancellation and exit during submitted work; drain/forget/quiesce/close before return |
| Failed provider stop | Demand activation: last grant release while pending is refused; app/provider/dependencies remain mapped |
| Failed output settlement | Eager activation: failed physical-status double retains the invocation and dependencies; no calls after fence |

Canvas cases use the same compiled modules. Only case 3 selects demand activation;
the other four use eager activation. Each activation/boot hash is recorded. A
last-grant release is not falsely claimed to stop an eager resident provider.
Physical clock, storage and display operations are deterministic host doubles;
Runtime, ProviderGraph, ELF loading, production Alarms/canvas service bodies and
consumer ownership paths are real. No native device execution is inferred.

The shared image observer binds actual temporary loaded bytes to declared module
identity before unlink, forwards real `dlopen`/`dlclose`, and records actual export
address/handle. Clean cases require one successful close per observed image and
loader metadata no longer mapping its saved numeric address to that old image
path. Retained cases require no close attempt and a still-mapped exact address.
It never dereferences released code or introduces an `RTLD_NOLOAD` reference.
Intentional retained process-lifetime custody is not described as leak-free;
ASan leak detection is disabled for those fixture paths.

The native service tests first run the original accepted native UTC suite, then
10 ordinary / 12 mixed-editor controls. These cover repeated IO/no writes,
anchor preservation and backward-time rejection, active occurrence, pending
persistence, uncertain outputs, non-read phase, invalid/context results, editor
recovery/permit, and sleep ticket invalidation. Existing raw/UTC/editor control
refusals and actual mixed-service control tests remain in the focused workflow.

## Source and compiler provenance

Host Runtime: `13eca114cb58469cf6004fafc3da84fbd9d29beb`.
Host System/canvas: `9d2908bf50d3708763d36cbd810b137e2b5a4c57`.
Utilities source parent: `ebd8e18d53ea9936ca35380e0069c404ae08cdd3` on
`badge/points-clock-edit-2026-10-28`, not Utilities main. The retained local
workspace is sparse; no whole-checkout equivalence is claimed.

Each newly added actual-consumer runner records every compiled translation unit
using `-M` and the actual compile prefix, including PIC/visibility flags.
Repository paths are classified before compiler standard roots, pragma-marked
system headers remain pinned, unknown external paths and environment include
injection are refused. Generated SDK bytes must have an exact source match in
the declared repositories. Standard headers and compiler identity are recorded
separately. Seven scanner regressions exercise these boundaries. Earlier broad
file snapshots are not complete compiled dependency closure and are superseded
for that claim, not silently equated to these receipts.

Target scheduler qualification deliberately retains its original target SDK
composition: Runtime `273bdbccf65647a803ab0586a418f8283732baec`, System
`6e4cda9072905c15b9a5ebc9b22913fec444a89d`, pinned GCC15.2 installer. It is not
labeled the current host SDK. The existing target loader/admission, import/export
checks, historical witness, licenses and Watch parity remain. A target build is
not a device run or physical alarm acceptance.

## Replay

From a Utilities checkout with the exact host Runtime/System inputs:

```
python scripts/test_lifecycle_dependencies.py
python scripts/test_native_clock_retry.py --runtime ../Runtime --system-apps ../System --output /tmp/retry
python scripts/test_headless_current_runtime.py --runtime ../Runtime --system-apps ../System --headless-only --output /tmp/headless
python scripts/test_provider_lifecycle_canvas.py --runtime ../Runtime --system ../System --canvas ../System --output /tmp/canvas
```

Repeat in fresh output directories with `--sanitize` (UBSan for the native
service controls; ASan+UBSan for actual Runtime consumers). The focused workflow
also runs the existing control-policy suite in both modes and builds the actual
target scheduler. Retain failure logs and distinguish target build, host
execution and unobserved device behavior in acceptance reporting.
