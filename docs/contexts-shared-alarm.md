# Common Contexts alarm contract

The common Contexts manifest declares optional `alarm.service.facade@2` and
`alarm.service@1`, using exact integer API versions accepted by Runtime.
The earlier `">=1"` manifest value was invalid in actual product admission;
host renderer and target ELF checks alone did not catch this packaging defect.

Contexts first acquires the distinct qualified facade capability at API2.
Only clean absence permits fallback to the legacy alarm.service API1.
Dirty/partial acquisition, malformed descriptors, revoked liveness and retained
custody remain terminal, never reasons to fall back. Both capabilities absent
leaves alarm-dependent functions unavailable. The app does not infer a device.

The facade header is reused unchanged from Utilities PR121 source
452c57bd4f59f3c1d1463763e6ea5b0f90e7d012. The facade forwards the selected existing
scheduler owner and adds no scheduler, RTC, storage or output ownership.
Cumulative product Contexts policy rows explicitly grant the facade API2.
An old Contexts-only alarm.service API2 grant must be replaced by that exact
facade grant; other apps' original alarm grants and owners remain unchanged.
Legacy API1-only deployments can retain their original grant.

Runtime rejects duplicate non-KV declarations even with different API values,
and matches declaration/grant versions exactly. Neither two alarm.service rows
nor a version-range string is a valid alternative. The generic builder rejects
non-integer APIs and duplicates before publishing; qualification covers facade,
legacy-only, both absent and uncertain-acquisition paths normally and sanitized.
Final product admission is checked by Home composition against actual Runtime,
separately from common ELF/source qualification. Prior public releases are
immutable and are not overwritten by this correction.
