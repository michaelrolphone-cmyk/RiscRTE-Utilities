# Current mixed-service headless Alarms

This adds the existing GUI-free Alarms owner and semantic control provider to the current mixed Alarm/Points source lineage. It does not replace a visible default application or start a competing scheduler loop. A product explicitly selects `alarms_headless.elf` as its owner and grants only `alarm.control@1`; neither GUI nor network capabilities are needed.

The UTC control profile is compiled with the current tagged clock-edit interface. Schedule preparation and application defer before reading/writing schedule KV while an edit intent is live; retained custody fences all further work. Exact commands remain retryable after cancel, finish or recovery. Unknown descriptor features and malformed editor descriptors are refused. Raw-RTC and non-editor API2 profiles retain their original tests.

The current test profile composes published Runtime318d7728 (all2850 source files match local046f4c22; published tree differs by additional provenance files), Systema24bf10e and this Utilities source. It executes actual host-loaded owner/control/scheduler ELFs through the production Runtime, with modeled clock/storage, and checks cold arm/fire/dismiss/rearm/cancel and absence of GUI grants/frames. Separate tests exercise current mixed scheduler clock-edit INTENT/cancel/finish/reset-recovery and no-write refusal. The historical PR100 watch/paper profile is not replaced by this current-profile test.

Run `scripts/test_headless_current_control.py` and `scripts/test_headless_current_runtime.py` with `--runtime`, `--system-apps`, and a new `--output` directory; repeat with `--sanitize`. `scripts/build_headless_current.py` builds the GUI-free target owner and raw/current-UTC control services and checks reproducible owner bytes and ELF structure. CI archives logs and outputs.

Host execution and target structural validation are not physical-device qualification, native target instruction execution, or simultaneous visible-default ownership. The visible default's existing settled scheduler pumping is preserved.
