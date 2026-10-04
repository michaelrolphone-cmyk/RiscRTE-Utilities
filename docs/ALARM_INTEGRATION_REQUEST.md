# Next scoped integration requests

These are source-backed requests for coordination, not changes made by this PR.
Do not alter the accepted Watch 0.5.2 / 1.0.0 release inputs to fold them in.

## Runtime app-policy capacity

Runtime 0.1.7 has two matching eight-policy bounds:
`src/bootstrap/Runtime.h` AppPolicy policies_[8], and
`src/bootstrap/Runtime.cpp` appPolicies rejects value.size()>8.
`docs/APP_CAPABILITIES.md` documents the same bound. Watch currently deploys
seven distinct manifests including default and Clock. Alarms + Countdown require
nine, and the selected existing Timecard would require ten.

The smallest generic candidate is one named compile-time maximum of 16 policies,
used by both allocation and validation, with unchanged eight requirements per
app and sixteen live grants. This matches the existing shared catalog capacity.
Do not add implicit capabilities, infer grants, remove accepted apps or collapse
the default/Clock lifecycle to squeeze under eight. Confirm the additional fixed
Runtime RAM cost in the real target build. Tests must admit 9/10/16 explicit
policies, reject 17 before entry points, and preserve duplicate identity/path,
undeclared-grant, wrong-version, namespace, handoff and retained-app enforcement.
Use a separately coordinated fresh Runtime version; 0.1.8 is reserved elsewhere.

## Shared client settled/error barrier

The existing shared adapter's present is synchronous but can return failed
while its native presentation is still ACTIVE; a later false poll is not a
settled-frame guarantee. The smallest useful adapter contract is an explicit
internal state at its central poll/present boundary: safe normal service step
only after completed display work, no drawing against a queued frame, no stale
input replay, and a failure route that invokes stop_only instead of normal step.
No provider poll or callback into the foreground app is needed.

The same central location must intercept alert ownership and Back before
PORTABLE_RETURN_APP queues request_launch. Keep the foreground module and RAM
loaded, cancel prior gestures, copy the bounded service descriptor, and restore
/redraw the original app only after safe durable dismissal. Preserve Calculator
operands/repeat state and Stopwatch's live monotonic path and error/pending state.
Fresh app versions and exact shared-client pins are required for rebuilt ELFs.

On a broken presentation, stop_only has its own small cleanup bound: at most
three calls, one independent output operation each, no persistence/RTC/new output.
Safe stop leaves a pending exact token and blocked service for the next healthy
foreground. Unsafe stop retains the current invocation/grants and reports a
one-way diagnostic without automatically repeating potentially unsafe I/O. The
output backend must prove owned-buffer cleanup is safe while an unrelated
display presentation is pending. Host fakes do not establish that guarantee.

## Output and sleep consumers

Native I2S TX and hardened haptic/speaker lifecycle remain separate prerequisites.
Clock must consume prepare_sleep with immediate serialized owned timed entry;
refusal/Light wake re-evaluates RTC, and Deep checks due service work before intro.
Settings writes only alert_mode in its existing namespace 1 grant. Watch then
selects the ordinary singleton and exact five-key storage map, and grants only
alarm.service to foreground consumers. No namespace-4 app grant, firmware alarm
scheduler, Watch-local app copy or new Runtime device/date policy is required.
