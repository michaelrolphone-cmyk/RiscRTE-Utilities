# Files background integration

Include `Apps/files_background.h` in the semantic Files root translation unit.
The public callback and copied-state boundary is `files_background_api.h`.
The helper uses published Context/CFP/rule/preference encodings and original
namespace identities. Providers own audio, RF, BLE and lifecycle; no device,
backend or resolution selection occurs here.

1. Initialize with the native Runtime API, a pure invocation identity guard,
   retain callback, and checked battery/local-hour observation callback. The
   observation callback copies a current root-owned snapshot; prepare any
   timezone/storage reads outside `step` using the storage boundary first.
   Do not reenter the helper from the callback or open app-owned storage while
   capture is live. The observation callback is required when Contexts is present. Local hour must
   preserve the user's timezone; do not pass UTC hour. Invalid/unavailable time
   is represented with `local_time_valid=false`, not a fabricated default.
   Fingerprint UTC aging is read independently through the tagged RTC UTC suffix,
   when present. With an old RTC calendar prefix, the helper copies and releases
   RTC, then asks the optional `time.civil` policy suffix to interpret that raw
   calendar using the provider's configured RTC basis. Native realtime is the
   last fallback only when these authorities are cleanly absent. Recognized
   malformed suffixes, retained results, dirty failure output from contracts that
   require unchanged output, and clean invalid
   time from an advertised authority never select another clock. No clock,
   preference or RTC write is performed; no timezone/device basis is inferred.
2. Supply the confirmed preset action mask and alarm output modes. These affect
   applicability only; unsupported saved fields are not rewritten. Optional
   Context absence is clean: telemetry continues and no Context actions run.
3. At settled regular boundaries call `files_background_step`. Supply audio/RF
   permission from capabilities and current owners, not product identity.
   Both are allowed for Files; existing radio/BLE/airplane/low-battery policy
   still limits RF. BLE progression precedes Context progression. A held input
   defers the root's idle/handoff decision after this progression.
4. During PRESENTING/text loops call synchronous `files_background_capture`
   at the original <=16 ms checkpoints. Regular step with `settled=false`
   performs no I/O and does not reset policy/capture. Capture does no storage,
   policy evaluation, workflow, UI or RF work.
5. Before storage, use `files_background_before_storage(write)`. Read-only
   boundaries preserve loaded policy. Before sharing/network ownership, finish
   `pause`; while sharing, pass `sharing=true` and do not call unrelated helper
   entry points. Capture and step then make no provider calls. Do not hand off
   until pause succeeds. A false pause with copied `cleanup_pending` and no
   retention is retryable cleanup; retained custody forbids all further I/O.
6. Message/launch callbacks copy the 33-byte payload synchronously. A successful
   message callback means the root queued a modal, saved its draft and will
   consume dismissal. A successful launch means an installed, permitted route
   was validated and queued; launch must not execute inside this callback.
   Keep `workflow_ready=false` while either handoff is outstanding.
7. Drain copied `pending_effects` through the negotiated resident contract.
   Pause background and settle/release the root scene/focus first. QUERY must
   succeed for each effect before APPLY. Old-host BUSY is an integration
   dependency, never permission to substitute POLL/POLICY/CONTROLS or to claim
   full behavior. Physical settings reconciliation requires a host implementing
   the contract wherever Context actions are available.
8. After APPLY OK, query the copied APPLIED/FAILED/PENDING outcome. A pending
   alarm uses PROGRESS(ALARM), never a second APPLY. On terminal outcomes call
   `files_background_effect_done`; clean physical failure never retries a
   committed preference, workflow claim or timer. Pending keeps the bit set.
   The helper postpones further automatic actions while effects remain pending.
9. Close before root handoff/unload. A successful close releases Context and BLE
   grants. Do not destroy pending or retained state.

The helper prefers the explicit native atomic automatic writer when advertised.
Only clean capability absence selects the original legacy namespace writer.
Accepted atomic tokens are collected before release; uncertain receipts do not
replay. Workflow/settings results are reported before physical-effect bits are
exposed. Existing countdowns are never overwritten.

Qualification uses endpoint doubles, not physical hardware. The test runner
records exact commands and compiled-input SHA-256 values. Root composition,
actual local-time provider availability, modal draft/dismiss behavior and selected
product deployment remain integration responsibilities. The exact original X4
Files authority has no Context/CFP/rules/RTC grants; no new grants are implied.

Legacy RTC calendar-prefix reads may fill fields before reporting invalid time.
Those failed-read fields are ignored; the grant is released and an available
native realtime fallback may be consulted. The tagged UTC suffix and civil
interpretation contracts still require unchanged failure output.
