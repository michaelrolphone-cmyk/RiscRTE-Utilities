# Capability-sized Stopwatch

`PORTABLE_STOPWATCH_NATIVE_UI` is an opt-in RGB565 presentation for the existing
raw-RTC and native-UTC Stopwatch controllers. The selected app lays out directly
at the reported width and height (160–1024 pixels on each axis); no finished
240-pixel viewport is enlarged. Numeric and caption glyphs, rounded primitives,
and all four hit regions are drawn in native pixels. Numeric text selects a
fitting face rather than cropping long hours. Full error messages use up to two
bounded text lines. Back remains a visible native control.

The original monotonic timer, restore rules, clock-discrepancy detection,
approximate marker, explicit save/retry, two-step Reset, 99:59:59.99 limit, and
write-on-explicit-action policy remain in their original controllers. Selected
raw-color mode requests the already reserved private namespace2 explicitly,
avoiding ambiguous default-instance resolution beside shared preferences1.
Legacy feature-off color and paper requests remain unchanged. Raw `stopwatch`
records and UTC `stopwatch_utc` records stay separate; neither is migrated.

## Ownership

System's private owner13 owns presentation generations, visibility watermarks,
ordered contact admission, and Quick replay. Same-layout timer refreshes retain
owned gestures. Foreign pixels invalidate the map; closing Quick or an Alarm
requires a completed native repaint before new touches can act. Top-edge taps
retain their original DOWN sequence rather than inventing a later gesture.

The native app requires checked custody, NOVA, native canvas and app-owned touch
chrome. Back queues the shared adapter's orderly return and pending-frame drain.
The UTC controller retains its existing terminal barrier and verified-save
launch guard. Native snapshot admission pauses when Quick requests the screen,
so 50ms timer refreshes cannot indefinitely occupy a slower display's mailbox.
This does not change non-Stopwatch compositions or shared audio/Alarm ownership.

## Explicit qualification builds

`profiles/stopwatch-native-qualification.json` contains mutually exclusive RTC
and UTC development compositions at version0.1.26. This is above the existing
0.1.25 source lineage. The adjacent legacy manifest and historical portable
inventory remain0.1.6; the qualification builder emits its separate native
manifest at0.1.26. The selected
composition requires display, ordered touch, navigation, battery, shared
preferences1, private Stopwatch storage2 and the corresponding clock/Alarm API.
The UTC profile reads runtime.realtime and never requests RTC or time-writing
control. Exact System/Runtime source commits are in
`sdk/stopwatch-native-sources.json`.

These profiles are not catalog entries and do not update any Watch/X4 product
recipe, accepted artifact, or physical fixture. The two variants are not intended
to be published simultaneously under one package version.

From the Utilities repository, with the exact dependency checkouts available:

```sh
for mode in '' '--snapshot' '--utc' '--utc --snapshot'; do
  python3 scripts/test_stopwatch_native.py --system-apps ../system --runtime "$RUNTIME" $mode
  python3 scripts/build_stopwatch_native.py --system-apps ../system --runtime "$RUNTIME" \
    --utilities-commit "$(git rev-parse HEAD)" --xtensa-cc "$XTENSA_CC" $mode
done
python3 scripts/test_stopwatch_native_optout.py --system-apps ../system --runtime "$RUNTIME"
python3 scripts/test_stopwatch_hours.py --system-apps ../system
python3 -m unittest discover -s tests -v
```

The actual controller/adapter suite exercises160×160,240×240,240×280,320×240 and
410×502, direct and immutable-snapshot paths, synchronous and120ms presentation,
normal and AddressSanitizer/UndefinedBehaviorSanitizer builds. Cases cover
start/pause, repeated actions, reset cancellation, storage failure/retry,
restoration, future anchors, the hour limit, UTC loss/retention, navigation,
private namespace2 denial, invalid storage API/size and isolated shared/private records,
pre-visibility/held contacts, cross-control drags, touch gaps, Quick and Alarm
interruption, and repeated module entry. Stride guards and pending-frame hashes
are checked by the asynchronous display fixture. Hardware peripheral providers
are doubles, not physical-device evidence.

Compatibility checks compare preprocessed production tokens against the exact
pre-change Utilities source and exercise existing raw/UTC paper controllers.
Separate System tests verify owner13 direct/snapshot/Quick custody, private symbol
visibility, and feature-off parity. Target builds reject wrong or dirty source,
validate ELF admission and public exports/imports, and retain source hashes,
compiler, feature flags, boot grants, version and artifact hashes.

Physical RTC continuity, actual display/touch behavior, real audio stream custody
and platform sleep/wake remain hardware qualification work. Source tests do not
close those holds or authorize a release.
