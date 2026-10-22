# Utilities controls-power consolidation

Current-main replacement for #88, #90, #91, #94, #96, #98.

All original heads were read back on 2026-10-22 at 17:11 UTC. Exact base, source commit, blob and mode inventories are in the adjacent JSON file. Only the selected explicit base/head deltas were transplanted; no historical product tree or frozen default hashes were substituted. Already-public power evidence removed by the charging stack is retained. Original PRs remain intact until replacement validation is complete.

This is source consolidation, not a release, installation, product pin change or hardware qualification. Full exact-head integration qualification is pending.

## Integration corrections

- The current T039 source and 0.1.31 candidate are qualified from the exact replacement head. Their admissions now validate the complete approved composition by blob and mode, instead of assuming no other work exists in the repository.
- The 0.1.30-dev1 frozen release recipe still runs unchanged against literal historical source `6b2047500905f8f45087cb49989103a926fc5c63`. The original workflow is retained byte-for-byte in `frozen-native-ble-buttons.yml`. The current controller's release-byte refusal remains a mandatory tested refusal in the current-head T039 suite. No historical failure is relabeled a release pass, no immutable hash is updated, and the historical workflow cannot publish an artifact under a different current source identity.
- Both candidate suites run on every PR and on main, including subsequent composed changes. Their full runtime, sanitizer, structural target, parser, package and transported-artifact checks remain mandatory. Original PR94 evidence remains independently verifiable using its original schema and exact source pin.
