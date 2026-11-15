# Explicit native clock editor protocol (software only)

Selected only by ALARM_SERVICE_CLOCK_EDIT_V2 / --clock-edit-authority, profile
native-utc-schedules-clock-edit. Historical native schedules and Watch/Points
profiles are unchanged when omitted. API2 retains its base offsets and adds an
explicit feature plus separately tagged/versioned tail from AlarmClockEditV2.h.
The current SDK anchor is separately pinned by the product source receipt.

## Authority and timing

The service requires storage.key-value.bound API2, the existing same-layout
2048-byte bounded-table contract. API1 only supports64 bytes and is NOT silently
used for the316-byte ledger. Native NVS advertises2048; tests model the same
capacity while the real Runtime enforces it. The seventh exact bound key is
clock_edit_auth, namespace4 read/write. No app receives namespace4 through this
addition. The tail is visible to every alarm.service API2 grant holder: Settings
alone dispatches it in the selected trusted product, NOT a separate security
principal or per-method access restriction.

begin checks settled service/no active occurrence, dismiss, persistence or output
uncertainty; fresh native UTC; exact current config/occurrence identities. A
changed writer identity returns STALE and starts fresh reconciliation. It
invalidates any prior sleep ticket, durably writes/readbacks INTENT, then returns
one copied permit. Generation advances durably; the exact private permit must
match, including size/reserved fields. Ordinary step/refresh/ack/sleep/stop-only
returns BUSY while the live permit is owned. Status remains memory-only.

The editor then explicitly seeds and verifies native time. finish consumes the
permit before callbacks, checks UTC in[requested,requested+2] seconds and a
maximum5-second same-boot permit age, persists VERIFIED replacement authorization,
reanchors only this explicit operation and starts ordinary reconciliation.
No schedule/configuration or occurrence record is rewritten by these methods.
The normal service may subsequently expire or deliver a due occurrence according
to its existing UTC/recovery-window rules; ACK identity prevents duplicates.

cancel also consumes the permit before callbacks. It restores the exact prior
authorization snapshot (with advanced generation/checksum) only after independently
confirming the previous clock plus bounded same-boot elapsed time. A typed clock
mismatch/invalidity preserves INTENT and enters BLOCKED RTC recovery; this is NOT
a successful cancel. Selected Settings invokes cancel only before a seed attempt.
Unknown/context provider outcomes or unconfirmed persistence are retained: no
subsequent provider call, restoration or successful quiescence is allowed.

## Crash boundary

Clock and KV are not atomic. INTENT is authorized intention, never verified Save.
A fresh service loading INTENT enters BLOCKED RTC before occurrence generation,
with edit_status.phase=INTENT and copied label CLOCK EDIT PENDING. It does NOT
silently use the union of old/new time ranges to deliver schedules. A fresh
explicit Save can recover a typed RTC-only block, including pending edit or an
unannounced jump already exposed by normal reconciliation; other errors and
output/custody uncertainty remain guarded. Ordinary refresh/step never invoke
this editor authorization automatically. Old in-memory permits cannot finish/cancel after reset. The owner
can use Back on the non-occurrence error modal and explicitly set time again.
A committed VERIFIED record survives fresh service/Deep and permits matching
existing configurations to be evaluated below their old creation timestamp down
to the newly authorized floor. Cold/default time below that floor still fails.
Awake unexpected jumps retain the independent monotonic-vs-UTC guard.

## ACE2 persistent schema

Exactly316 bytes; unsigned little-endian32-bit values; no native structure dump.

| Offset | Bytes | Meaning |
|---|---:|---|
|0|4|ASCII ACE2|
|4|4|Schema1|
|8|4|NONE0, INTENT1, VERIFIED2|
|12|4|Nonzero durable generation|
|16,20|4 each|Requested and previous UTC seconds since2000|
|24,28,32|4 each|Previous phase/requested/previous for intent context|
|36|4|Reserved zero|
|40|136|Two new authorization rows|
|176|136|Two previous authorization rows (intent context only)|
|312|4|FNV-1a checksum of bytes0..311|

Each68-byte row contains exact32-byte schedule config bytes, exact32-byte
occurrence baseline bytes (zero if absent), and an authorized UTC floor. Config
identity must match exactly; a newer/replaced config cannot inherit authorization.
Occurrence generation may advance normally, but cannot fall below the baseline;
equal-generation completed occurrence bytes must remain exact. A pending baseline
may advance only to the same-identity ACK/EXPIRED state, with immutable
kind/revision/deadline/recovery/mode and a one-way silenced marker. This preserves
a legitimately completed owed occurrence without authorizing arbitrary mutation. No ACK/deadline/revision is
silently deleted or moved. VERIFIED floor is min(config.created, requestedUTC),
replacing rather than accumulating the lowest historical edit. NONE and unused
rows are canonical zero. All same-generation reload bytes must match the current
record exactly, preventing stale INTENT replacing an in-memory VERIFIED record.
The live permit owns an exact prior-record RAM copy for checked cancel; resets
remove that copy and instead require explicit recovery.

## Evidence boundaries

Owning tests cover forward/backward multifield dates, future/due/dismissed
schedules, unchanged config/ACK identity, new-config refusal, unannounced jumps,
all316 single-byte ledger mutations, all32 permit-byte mutations, equal-generation
rollback, nine before-write/after-write/readback cuts across begin/finish/cancel,
reset after seed, pending recovery, checked cancel, typed IO+verified readback,
unknown outcomes and no callbacks after retention. The actual Runtime fixture
admits seven keys via boundKV2 and persists/readbacks316 bytes, with normal and
sanitized variants. SDK/native hardware/storage boundaries remain modeled; no
physical RTC accuracy, power-loss atomicity, sleep current or device validation
is claimed.

## Selected target compiler qualification

The first target attempt accidentally introduced a64-bit division helper into
bounded elapsed-time arithmetic. The pinned Xtensa linker aborted while pulling
that libgcc helper. The corrected code narrows only AFTER proving the5-second
orUINT32_MAX bound, then divides the32-bit duration as the historical service
does. It retains -Os and the original link recipe; no unresolved helper import,
linker-check bypass or optimization workaround is accepted. Failed attempts are
retained separately. The final target must pass structural/import/PSRAM and
actual loader validation at all eight alignment placements.
