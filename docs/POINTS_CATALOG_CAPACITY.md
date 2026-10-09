# Large catalog scheduling and sleep

The selected alarm-service 0.4.7 (Watch raw RTC) and 0.4.8 (native UTC) variants avoid two full encoded catalog comparison buffers and transfer the read-only decoded catalog into staging. Equality still compares every canonical wire value, including stable IDs and symbols; padding has no meaning. The copied face projection and its 408-byte enclosing retained Home record are unchanged.

A deterministic production-service reproducer advanced monotonic and RTC time by 250 ms after each evaluation. The prior service never granted sleep after 181 scans because every scan invalidated its own 100-ms sample. The successor reads and validates the RTC again after deadline scanning. It returns a fresh sleep decision only if the earliest unhandled deadline remains in the future. Crossing a deadline resumes reconciliation. Backwards clocks, implausible clock disagreement, RTC read errors and native context loss retain their refusal/fencing behavior.

The provider uses the same serialized reconciliation and atomic occurrence ledger. It never writes the legacy event records or frees storage buffers after terminal retention.

## Qualification

- `scripts/test_points_catalog.py`: canonical encoding equality oracle, stable IDs, migration, allocation failures, file transactions and terminal read custody, normal and ASan/UBSan.
- `scripts/test_points_catalog_service.py`: both production scheduler domains, normal and ASan/UBSan, including eight terminal storage cases.
- `scripts/test_points_catalog_latency.py`: 24 actual-service cases for slow scans, fresh tickets, a deadline crossing, backward/implausible clocks, read failure and context loss.
- `scripts/probe_points_catalog_capacity.py`: actual Runtime AppDataFiles backend at 8, 100, 1000, 2000 and 2022 events, cold/warm/restart/sleep/outage and constrained allocation.
- `scripts/build_points_catalog_service.py`: pinned GCC 8.4 service ELFs, every relocation under eight alignments, and both unselected legacy service ELFs byte-identical to their original baseline.

At 2022 events and 12 types, the 65520-byte catalog uses 154296 requested resident bytes and a 308728-byte peak across the catalog and actual backend buffers. The prior values were 211536 and 432168 bytes. A 256-KiB artificial combined budget still fails safely without alerts or leaked allocations. Memory figures exclude allocator metadata, stacks, ELF mappings and other applications. CPU timing is host-only; physical target performance and remaining uninterruptible scan latency are not claimed to be qualified.

Watch1.0.22 remains frozen on the earlier service. This successor is for an explicitly rebound later cohort.
