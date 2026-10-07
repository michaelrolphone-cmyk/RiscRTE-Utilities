# BLE HID application integration, Watch 1.0.5+

This is a standalone application profile. It does not modify the Watch builder,
staged SDR 1.0.3/1.0.4 packages, scanner profile, SDR source pins or historical
Reader migration apps. Neither a BIN nor a hardware-qualified result is produced
by this work.

## Sources and outputs

- `hid_apps` inventory: `ble_touchpad` and `ble_buttons`, both 0.1.1.
- Exact standalone System-Apps source: `d5b6c0fa7c06bb36a776e7ce501028a9827221e2`.
- Generic `RiscBluetoothHidV1.h` is an exact copy of Drivers' public API and is
  SHA-256 locked in `sdk/hid-sources.json`.
- `scripts/build_hid_apps.py` creates `dist/hid-apps/{ble_touchpad,ble_buttons}.elf`
  plus sidecars, notices and content hashes. It verifies exact clean System
  source, target ELF ABI, imports/exports and the real ELF structural validator.
- Runtime minimum is 0.1.34 for this new profile. The later combined Watch
  deployment must use its own exact consolidated component pins.

## Required deployment changes

1. Install the separately built generic `bluetooth.hid@1` provider and its
   audited dependencies. HID is logical **Global0**, not a physical instance 19;
   do not add a fake board device. The application grants must resolve the unique
   HID provider through instance 0. The driver exclusively owns native HCI and
   its private namespace 10 bond records. Apps never read or write bond secrets.
2. Grant both applications `display.output@1`, genuine `input.touch.raw@1`
   instance 6, `bluetooth.hid@1` Global0, `alarm.service@1` and shared
   `storage.key-value@1` namespace 1. Their own policy read uses only `quick_radio`;
   the existing quick-controls adapter owns its usual explicit settings writes.
3. Grant **only ble_buttons** private `storage.key-value@1` namespace 11 for
   `hid_buttons`. No namespace 11 grant is required by Touchpad.
4. Provide existing optional `board.battery@1`, `rtc.clock@2`, `net.wifi@1` and
   `bluetooth.hci@1` grants used by board telemetry and shared quick controls.
   The HID host itself always uses `bluetooth.hid`, never the app HCI grant.
5. Build with `PORTABLE_RADIO_SESSION`, `PORTABLE_ALARM_CLIENT`,
   `PORTABLE_NOVA_UI`, `PORTABLE_APP_OWNS_TOUCH_CHROME`, `PORTABLE_QUICK_ACTIONS`,
   `PORTABLE_QUICK_RADIOS` and explicit `PORTABLE_RETURN_APP="springboard.elf"`.
   Reuse the ordinary adapter plus its four quick-control sources. The target
   Watch additionally supplies its normal navigation, sleep and time hooks.
   Use `PORTABLE_TOUCH_ROTATION=0`; the app rejects other rotations at compile time.
6. Add both cards to Utilities and include both exact ELF manifests in the
   cohort. The combined opt-in Springboard catalog must have capacity **20**
   (the SDR cohort needs 18). Keep generic/default catalog capacity unchanged.
7. Extend the later cohort's reviewed shared-key-value migration authority with
   exactly these two new application entries, alongside the previously approved
   entries:

   ```json
   {"application_id": "ble_touchpad", "api": 1, "namespace": 1}
   {"application_id": "ble_buttons", "api": 1, "namespace": 1}
   ```

   Namespace 11 is new private app state; namespace 10 is driver-owned bond
   storage. Neither is an existing shared-namespace migration entry.

The separate integration owner must coordinate one stable Utilities source head
with the other in-flight component changes and preserve existing OTA/NVS/app-data
semantics. No later integration authorizes a merge or physical device action.

## Verification

```sh
python -m unittest discover -s tests -v
python scripts/test_hid_apps.py --system-apps /exact/clean/system-apps
python scripts/test_hid_renderer.py --system-apps /exact/clean/system-apps
python scripts/build_hid_apps.py --system-apps /exact/clean/system-apps
```

