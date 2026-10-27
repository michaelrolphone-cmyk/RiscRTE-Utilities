# Explicit native UTC visual Alarms/Countdown clients

`build_badge_native_alarm_apps.py` is a new opt-in profile. Historical Watch,
paper, raw-RTC and X4 builders/profiles are unchanged. The new profile pins
Runtime273bdbc, System6e4cda9 and the recovered byte-identical API2 alarm SDK
at e801723. It uses GCC15.2, validates the target ELF and imports/exports, and
runs the exact Runtime target relocation loader at eight allocation alignments.
The evidence records compiled header/source hashes, commands and source pins.

The applications bind `alarm.service@2` instance0 to the explicitly selected
native-UTC visual schedule-only service0.5.0. **Do not bind the old native
Points-enabled service as a substitute**: missing Points settings can activate
its factory schedule. The apps do not acquire Points/audio/haptic/RTC authority.

Namespace3 owns the app's `alarm_utc_cfg` or `timer_utc_cfg` writer. Namespace1
provides shared preferences/timezone reads. No app has namespace4 occurrence
write authority; the service owns occurrence records. Runtime namespace grants
are namespace-wide, not key-filtered: the documented per-app key use is an
implementation contract, not a claim that Runtime restricts namespace3 to one
key. Native reader grants close per sample, and retained/context/unknown errors
fence all subsequent provider access. No native clock seeding is performed.

The existing generic safe-content renderer is selected with rotation0 and full
frames. This is not the NOVA/native-height profile. Both local Back (SHORT on the
Badge) and semantic Home (LONG) target `default.elf` only after the existing
uncertain-save guard permits departure. Alarm local time uses the shared IANA
preference; gaps/folds are rejected rather than silently choosing an offset.
Countdown deadlines and service records use UTC seconds since2000, never mixed
with Runtime's Unix seconds.

Run:

```
python scripts/build_badge_native_alarm_apps.py --runtime RUNTIME --system-apps SYSTEM --cc XTENSA_GCC --output EMPTY_DIR
python scripts/test_badge_native_alarm_apps.py --runtime RUNTIME --system-apps SYSTEM --output TEST_DIR
python -m unittest discover -s tests -p test_badge_native_alarm_profile.py -v
```

The app tests reuse owning native controller and actual adapter scenarios with
an explicitly selected ordered-edge touch fixture, normal and ASan/UBSan. They
exclude historical scenario15 (paper QuickActions swipe/panel; neither selected) and
exercise save/cancel/readback uncertainty, repeated taps, multi-contact cancel,
Home guard, typed/unknown/retained provider failures, UNSET/malformed/range-limit
time, selected timezone and clean/retained teardown. Providers/storage/native
clock are models; the tests do not execute Xtensa instructions or test hardware.

**Sleeping alarms are not integrated by this client build.** Home Deep and
foreground Light must acquire the tagged service, reconcile due state, retain
custody on uncertain outcomes, select timed sleep and resume using the same
service ticket. No existing foreground helper or Home policy is changed here.

### Explicit System successor qualification

The builder and owning test script accept `--system-revision FULL_40_LOWERCASE_SHA`.
Omitting it retains historical System `6e4cda9072905c15b9a5ebc9b22913fec444a89d`.
Selection is not a validation bypass: the provided System checkout must match that
exact SHA and have no tracked or untracked changes. Runtime and tagged alarm SDK
pins remain fixed, and receipts record the selected System SHA. Qualify both host
lanes and the target compiler/loader before using a successor; source selection
alone does not enable sleeping alarms or change application capability grants.

## Optional returning foreground Light

Add `--foreground-alarm-sleep` plus `--foreground-idle-helper-dir`,
`--foreground-idle-runtime-dir` and `--foreground-idle-drivers-sdk-dir` to
select the exact System foreground alarm recipe. Optional
`--foreground-idle-fallback-seconds` retains its confirmed-missing-key meaning.
The dedicated profile already selects the tagged alarm client; no duplicate
service grant is introduced. Omission preserves the previous ELF inputs.

Each selected application emits `<app>-build-record.json` with exact
`compile_inputs`, `foreground_idle`, dependency hashes and `grant_bindings`
(private namespace3 and shared namespace1). Manifests additionally require
power.sleep, display.power and input.power-key. Their physical instances remain
unbound in `required_grants`: the product integrator must supply mappings, as
with the other shared foreground apps. The historical `.boot-policy.json`
contains the profile's bound grants, not a complete deployable sleep graph.

The shared adapter suspends the existing stack; it does not save/cancel alarm
or timer edits. Uncertain-save state and launch refusal remain controller-owned.
An enabled hook is not itself evidence that a sleeping alarm has been tested
on hardware or within a complete product graph.
