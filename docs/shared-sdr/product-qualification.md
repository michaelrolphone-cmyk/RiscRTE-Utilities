# Shared SDR offline product qualification

The common manifest preserves Waterfall / solid:f0ec presentation and declares
foreground lifecycle. The selected Runtime normalizes a missing title to the ID,
drops missing icons, and treats missing dynamic lifecycle as legacy. A foreground
manifest does not enable resident policy on Watch/Wave; the shared controller
selects local lifecycle when the Runtime cleanly refuses the resident interface.

Run after build_shared_sdr.py, with exact clean sources:

    python3 scripts/test_shared_sdr_products.py
    python3 scripts/assemble_shared_sdr_products.py --system System --bootfs Bootfs --baseline out/baseline --baseline-lock docs/shared-sdr/baseline.json --shared out/shared --output out/candidate
    python3 scripts/qualify_shared_sdr_products.py --runtime Runtime --system System --bootfs Bootfs --baseline out/baseline --baseline-lock docs/shared-sdr/baseline.json --shared out/shared --candidate out/candidate --output out/qualification

Source pins: System 710b200021b983ea55eb86a6fbddbbf5ce10a984;
Bootfs ab9ff33975680c5bb8d9a2e7ed346d0300732a4e;
Runtime 712ecc3f894dba99d9c9638941778a82f5856b97.
All six published baseline asset lengths and SHA256s are checked.

Except for the audited raw-alarm-to-facade migration below, existing grant rows
remain unchanged and ordered. Three uniquely verified instance-0 grants are
appended: canvas raster API1, canvas input API1 and alarm facade API2. Installed providers, native policy, board, resident shell and peers
are not changed. Existing peer storage envelopes and partition sizes remain exact.

Wave's selected SD manifest and ELF are replaced together. X4's content-addressed
successor directory contains the identical common manifest and ELF; both old
members remain intact. Its boot catalog-authority reservation selects that same
new directory and retains every non-migrated grant. The native fixture invokes the
actual selected Runtime installationAdmit with the real successor manifest,
checks inherited authority, title/icon and foreground behavior, and rejects a
wrong directory and an undeclared required capability. The host-only hook selects
an inert invocation context; no target code or physical provider runs.

Qualification includes production SPIFFS builder/independent parser roundtrip,
SD ZIP roundtrip, full candidate cohort/ELF admission and standalone shared ELF
validation, normally and with ASan/UBSan. Leak detection is disabled as in the
owning fixtures. Failures are retained and never converted into skips/passes.

Limits: this selects current Runtime for every lane, including X4; the previous
Home runner used X4 Runtime 0052d1fc42e9cfc82d626c5a247d460919c87935.
It is not predecessor qualification. Fixture capacities are inherited assumptions:
Watch/Wave provider rows 48, X4 requirement rows 32. Native linked capacity proof,
physical memory fit, SD discovery/execution, application behavior, migration and
deployment remain separate checks. No publication or device operation occurs.

## Audited alarm facade migration
The shared controller uses only `alarm.service.facade@2`, instance 0. Both unused raw `alarm.service` declarations are removed. For Waterfall alone, assembly removes exactly one legacy raw alarm grant (API1 or API2, instance0) and grants the existing facade. Its unchanged backend binding must exactly match the removed grant. All remaining grants retain order and content; every other app, provider binding, and stored peer stays unchanged. Assembly and independent qualification receipts record the removed grant, added facade, backend, provider and selections. Unexpected raw authority fails closed. The original callback/context, occurrence token, sleep ticket, resume, refusal and custody tests remain intact. Runtime stays pinned to 712ecc3; no duplicate-capability semantics are broadened.
