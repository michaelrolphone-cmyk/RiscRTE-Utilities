# Native Alarms / Countdown presentation

Opt in using `build_badge_native_alarm_apps.py --native-alarm-presentation`.
Receipts report `native_alarm_geometry=true`. Omission retains the historical
logical safe-viewport profile and its app versions. This branch is distinct from
legacy `DAILY_NOVA_APP`; it borrows native primitives, not legacy editor routing.
The existing service, clock editor protocol, writer records and grants do not change.

`Apps/alarm_native_layout.h` is a pure content-local native-pixel layout contract:
width/height 240..1024 inclusive, kind1 Alarms / kind2 Countdown, page0 root,
page1 field list, page2 field editor. Invalid inputs return `valid=false`.
Rectangles are half-open; inactive controls have zero rectangles. Production
render and hit dispatch use these same rectangles. System owns the sole safe
origin/inverse touch mapping. Existing original font faces are selected by width,
not bitmap-scaled. Essential numeric values preserve their full representation;
long descriptive labels may ellipsize. At328 content height the root targets are
48px (Edit/Arm/Cancel),44px Retry; smaller supported content retains positive
non-overlapping targets rather than assuming a466 physical display.

Alarms explicitly arm once, display the saved deadline's local date, and use the
saved12/24-hour preference. Twelve-hour values include AM/PM; editing hour wraps
within the selected half-day, with a separate AM/PM toggle. Countdown is always
HH:MM:SS and independently wraps fields; zero duration remains rejected. Done
and nested Back edit/navigation never persist. Only Arm/Start/Cancel or exact
uncertain-write Retry dispatch the existing verified writer.

The application owns Back navigation (editor → list → root → return), including
short physical Back. The shared adapter still owns guarded global Home, modal
custody and ticket-aware returning Light. Uncertain Save blocks editing/Back/Home;
retry preserves the exact pending deadline/revision. No audio, volume or haptic
control/capability is introduced. Alerts remain visual only.

Owning qualification:
- `scripts/tests_native_alarm_geometry.py`: actual production app/controller and
  selected System native renderer, modeled storage/time/raster; generalized layout
  sweep, six asymmetric/round/min/max safe-content fixtures, guard pixels/odd
  stride, all24 hour choices, independent wrapping, explicit Save/Cancel/retry,
  DST rejection and range boundaries. Captures are modeled PPMs, not device photos.
- `scripts/test_native_alarm_adapter.py`: real System adapter/app cold acquisition,
  native ordered touch, nested navigation, uncertain-save fences and modal recovery.
- Historical `scripts/test_badge_native_alarm_apps.py` lanes remain intact;
  `--native-alarm-presentation` adds the native owning scripts.

All accept `--runtime`, `--system-apps`, `--system-revision`, `--output` and optional
`--clock-edit-authority`. Product qualification separately replays its full
Runtime/Deep/wake cohort and admissions against exact target recipes. Physical
RTC accuracy, sleep power and actual panel/button behavior are not proven here.
