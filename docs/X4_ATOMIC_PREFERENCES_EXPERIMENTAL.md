# Explicit Utilities preference writers, experimental X4 only

The opt-in `PORTABLE_UTILITIES_ATOMIC_PREFERENCES` flag also requires
`PORTABLE_UTILITIES_TARGET_X4`. No normal builder, package catalog, Watch input,
product source selection, firmware or hardware is changed.

The source is isolated from the exact .99 Utilities commit
`5c91c3517aa9f992e24fc453855701ec5a992355`, preserving its Hollow Trail source.
The separate OBD working tree at `52422df` is untouched; ancestry alone is not a
claim about its source differences. Only the identified settings writers change.

The explicit System input is `034720472ac20da5417aa48296b6ab5ac22d77f9` atop
Settings and Quick `a4c7805`. Its unmodified USER envelope is PFI0001 (176 bytes),
with copied expected/desired values, captured namespace-1 revision, and native
atomic submit/collect/cancel custody. Utilities does not redefine legacy puts.

- Battery pauses the broadcast client before submitting the explicit toggle.
  Its usual policy reload reads the same Core. Missing preferences remain virtual.
- BLE name editing captures revision, prior bytes and copied address identity
  before the text editor opens. Save/clear publishes the complete 40-byte alias or
  tombstone. Cancel releases an unsubmitted edit; stale saves never rebase.
- Enable Bluetooth captures `quick_radio` before any radio operation. It admits
  the hardware control grant/status first, commits the explicit setting, then
  enables and verifies the physical controller. Failure compensates only the
  originating committed receipt. PFR0001 (192 bytes) is ROLLBACK, contains the
  originating operation ID and receipt revision, and checks the exact committed
  value/revision. Missing prior keys are restored with DELETE. A newer revision,
  failed inverse, uncertain receipt, or failed physical restoration retains
  custody. Physical radio state cannot be part of the storage transaction.
- Contexts explicit detection/source/timing controls submit copied values.
  Models and rules stay in app-data. The automatic foreground rule/workflow
  evaluator is an explicit activation gate: it is not called or consumed in the
  experimental profile. This profile is not ready for product selection until
  those separately identified semantics are qualified.

Every accepted operation is collected to a checked terminal receipt. Exit
requests cancel USER work and still collect, including a committed cancellation
race. Compensation for an already committed edit must finish even during exit.
Provider/receipt/release uncertainty retains both token and grant; there is no
legacy fallback, automatic stale retry, rebase or merge policy.

`python scripts/test_utility_atomic.py --system-apps SYSTEM --runtime RUNTIME
--output OUTPUT [--sanitize]` runs actual Battery broadcast, BLE, and Contexts
controller source with a copied asynchronous transport double. The default BLE
fixtures remain unchanged in behavior and pass normal/sanitized. These are
controller/source tests, not SD durability or hardware qualification.
