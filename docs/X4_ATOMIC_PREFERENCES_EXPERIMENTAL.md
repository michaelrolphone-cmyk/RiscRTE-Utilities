# Explicit Utilities preference writers, experimental X4 only

The opt-in `PORTABLE_UTILITIES_ATOMIC_PREFERENCES` flag also requires
`PORTABLE_UTILITIES_TARGET_X4`. No normal builder, package catalog, Watch input,
product source selection, firmware or hardware is changed.

The source is isolated from the exact .99 Utilities commit
`5c91c3517aa9f992e24fc453855701ec5a992355`, preserving its Hollow Trail source.
The separate OBD working tree at `52422df` is untouched; ancestry alone is not a
claim about its source differences. Only the identified settings writers change.

The explicit published System input is
`472036f0c52e8fc88f0ccefb262e25734e6eda39`, tree
`8fcb0e9c0216d2c16db5115da975e4f5f4132b95`. Tests can also consume the explicitly
listed local equivalent `758df40e72a265ea11cab187ef6410045b8ca26f` only when its
whole tree matches and tracked files are clean. Receipts distinguish the public
pin from the actual checkout. The earlier standalone qualification against
`54b3061766e2bbc44e91512f5a8646a8777797ec` is preserved separately. A dependency
and preprocessing check bounds the Home additions and the explicit two-file
Battery result-hook delta; unrelated System code is not activated. Its unmodified USER envelope is PFI0001 (176 bytes),
with copied expected/desired values, captured namespace-1 revision, and native
atomic submit/collect/cancel custody. Utilities does not redefine legacy puts.

- Battery pauses the broadcast client before submitting the explicit toggle.
  Its usual policy reload reads the same Core. Missing preferences remain virtual.
  After terminal collection, the opt-in `PORTABLE_BROADCAST_RESULT_ONLY` System
  hook applies the checked save outcome without any KV write. Failed Disable
  remains paused through the original `save_failed` latch until explicit retry;
  successful retry clears that latch. Receipt success is never synthesized.
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
  Models and rules stay in app-data. The automatic foreground evaluator keeps
  its existing change mask, ascending field order, low-battery exclusions,
  `0x274` applicability mask, partial reports and workflow ordering. Its private
  encoder view emits PFA0001 AUTOMATIC with boundary 4 (foreground Contexts).
  A fixed initial revision advances only through its own collected successes;
  another writer causes stale refusal, never automatic rebase/replay. There is
  no general legacy-put-to-intent route. Existing absent/default and no-op
  behavior is preserved.

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


`python scripts/qualify_utility_atomic.py --target x4 --system-apps SYSTEM
--runtime RUNTIME --output OUTPUT --xtensa-cc COMPILER` builds the three opted-in
ELFs with the source-owned loader/import/export gates and compares each flag-off
ELF byte-for-byte with the frozen Utilities baseline under identical inputs.
The build fails when the X4 opt-in marker is missing. It is single-job and local.

`python scripts/test_native_broadcast.py --atomic-preferences --system-apps
SYSTEM --runtime RUNTIME --drivers DRIVERS --xtensa-cc COMPILER` additionally
runs the actual Battery app_main, shared adapter and broadcast service together,
including rapid opposite toggles, error/retry, rotated input and native custody.
The atomic touch fixture emits actual ordered edges and matching snapshots for
the current shared reducer. A checked receipt updates the local toggle before
the advertiser's next policy reload, preserving rapid opposite actions.


Published-source reproducibility: `scripts/verify_utility_system_pin.py` takes
`--previous-system`, `--system-apps`, `--runtime`, `--xtensa-cc`, and `--output`.
It records the compiler's actual dependency closure and requires all default
and non-Battery selected profiles to preprocess identically under the previous
and published System inputs, normalizing checkout paths only. Atomic Battery
admits the requested result-hook delta; the whole System tree must differ from
the prior published tree in exactly the two broadcast API/adapter files. The
normal target qualification independently checks emitted ELF loader validity,
imports/exports, and exact default/base bytes. A different SHA without the
explicit exact-tree mapping is rejected.
