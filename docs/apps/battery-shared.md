# Shared Battery

`Apps/battery_scene.c` is one profile-free application. It consumes optional
`ui.scene`, `board.battery`, `telemetry.broadcast`, `storage.key-value` and
`storage.preferences`/`storage.key-value.atomic` contracts. The presentation provider owns native-class
ring, typography, layout, scrolling and input. Resident Home owns shared controls
and physical lifecycle. There are no product, geometry or backend branches.

## Preserved behavior

The selected product baseline is the existing POWER interface in
`battery_power.inc`/`battery_paper.inc`, not the unselected legacy T5 detailed
charger-register list. Overview, four detail pages, explicit refresh/retry,
nested Back and broadcast settings remain. The original basic battery sample
is unchanged. `STATUS_VALID` gates extended USB/charger/presence/thermal flags;
missing status means unknown. A full percentage is not charger completion.
Thermal regulation is not cell temperature. Failed reads clear stale samples.
Charging remains autonomous, and this application never writes charger policy.

The original `ble_broadcast` namespace-one four-byte record is unchanged:
`[0x62, 1, enabled, enabled ^ 0xa5]`. The original `quick_radio` policy is read,
not rewritten. Only an explicit enable/disable/retry creates a preference
intention. Admitted preference or atomic-key-value endpoints own copied transactions.
A generic contract adapter selects only from actually granted capabilities
before beginning an operation; there is no device branch, post-operation
fallback, or legacy put after failure. Accepted
pending operations retain custody through collect/cancel. Failed/uncertain
saves leave RF paused until explicit retry. Status is refreshed after commit
before another toggle can use it. Cleanup pauses RF before shared controls and
exit, drains pending receipts, and retains the invocation on terminal failure.

Missing-record defaults are deployment policy, not device branches. The
additive `TelemetryBroadcastPolicyV1.h` suffix preserves the existing API1
prefix while returning copied default settings. Provider configurations can
retain default-on or default-off behavior. A stored valid record always wins;
an old provider without a default suffix does not authorize guessing or
seeding a missing setting. Missing service/storage is disclosed rather than
silently interpreted as successful enablement.

## Source and qualification

The canonical registry builds a single `battery.elf`, repeats that recipe for
reproducibility and reuses full bytes in all three selections. Provider default
configurations are separate driver packages, not application variants.
`battery-scene.yml` runs focused controller/provider tests, actual Runtime
fixtures, target construction and selected-Runtime structural validation.
Test receipts define the proved scope; source/build success is not evidence of
executed physical hardware. Product policy projection preserves selected gauge
identity, namespace-one data, SD routes and unrelated applications, and updates
content-addressed provider paths when driver bytes change. Native endpoint
admission and exact product composition must be checked against selected inputs.
The selected X4 Runtime compiles the preference broker off and uses its existing
atomic-key-value contract; Watch/Wave admit the native preference endpoint.
A broker grant alone cannot enable a compiled-out feature.
