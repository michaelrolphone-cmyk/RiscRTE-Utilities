# Shared-input historical evolution v1

Historical pins remain RF82d17d0d902d427938af95aae0e976c8f78041ed,
Spectrum65bffc27f96088db3c2f76ee95ae812b223459f7, and native Spectrum
12c7440c4c2fd9b6b78e10f11a983947c145b346. None is advanced.

The JSON contains exact line edits, immutable before/after Git blob hashes, and
a rationale for each file. The reference is reconstructed only from its named
historical checkout, in a temporary directory. The candidate never supplies
reference bytes. A future source change fails full token comparison unless it
preprocesses away or receives an explicit, separately reviewed evolution.
No regeneration command is run in CI.

The reference includes the already-integrated scoped-storage and coordinate
changes in RF's older baseline; it also includes native-capable RF geometry,
bounded228-burst history, fail-closed custody, and portable uint32 formatting.
Native Spectrum needs only the shared-library PRIu32 change for its selector-off
profiles. Native-only Spectrum branches need no exemption.

Current shared-client behavior tests remain intact. Spectrum additionally runs
its actual default-controller keyboard fixture with independently specified
ASCII pages and pixel hits, app_main navigation,16-byte bounds, and no writes
before explicit Save. RF additionally runs existing actual adapter/controller
local-keyboard, cancel, draft guard, history and clean-exit scenarios with no
native-canvas selector. These fixtures supplement, not replace, exact parity.

Git blob SHA-1 is the upstream object identity over the exact file bytes.
The complete migration manifest also receives a SHA-256 digest in each report.
Review the hunks and rationale before accepting any future manifest change.
Host qualification does not establish physical-device qualification.

Coverage: RF local-opt-out-contract explicitly rejects text-entry acquisition, counts submitted local-keyboard frames, checks unchanged KV/app-data writes after Done and Cancel, requires a KV write after Save, fills the228-burst ring and checks strictly chronological timestamps after wrap, then confirms stopped capture/history remain unchanged. It does not prove pixel-by-pixel waterfall equivalence or physical hardware behavior. Default Spectrum uses its existing independent keyboard fixture.

References are extracted from exact Git objects, never copied from a mutable working tree. The archive-only native baseline is resolved from the candidate checkout's full Git history. All three comparators use one maximal-munch token lexer, with lexer-level operator, numeric and literal mutation tests. Source/fixture/helper snapshots are checked for stability before a successful result.

## Native full-feature target reference

The separate `native-target` entry retains historical commit
`12c7440c4c2fd9b6b78e10f11a983947c145b346` and applies only two reviewed
source evolutions: portable PRIu32 formatting and the retained-custody guard
around the model-save failure notification. The latter is enabled by the
full-feature target recipe but absent from the three simpler host token profiles.
Both file transformations require exact historical and resulting Git blob hashes.

Target qualification now builds four modes: untouched historical baseline,
historical sources plus the reviewed evolution, current selector-off, and current
native. Every ELF still receives structural loader admission. Historical versus
current byte equality remains an explicit, truthful Boolean in the receipt.
Current selector-off must equal the reviewed reference ELF byte-for-byte; a
different historical ELF is not labeled identical. No compiler, feature flag,
application logic, baseline pin, or existing host assertion is removed.

Packaging requires all four modes, the exact reviewed definition and source
transformation, truthful historical equality, exact reviewed ELF equality, and
all six geometry/subpage/local-keyboard normal/sanitized records. Synthetic
negative controls exercise missing/duplicate modes and records, failed records,
dishonest historical equality, mutated historical inputs, unreviewed source
changes, and reviewed ELF mismatch even when its digest is updated. Synthetic
packaging controls do not replace execution of the target build.
