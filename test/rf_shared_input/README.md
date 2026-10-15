# RF shared text-input workflow qualification

`python scripts/test_rf_shared_input.py --system-apps /path/to/published/system`

The runner executes the actual `Apps/waterfall.c` `app_main`, production app
adapter, production shared text host, production scene host, and independently
selected scene presentation profile. The fixture includes the app verbatim only
to observe private state and seed boundary conditions. Every edit, Done, Back,
Home, Save, Train, and close operation then follows production input and service
paths. The `ui-*` scenarios discover all four editors entirely through ordinary
input from live startup.

## Profiles and provenance

Pass `--watch-recipe` and `--x4-recipe` to use a device build receipt. Both the
original aggregate receipt (`waterfall` member) and the new RF component receipt
are supported. All `-D` flags are preserved; explicit native-only additions and
omitted target machine/LTO options are recorded in `qualification.json`.

- Watch: 240×240 RGB565, the complete published RF app profile, compact scene
  host (`SCENE_HOST_WATCH_240=1`, `SCENE_HOST_TELEMETRY=0`), and
  `scene-profile-compact-color` with rotation zero. The target's canonical
  `PaperPresentation.h` preinclude is retained in native qualification.
- X4: the complete resident/raster/paper RF app profile, published Runtime SDK,
  and the **packaged full** scene/text hosts. The app adapter uses its existing
  `PORTABLE_DISPLAY_ROTATION=90`; the independently specified scene profile is
  `scene-profile-portrait-monochrome`, with `SCENE_DISPLAY_ROTATION=270`.
  These are different APIs with different rotation conventions. Touch reports
  are already portrait. The full host's generated telemetry glyphs are built
  from its published generator/TTFs and must match SHA-256
  `2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1`.

Separate `--runtime`, `--x4-runtime`, and `--x4-host-system` arguments prevent an
app-adapter upgrade from silently upgrading X4's unchanged native SDK or hosts.
Source commits, dirty state, source hashes, exact commands, build receipt hashes,
profile flags, and all scenario logs accompany the result.

## Coverage

All four 16-character names are exercised: frequency label, signature, temporal
event, and context signal. Coverage includes:

- Capacity 17 bytes, actual shared-host key geometry, mixed ASCII, navigation,
  over-limit input, Done as draft-only, cancellation from empty and seeded
  drafts, acceptance/cancellation/reopen, and explicit Save/Train
- Missing, short/old, and unavailable host tables; failed reacquisition; retained
  cleanup and runtime loss at open, poll, and close, with no later I/O or reentry
- Normal, rapid, absent, and delayed display presentation; immutable submitted
  raster bytes; draft application only after clean close; continuous RF capture
  and canonical analysis during editing and repeated close-AGAIN
- Keyboard-open activity beyond the normal 60-second idle limit, followed by
  continued live operation, with no sleep or resident-policy dispatch
- Hardware and header Back/Home; explicit Home cancellation of accepted drafts
  and signature/event/context training; Watch launch refusal/retry; X4 clean
  child return to the existing resident Home host
- Four saved-record Home workflows that decode the stored record and assert
  byte-for-byte persistence and write-count preservation
- Four failed-commit Home workflows (label/signature KV, temporal app-data,
  context fingerprint data). Watch additionally refuses Home once and exits
  through ordinary Back. No canceled write is retried and no RF storage read or
  acquire is performed after Home; the paused app marks canceled records unread

Default scenario completion walks ordinary Back navigation to root only after
the scenario has checked its outcome. Explicit Home cases assert that this
fallback was never used, including when clean host close waits for a delayed
presentation token.

## Pixel comparison

Nineteen deterministic non-keyboard views are compared byte-for-byte with both
specified predecessor sources:

1. `--baseline-utilities`: published Spectrum/shared-input Utilities source
2. `--watch-baseline` or `--x4-baseline`: exact packaged RF predecessor source

Each baseline uses the same selected adapter and host profile as the candidate,
so the comparison isolates the RF application migration. It is not an image
comparison against a physical panel or the predecessor's entire firmware image.
The views cover startup, spectrum, waterfall, monitor, controls and scrolling,
label/signature/event/shared lists and editors, capture/review states, monitor
editing, and receiver controls. Normal and ASan/UBSan captures are also compared
byte-for-byte. PPM captures are converted incrementally to lossless PNG.

## Limits

RF IQ, display, navigation/raw touch, battery, RTC/realtime, KV/app-data, alarm,
radio control, Contexts, and resident dispatch are deterministic native fixtures.
No hardware, PMU/actual sleep, radio silicon, ELF loading, native flash/SD storage,
or unpublished persistence implementation is exercised or claimed here.

The runtime retention contract is real at the app/adapter/provider API boundary;
the native Runtime implementation remains a fixture. ASan and UBSan are enabled
with nonrecovering errors. LeakSanitizer is disabled for this executor's ptrace
restriction; explicit allocation, grant, subscription, frame/token, and stride
canary assertions cover every clean exit. Retained invocations intentionally keep
their exact resource set frozen.
