# Shared semantic BLE Scanner

`Apps/ble_scanner_scene.c` is the canonical shared-scene implementation, selected
as `ble_scanner` version 0.3.2 in `utilities-manifest.json`. The original legacy
application remains intact for existing recipes. Build with
`scripts/build_shared_scene_apps.py --runtime RUNTIME --system-apps SYSTEM --app ble_scanner --output OUT`.
The one executable is reused byte-for-byte across product selections; product
manifests and granted authorities are independently projected from their original
published rows, never chosen by executable device branches.

## Preserved behavior and data

Explicit 15-second scans retain up to 32 devices, named-first filtering, complete
copied readings, service/raw-data details, RSSI and age. Details identify random
addresses and unverified data. Naming uses the original namespace-1 BSv1 alias
wire format and 24-character normalization; retry keeps the original expected
bytes and desired intent. Cancellation drains pending storage before changing
identity. Bluetooth enable is explicit, revision-fenced, physically verified,
and compensated only while the original policy intent remains current.

Context audio capture continues at bounded loop checkpoints when enabled;
foreground scan owns Bluetooth RF, while background RF and broadcasting are
paused. Existing fingerprint formats, shared namespaces and provider UTC basis
are retained where originally authorized. Clean capability absence is disclosed;
malformed or uncertain ownership fails closed. UI geometry, input, rendering,
text entry and physical lifecycle remain provider-owned.

Scan close may remain pending and is continued without advancing unrelated
owners. Resident attention is optional. Where absent, the existing admitted
alarm service advances cooperatively, snapshots reservation before each phase,
and stops scan/background/text owners before the next output phase. Dismiss
uses the exact copied occurrence token. Quiet editing still advances the alarm
scheduler at settled text checkpoints. No automatic scan restart follows a
modal or handoff. Manual copied attention preserves neutral-short Back and
ignores long/combined events. Automatic alarm handling uses the negotiated alarm-only
checkpoint, never ordinary POLL/POLICY. Configured idle uses the key-independent
idle contract after stopping owners. Ordinary idle preserves editor drafts; an
actual alarm/cue/error outcome cancels editing through the copied outcome query.
All host checkpoints require settled custody.

## Product metadata

The Runtime permits only one declaration of each non-KV capability. Canonical
metadata declares alarm.service API2; the checked projector selects the original
product grant API (Watch/Wave1, X4 2) without changing the executable or provider.
Original alarm grants and providers remain. Scene/profile bytes remain unchanged.
Watch/Wave select the repeat-qualified key-independent Home 0.25.1/0.25.2;
only Home ELF and manifest version change, with original authority retained.
X4 Home bytes remain unchanged. Watch's equivalent namespace-1 preferences facade is added only with
native backend qualification; Wave/X4 retain their original atomic authority.
Watch/Wave select the published RTC UTC suffix with explicit original +28800
civil-time basis; X4 gains no RTC or Context authority. SD routes and unrelated
payloads are preserved, with only a content-addressed Scanner route updated
where the original product requires it.

## Qualification and limits

Focused runners cover session, presenter, preferences, background, root lifecycle
and alarm behavior in normal and ASan/UBSan modes. Actual Runtime tests use real
application, scene and text components with deterministic sensor, alarm and
native backend endpoints. They exercise scan/details/text cancel/rescan/stop,
pending close, two unloads, no-UI zero-I/O and reservation/exact-dismiss paths.
Both current scene sources and the exact retained 0.4.8 provider sources are
qualified against current SDK headers. These tests are not physical sensor,
radio, alarm-output or flash tests. LeakSanitizer is disabled where ptrace prevents
it; no leak-check claim is made.

The owning workflow retains exact source archives, compiler dependency manifests,
commands, logs, binaries and target repeat hash/size receipts. Historical lost reconstruction tests
are not used as evidence for this source. Product store preparation proves
metadata admission and byte preservation, not physical firmware execution.

See [key-independent handoff qualification](qualification/scanner-key-independent-handoff.md)
for the repaired source pins, contract boundaries and exact evidence scope.
