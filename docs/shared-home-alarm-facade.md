# Shared Home alarm facade
The Home ELF uses `alarm.service.facade@2`. Product metadata installs one selected facade variant:
- `legacy-clock-change`: depends on unchanged `alarm.service@1` with the validated ClockChangeV1 tag and sleep callback. This preserves the existing Watch/Waveshare scheduler, data namespaces, UTC+8/Denver policy, output controls, startup/reconcile retries, and direct time-editor clock-change contract.
- `tagged-owner`: depends on unchanged `alarm.service@2`; copies its validated output modes and optional resume feature, and preserves a validated Points projection suffix when present.

The facade owns no scheduler, RTC, storage, output session, worker, polling or retained ticket. It borrows callback/context pointers from its sole pinned dependency. Existing Alarms/Countdown/time-editor consumers keep their original capability/API and owner. Tickets and copied status are not translated. A facade release is not a claim that the scheduler's outputs are quiescent: Runtime separately retains/quiesces the underlying provider.

Sources are independent of device class and display geometry. Profile selection is provider contract selection, not an application branch. The legacy variant deliberately rejects an untagged or malformed table instead of guessing a suffix from its size.

`scripts/test_alarm_facade.py` tests production forwarding and the unchanged real legacy scheduler normally and under ASan/UBSan. `scripts/build_alarm_facade.py` produces both native SDK target profiles with exact receipts and real Runtime loader validation. Hosted workflow also replays bytes. Actual Runtime provider-graph coexistence is qualified separately by the owning Runtime test-only PR110; do not count its queued result as passed. Physical-device testing is not an added acceptance gate.
