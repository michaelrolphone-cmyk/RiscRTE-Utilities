# Scanner 0.3.3 ordinary publish input preservation

A current-revision navigation event arriving immediately after a successful
component update was consumed by the following repeated lifecycle.configure.
The ordinary publish path now configures only when it opens a new scene session.
Resident policy is fixed for that session. Closing/reopening still configures a
fresh session; explicit handoff, alarm and rejected-manual paths are unchanged.
No provider semantics, persistence formats or capability grants changed.

Source baseline: Utilities f057c6cf5b424d2973408c0824412a947b6fe95d.
Current SDK/provider: System eb4dd5306906d57e65f24f911d325bc87370666b.
Retained provider: System bb0cadf3cec3445a64e11513b47b2676f3fdd92d.
Runtime: 0052d1fc42e9cfc82d626c5a247d460919c87935.

The new test includes the actual Scanner root and runs its publish against the
production scene provider. A test-only update wrapper injects a navigation key
immediately after the actual component update returns, after the root snapshot.
Physical display/touch/navigation transports and unrelated app services are
doubled. Successful changed update must leave configure count at one and deliver
rc=0, kind=3, revision=2, sequence=1. Closing and reopening must configure again.
The --app-root option permits the exact same test against original app source.
The original root instead configures twice and returns rc=1 (IDLE), kind=0,
revision=0. This demonstrates app/provider composition without claiming that this
focused test runs the full Runtime.

Run the new test for each provider and both sanitizer modes:

    python3 scripts/test_scanner_publish_provider.py --runtime RUNTIME --system SYSTEM --provider-system PROVIDER --output OUT [--sanitize]

The owning .github/workflows/ble-scanner-scene.yml now includes this regression.
Its existing focused suites and actual Runtime lanes remain separate evidence.
Both current/retained Runtime lanes run the actual app/scene/text ELFs on paper,
watch and amoled with explicit sensor/alarm/KV/hardware doubles. Each lane has
12 cases; normal and ASan/UBSan runs total 48 case executions. LSAN is disabled.
The focused suites retain their existing scope: 50 preferences, 70 root, 31 alarm,
33 presenter, 44 session cases, 67 background cases, plus the projection tests.

Canonical build uses the unchanged owning recipe:

    NATIVE_APP_CC=CC python3 scripts/build_shared_scene_apps.py --runtime RUNTIME --system-apps SYSTEM --app ble_scanner --output OUT

Scanner 0.3.3 ELF SHA256:
b4d9eae408b42265bae83e24ceecfceb474ae18a5b14776dbb55be8d91ad16ef.
Repeated build and all three copied selections match. The actual native ELF
validator passes. Compiler: xtensa-esp-elf-gcc 15.2.0 (esp-15.2.0_20251204).
These are software qualification results, not hardware or deployment evidence.
No product stores, task records, historical qualification or prior completion
records are rewritten by this repair.