The model suite covers taps, two-contact movement, replacement, surface bounds,
time wrap, neutral gating, all selectable HID keys and version/checksum/readback
persistence. App fixtures compile both actual source variants under normal and
ASan/UBSan builds: scoped grants, radio policy, paired/reconnect start, six-digit
comparison, carried-over input rejection, ordered reports, modifier cleanup,
host disconnect, gaps/errors, failed opens with cleanup tokens, close/release/
unsubscribe retry, failed saves, cancel and Back.

Separate production-adapter fixtures compile unmodified app and adapter sources,
with real Nova fonts/rasterization, copied fake peripheral APIs and independent
raw subscribers. They exercise Pair accept/reject, one/two-finger input, held
keys, reconnect, disconnect, gap cancellation, alarm and quick-control
interruption, edit/save, nested and root Back, stride guards, and complete grant
teardown. Screenshots live in `build/hid-apps/*-frames` and
`build/hid-renderer`; they are real software rasterizer captures, not mockups.

Physical Watch touch latency, Bluetooth pairing/interoperability with actual
hosts, actual disconnect/reconnect behavior, power/sleep and NVS/power-loss
qualification remain deployment tests. No physical test is claimed here.

## Pairing diagnostics and Watch touch regression

HID 0.1.1 emits bounded serial diagnostics through the existing Runtime diagnostic
API: connection state/error transitions, a fresh pairing contact's target and
coordinates, ignored-contact reasons, and confirmation request/result. Pairing
numbers, bond material and HID report contents are never logged. A held contact
is logged once, with at most one contact-cancellation reason until neutral.
Each continuous event-gap episode is reported once; ordinary polls are silent.

The Watch deployment must use a touch provider whose snapshot sequence is the
last emitted event sequence. The old FT6336U provider advanced its snapshot one
event ahead, which the HID app correctly rejected as a gap, cancelling every
Accept and Reject contact before confirmation reached the Bluetooth provider.
The generic provider and previous API fakes already used matching sequences,
so their isolated tests could not expose this deployment defect.

Exercise both production HID apps, the real shared adapter and the production
Watch FT6336U driver, replacing only its physical I2C/GPIO/time dependencies:

```sh
python scripts/test_hid_renderer.py --system-apps /exact/system-apps --watch /exact/watch
```

The optional `--scene pair-accept` or `--scene pair-reject` narrows this same
normal and ASan/UBSan test to the reported failure. Use
`--watch-touch-source drivers/twatch_touch/driver.c` to reproduce against the
historical provider; the corrected `drivers/current/twatch_touch/driver.c` is
selected automatically when available. The current Watch cohort
owns the corrected driver version and runs this cross-repository regression;
standalone Utilities CI continues to test the generic raw-touch API profile.

Add `--runtime /exact/runtime` to link the production Runtime diagnostic logger
and its existing host USB shim. This asserts live serial output and `diag`
replay after the HID app returns; set `HID_RENDER_SERIAL_ABSENT=1` to verify
retention while disconnected and replay after reconnect. This is host linkage
evidence, not a physical Watch USB-capture claim.


### HID 0.1.2 failure visibility and regression

The app records its original stop reason and a copied state before teardown,
then records the final close separately. A nonzero OFF error can describe the
original transport failure; it no longer replaces that reason with a misleading
cleanup-only message. With Drivers ble-hid 0.1.2, the optional API suffix adds
numeric transport stage, disconnect/security/notification results, poll-gap
maximum and independent host/native cleanup results. Codes, keys, addresses and
input contents are never logged. Older API prefixes remain supported.

The renderer fixture can use `--low-battery` with the delivered System adapter.
It covers saved-session mouse reconnect, a missing mouse notification
subscription, transport-error return and explicit Reconnect, and the below-10%
policy hook. With `--watch` and `--runtime`, these use the production FT6336U
backend and serial diagnostic journal over their existing host shims. The radio
in this UI fixture remains a copied peripheral API fake; the Drivers protocol
suite independently runs production NimBLE and cryptographic pairing/reconnect.
Physical host/controller behavior is still unqualified.
