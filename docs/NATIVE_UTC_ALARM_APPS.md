# Native UTC Alarms and Countdown development profile

This opt-in profile implements Alarms 0.2.5 and Countdown 0.1.9. The ordinary
Watch and raw paper source paths, manifests, configuration keys and versions
stay unchanged. `ALARM_NATIVE_UTC` selects a separate controller; it must be
combined with the shared native custody, native toolbar, launch guard, and
tagged alarm API 2 flags. No install catalog, product, BIN, release or hardware
change is included.

## Exact composition

- App baseline: Utilities `bb81ab0cdc137e1b2fddcfbe191880c1c6e649ad`.
- Tagged native visual service and pure scheduling helpers: Utilities
  `637e13b0bce62ad49b756bec2468a6271d163fc7` (service 0.4.4).
- Frozen realtime/timezone helpers: System
  `1d589d90bf27c7ffb76420de46564088ddb3714f`.
- Native toolbar/custody and tagged portable alarm adapter: System
  `81f884b8a053cf917054fb1433c7850714cd0c48`.
- Canonical Runtime SDK: `30dcec5ce6ce33223f2b203a2399283e1f758567`.

The provider and app histories are merged, including the original paper
history. Provider/header/build sources equal the qualified tagged provider
checkpoint. The earlier provisional suffix resolution is superseded. Native
apps request `alarm.service@2`, validate the complete tagged descriptor and
require visual output mode. They never infer the meaning of a v1 suffix and
never downgrade to API 1. The deployment must bind the native UTC service
profile; the descriptor alone does not identify a service's time basis.

System adapter 81f884b contains separately owned, publication-blocked source.
It is used only for local composition testing/building here. The builder reads
it in place, with temporary header symlinks; no System source is vendored or
committed to Utilities. Publication remains blocked until those dependencies
are cleared and a complete published composition is selected.

## Behavior and authority

The runtime realtime client opens a readonly grant, reads one typed snapshot,
and closes it for every sample. It never acquires an external RTC or realtime
control grant, seeds time, migrates records, or writes timezone preferences.
`UNSET`, malformed values, safe IO errors and unavailable grants display
unavailable. UTC must fit 2000-01-01 through Unix `INT32_MAX`; the 60-second
service recovery interval is also reserved when a deadline is created.

`alarm_utc_cfg` and `timer_utc_cfg` store absolute UTC seconds since 2000. Old
`alarm_cfg` and `timer_cfg` records are never read or reinterpreted. Namespace 3
owns the selected app's configuration. Namespace 1 supplies the canonical TZ1
`time_zone` preference and `time_format`. Missing timezone is the documented
virtual UTC default. Invalid or unreadable timezone data blocks local alarm
planning and local clock display. Countdown timing remains UTC-based even
when the timezone preference is unavailable or changes. Existing deadlines
are never shifted by a timezone edit.

One-shot alarms use the complete 419-entry frozen timezone catalog. They reject
ambiguous folds and nonexistent gap times explicitly, retaining the existing
policy. Saved alarms display their UTC deadline in the current selected zone.
The shared paper/QuickActions clock calls the same app-owned local-time hook,
with a fully closed native grant before the callback returns. It has no RTC
fallback. Native visual Alarms show the selected timezone and no volume action.

Safe typed KV IO preserves an unconfirmed pending record. Cancel, Back, Home and
QuickAction launch cannot discard it; an explicit retry writes and verifies the
same revision. IO-after-commit is successful only when exact readback confirms
the expected record. CONTEXT, unknown status, a nonempty failed acquisition,
failed/uncertain grant release, and retained service/output status immediately
call the shared custody fence. No further provider/storage/status work,
rendering, polling, cleanup, release or free is allowed afterward. Ordinary
storage errors are not treated as lost custody.

`Apps/native-utc-alarms.json` records the opt-in versions and deployment grants.
The apps require native realtime read authority and tagged alarm API 2; they
have no RTC or realtime control authority. Shared QuickActions retains its
existing namespace 1 preference behavior. The runtime storage ABI grants a
namespace, not per-key enforcement; the listed app keys describe actual usage.

## Reproducing the checks

Use existing toolchains, without installation:

```
python scripts/test_native_utc_alarm_apps.py --system-apps SYSTEM_TIME --runtime RUNTIME
python scripts/test_native_utc_alarm_adapter.py --system-apps SYSTEM_TIME --adapter SYSTEM_ADAPTER --runtime RUNTIME
NATIVE_APP_CC=XTENSA_GCC python scripts/build_native_utc_alarm_apps.py \
  --system-apps SYSTEM_TIME --adapter SYSTEM_ADAPTER --runtime RUNTIME \
  --raw-system SYSTEM_a7f08a9 --watch-system SYSTEM_13f32d3
```

Direct production-controller tests cover isolated records, all catalog entries,
DST gaps/folds, saved deadlines across timezone changes, signed-2038 bounds,
malformed/UNSET/absent native time, descriptor rejection, partial storage
outcomes, exact revision retries and sticky custody. The helper is the frozen
production `PortableRealtimeClient`, not a reimplementation. This first suite
uses a test-only adapter fence declaration/double.

The composition suite links the actual Alarms/Countdown controllers and actual
System adapter, including native toolbar, QuickActions and tagged alarm client.
Hardware/provider tables are deterministic doubles. It runs 132 fresh-process
cases across paper/Nova and normal/ASan/UBSan: cancelled and held contacts,
repeated actions, Home, safe partial saves, explicit retries, read grant closure,
Quick clock, retained native/KV/service results and failed release. Retained
cases verify unchanged provider counts, framebuffer contents and no frees after
the fence, including finalization. LeakSanitizer is disabled because this
executor's ptrace environment does not support it; ASan and UBSan remain active.

The target builder uses pinned Xtensa GCC 8.4.0 2021r2-patch5, verifies ELF
structure and exact imports/exports, emits separate app manifests and boot
policy, records transitive source hashes and licenses, and compares four entire
flag-off Watch/paper Alarms/Countdown ELFs with exact baseline bb81ab0 bytes.
The resulting native ELF handles both paper and Nova displays. Structural target
validation is not target execution or physical qualification.
