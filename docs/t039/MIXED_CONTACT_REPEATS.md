# T039: sampled holds followed by queued shortcut taps

## Reproduction and scope

The unchanged Utilities `6b2047500905f8f45087cb49989103a926fc5c63` controller
loses a complete second tap when the preceding contact was already sampled and
reported as held. The production FT6336U 0.2.1 provider emits this sequence:

1. At 25 ms, DOWN is sampled by the app and sends Ctrl+Alt+Delete.
2. At 30 ms, the first contact ends.
3. At 35 and 45 ms, a second complete DOWN/UP pair enters the raw queue.
4. At 60 ms and on subsequent pumps, only the first neutral report is sent.

The baseline emits two keyboard reports instead of four. The same regression
still fails when additional pumps are allowed. Physical I2C/GPIO/time are test
doubles; the app controller, raw event queue and FT6336U implementation are real.
This establishes a remaining software defect, not the cause of an unspecified
historical hardware report.

The October 14 entry-return/retained-invocation report predates the separate
shared optional-storage first-poll repair. Its diagnosis is retained in System
`docs/shared-app-crashes/README.md`. This change does not reimplement that repair
or claim new physical launch qualification.

## Bounded repair

The Buttons trace distinguishes a contact whose press was already reported from
an unreported queued tap. Its UP neutralizes the sampled hold, without adding a
duplicate shortcut. Later validated complete contacts remain in the existing
FIFO and are sent on subsequent pumps. A sampled neutral or one queued pair is
sent per pump, preserving time for the HID owner to service ACL credits.

The 100 ms limit remains on unreported queued taps; an already-sampled hold can
last longer. Contact identity, same-tile bounds, monotonic event time, sequence
continuity, neutral gating, native presentation ownership, authentication,
encryption, notification subscription and connection-generation checks remain.
Failed release or subsequent press stops the session through existing checked
cleanup. No pairing, bond storage, provider or Touchpad behavior is changed.

## Reproducible host verification

Consumed dependencies:

- Utilities baseline: `6b2047500905f8f45087cb49989103a926fc5c63`.
- Generic HID System: `f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a`.
- Native Buttons System: `f3731ab02127dea86a308472e3780068c5a0a1cf`.
- T-Watch-S3 provider source: `851b8998e804c57b2c9f42d9f8fd0a69e79f2b41`,
  `drivers/current/twatch_touch/driver.c`, version 0.2.1.
- Unmodified Drivers: `a7ae39994603c4545a3c014f09051ecfc12084d2`.

Checks performed:

- `python -m unittest discover -s tests -v`: 269 tests pass.
- `scripts/test_hid_apps.py`: real app/model/gesture tests pass normally and
  under ASan/UBSan; production Nova frames are captured.
- `scripts/test_hid_buttons_watch_queue.py`: all 50 cases pass normally and
  under ASan/UBSan, including 18 added mixed-contact cases. They cover a later
  held contact, different tiles, mouse/wheel, longer sampled holds, stale or
  interrupted input, identity changes, security/subscription changes, disconnect,
  generation changes and press/release failures.
- `scripts/test_hid_buttons_wire.py --mixed --taps 3 --reconnect`: exact
  production-app/FT6336U/NimBLE ATT press/neutral FIFO passes in report and boot
  modes, normally and sanitized. Each run checks two transactions across a
  saved-LTK reconnect and rejects delayed watchdog duplicates.
- Unmodified Drivers `test/run_ble_hid_test.sh` passes normally and sanitized,
  including cryptographic numeric-comparison pairing and reconnect controls.
- `scripts/test_hid_renderer.py`, both color and `--paper`: production adapter
  controls pass normally and under ASan/UBSan.
- `scripts/test_ble_buttons_native.py --temporal --async --mixed-contacts`:
  the production native adapter passes the new case across all seven profiles,
  normally and sanitized. The existing expanded-recipe temporal controls also
  pass 98 cases per mode. Full-feature retained-failure cases 7 and 10 require
  the separate target recipe and were not run in this expanded host recipe.
- The native geometry/controller C fixture passes all seven profiles normally
  and sanitized. Touchpad color and paper preprocessed source tokens are
  identical to the unchanged baseline.
- `git diff --check` passes.

For sanitized native runs, set `ASAN_OPTIONS=detect_leaks=0` on the Python
runner as well as its child scenes. The first attempts without that setting
failed at the layout subprocess because LeakSanitizer cannot run under this
executor's ptrace environment. Those attempts are not passes. AddressSanitizer
and UndefinedBehaviorSanitizer subsequently pass; leak detection is untested.

The queue runner's `--seam --sanitize` builds the application bridge. For the
sanitized Python wire runner, preload `$(cc -print-file-name=libasan.so)` and set
`ASAN_OPTIONS=detect_leaks=0:verify_asan_link_order=0`, using Drivers' sanitized
`build/ble-hid-san/driver.so`. The ordinary provider is
`build/ble-hid-host/driver.so`. Each runner records commands, source hashes and
results in its requested output directory.

## Release and qualification boundary

This is a source repair for review. No version, tag, release asset, catalog,
firmware image, installation or hardware state is changed. Drivers and secure
pairing are unchanged.

The existing immutable `native-ble-buttons-0.1.30-dev1` qualification deliberately
requires selector-off bytes identical to its retained predecessor. A shared
controller change is not that frozen artifact. Its gate is preserved, not
weakened or rebaselined. A separately versioned candidate and qualification
recipe are required before artifact publication; the fixed-version publication
workflow remains restricted to the existing explicit main-branch dispatch.

Target ELF builds, the full native release pipeline and remote CI have not been
run for this patch. Physical panel/touch behavior, Bluetooth RF, real host key
acceptance, host switching, deployment and hardware qualification remain open.
Passing these software checks does not complete T039's physical acceptance.
