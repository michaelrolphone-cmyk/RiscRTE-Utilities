# Storage-scaled Points catalog

This implementation is in progress. It is not selected by a delivered product.

The editor and alarm service will share a variable-length `points.catalog`
document through app-data storage. The document contains sorted, stable event
and type IDs, per-event scheduling revisions and creation boundaries, a catalog
revision, and an explicit raw-RTC or native-UTC time domain. There is no fixed
event or custom-type count. The selected filesystem's byte capacity and available
memory constrain a save; an unsuccessful save must leave the prior document
intact. This format does not select an SD card or change storage backends.

An event references a distinct type record. A custom type has a name, color,
duration capability, default duration, default alert mode and end/warning
defaults. Selecting it copies its defaults into a new or explicitly changed
event. Renaming a type changes its label without silently rescheduling all its
events. In-use types cannot be deleted or made incompatible with saved events.

Migration retains every occupied legacy slot's identity, schedule, weekday mask,
duration, alert options and creation/revision boundary. Slot holes stay holes in
the initial ID assignment so durable legacy occurrences can be matched safely.
Both existing custom-type names and colors survive. Legacy storage remains a
read-only migration source; the final integration must not overwrite it.

The proposed provider storage contract is `storage.app-data.bound@1`, with
exact filename, namespace and read/read-write policy. The alarm provider gets
read access to `points.catalog` and read-write access to `points.ledger`, while
the editor gets its namespace-bound app-data grant. Backend revision checks,
atomic replacement, uncertain commits and terminal retention remain mandatory.
The Runtime contains no Points schema or migration policy.

Faces will consume a bounded upcoming-events projection, independently of the
catalog size. The final service must preserve existing one-shot alarm and timer
arbitration, missed-occurrence handling, sleep deadlines and output cleanup.

Completed model validation: 1,508 entries and 48 types serialize below the
current Watch backend's file-size bound; stable-ID deletion and insertion,
legacy migration, custom-type behavior, corrupt input, integer exhaustion and
allocation refusal pass normally and under ASan/UBSan in both time domains.
Run `python3 scripts/test_points_catalog.py`.

Remaining integration: persistent writer and ledger migration, actual alarm
service scheduling, bounded face snapshot, visible weekday/type editors on
Watch and X4, selected target builds and actual Runtime lifecycle tests.

## Terminal read-buffer custody

A catalog or occurrence-ledger backend read returning RETAINED or CONTEXT keeps its supplied allocation reachable in the storage state. No allocator call, further file call or storage-state disposal is allowed after that result. The provider's verification phase also avoids freeing even an empty temporary ledger after the terminal fence. A fresh invocation is a host lifecycle operation; application code cannot clear this state to retry.

The writer regression instruments the allocator and covers catalog/ledger load and ambiguous-write resolution for both terminal statuses, plus ordinary I/O errors. The real provider regression instruments allocations and executes all eight terminal paths in both Watch and native-UTC profiles, normally and under ASan/UBSan. Before the final provider correction, its retained ledger-read case reproduced an allocator call after the backend fence.
