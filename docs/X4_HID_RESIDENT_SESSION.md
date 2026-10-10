# X4 resident HID session lifetime

The selected resident Touchpad and Buttons profile advances from 0.1.17 to
0.1.18. Legacy Watch/paper profiles are unchanged unless they explicitly select
`PORTABLE_RADIO_CONTINUOUS_CAPTURE`.

## Reproduced cause

The X4 0.1.52 store preserves the resident applications built from Utilities
`bda1c2ec01d2c18e39f822fbcf6cc8ac3b12aa9e` and System
`09922405ee21639f845359386abaae4a258d2a95`. Their original target ELFs can be
rebuilt byte-for-byte. Applying their recorded metadata-compaction recipe yields
the exact installed Touchpad, Buttons and Waterfall ELFs.

The resident Home policy asks for a settled POLICY checkpoint when battery
sampling is due (five seconds after its preceding sample), even while
the idle deadline is not due. The old HID client had no active-radio policy
gate. Its modal POLICY checkpoint called `portable_radio_suspend` before
handing control to Home. That closed the live HID token and displayed
"Paused - choose Reconnect". A pairing-screen redraw can use enough of that
five-second interval to make suspension appear immediate.

The regression compiles the actual HID controller and System adapter with the
installed deployment flags. Its controlled Runtime dispatch reproduces the
five-second host request. The original profile tries to close the pending
comparison at approximately 5,002 ms before any accept/reject action, failing
the assertion that pending comparison must remain live. Peripheral I/O and
Runtime dispatch are host fixtures; target instructions are not executed.
The separate System suite runs the actual 0.1.52 host policy through production
Runtime 0.1.100, ProviderGraph and `dlopen`.

## Focused change

An open HID token, or its unresolved checked close, opts into the existing
local radio-activity inhibitor. This holds off automatic resident battery/idle
policy while advertising, comparing numbers, reconnecting or using the secure
link. The ordinary HID provider poll and input pump continue. App battery
updates continue on their existing schedule. There is no new Runtime API,
background task, timer, allocation or per-poll diagnostic.

Explicit Stop, Back, controls, alarm and sleep still use the existing suspension
boundary. A provider pairing timeout, rejection, transport error, failed open
or completed close clears the gate as soon as cleanup is verified. Unresolved
close/unsubscribe/release preserves custody and remains gated. No pairing
number, key, address or report content is added to diagnostics.

Encryption/authentication requirements, fresh-touch consent, held-report
release, numeric-comparison timeout, bond storage, reconnect and forget logic
are unchanged. Native Runtime, Home, providers, GameBoy and product image are
unchanged. Periodic shared battery/idle policy resumes after the explicitly
opened HID session ends.

## Verification

`python scripts/test_hid_resident_policy.py --baseline-build BASELINE --system-apps SYSTEM --output /tmp/hid-policy --candidate`

Add `--sanitize` for ASan/UBSan. Omit `--candidate` to reproduce the old deployed
build profile's premature close assertion. The baseline receipt supplies the
production build definitions and copied SDK; candidate adds only the activity
opt-in. The actual-controller gate test also reruns the existing complete HID
unit lifecycle/security checks.

Covered for both Touchpad and Buttons:

- Fresh accept and reject after a 61-second idle deadline with comparison live
- Provider timeout followed by explicit reconnect
- Transport error followed by explicit reconnect
- Explicit Back/cancel while comparison is pending
- Rejection with failed close, unsubscribe and grant-release retries
- Advertising, connected, comparison and ready states, including long silence
- Fully resolved cleanup releases inhibition; unresolved cleanup preserves it

Existing HID model/controller tests and full paper renderer scenarios cover
fresh-consent carryover rejection, movement/multitouch/event gaps, disconnect,
reconnect, forget, mouse drag/scroll, button reports, edits, alarm, controls,
sleep and return. These unchanged legacy profiles remain separate from the
resident policy test.

All execution is host-side or cross-compilation. Physical Bluetooth/controller
interoperability and device timing still require hardware qualification. No
radio, device, flash, publication or endpoint operation is part of this change.
