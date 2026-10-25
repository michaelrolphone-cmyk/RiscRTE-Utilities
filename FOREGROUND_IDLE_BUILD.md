# Ordinary foreground Light build selection

`scripts/build_portable_apps.py` accepts the same explicit four options as the
selected System builder:

- `--foreground-idle-helper-dir`
- `--foreground-idle-runtime-dir`
- `--foreground-idle-drivers-sdk-dir`
- `--foreground-idle-fallback-seconds`

The first three are selected together. `--navigation` is required to retain the
filtered power-key dependency across the temporary helper-grant release. The last is an optional 5–3600 second
missing-key fallback (15 in this opt-in candidate). Confirmed valid namespace-1
`sleep_idle` values win; invalid or unavailable values defer idle. The helper
never chooses Deep/Hybrid or changes saved preferences.

The Utilities builder retains its historical System revision default and exact
clean-checkout verification. Pass `--system-apps-revision` with the exact committed
System revision containing this feature. It imports that checkout's
`scripts/portable_foreground_idle_build.py` for validation, staging, flags, source
selection and capability declarations; no SDK-copy fork lives in Utilities. The
helper must itself match the selected committed object, not an untracked or
modified script added to an older checkout.
Native Stopwatch, if selected, must use the same exact canonical Runtime pin.

Calculator, Stopwatch and optionally Battery get the returning Light helper.
The added namespace-1 KV grant does not replace Stopwatch namespace 2. Receipts
record `[2, 1]` for Stopwatch and `[1]` for the other tools. Namespace 1 is read
only by helper behavior, not by capability security: KV1 also exposes writes.
Physical power/display/key provider selection belongs to the deployment owner.

Each selected app row in `build-evidence.json` includes the actual compiler
command, definitions, capability requirements, source hashes, explicit KV
bindings and the shared System recipe receipt. Consuming host integration must
use these actual inputs instead of inventing a second flag/source list. All
omitted-flag build behavior and bundled SDK pins remain unchanged.

Host routing qualification:

```
python scripts/test_portable_foreground_idle_build.py --system-apps /path/to/selected/system
```

This does not establish real-device battery current, wake latency or retention.
