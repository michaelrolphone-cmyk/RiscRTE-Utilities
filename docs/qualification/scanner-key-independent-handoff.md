# Scanner manual-key and idle preservation repair

This successor repairs the original shared Scanner's automatic handoff paths.
It does not change the BSv1 alias format, sensor service protocol, shared
fingerprint/rule data, Bluetooth preference transaction, or one-ELF selection.

## Ownership and behavior

Automatic main/editor paths no longer use ordinary resident POLL/POLICY, which
could consume a physical key arriving after the app's copied attention sample.
Short Back and ignored long/combined input remain app-owned. Explicit Controls
remain explicit. Configured idle uses the published key-independent idle
contract after checked sensor/background/UI settlement; scan updates are not
user activity. A suspended scan remains stopped until the user requests Scan.

The new generic alarm-only contract is System PR178, commit
eb4dd5306906d57e65f24f911d325bc87370666b. It requires settled custody and does not
change the Runtime ABI or provide a capture-safe observation operation.
Unsupported hosts do not trigger an ordinary-POLL fallback; the existing
checked alarm-service path remains available.

Home PR20, commit ba88f5514b5bb8ed6e176c00b99e73aa055971a1, implements alarm-only
handling and a copied last-idle-alarm outcome. An ordinary idle redraw resumes
the paused editor and draft. Actual alarm/cue/error cancellation, including
late final reconciliation, cancels the editor without saving or fabricating
a Home exit. BUSY retries do not replay idle application. Close-AGAIN remains
owned even when the editor was fully paused. Genuine EXIT/HANDOFF retains its
separate exit semantics.

Healthy fallback alarm-client cleanup releases its grant without invoking the
failure-only stop fence. Owned/BLOCKED cleanup remains checked; unknown or
uncertain outcomes retain custody and prohibit later work.

## Selected products

The assembler requires both first and repeat Home packages for Watch and Wave.
It validates exact existing dependency locks, selected automatic/joint profiles,
repeated input maps and ELF bytes, and manifest equality except version.
Watch selects Home0.25.1 and Wave0.25.2. X4 Home bytes and authority remain
unchanged. Existing scene/provider profiles, RTC selection, app routes, unrelated
payloads and all other grants remain preserved.

The canonical Scanner0.3.2 target is compiled once and copied byte-for-byte into
all three selections. Product-store decode and actual native metadata admission
are separate from real firmware or physical execution.

## Qualification boundaries

Focused root tests exercise actual main-loop cancellation, not only direct
helper calls. Production Home tests include late-prime alarm, failed-prime and
completed-CUE outcomes. Actual Runtime transport tests use deterministic host
callbacks; they are not production Home execution. Scanner Runtime fixtures
exercise real app/scene/text with explicit physical sensor/alarm/KV doubles
against both current and retained providers.

Owning workflows retain exact source archives, compiler dependencies, commands,
binaries and target repeat receipts. Local and hosted result receipts identify
their exact source; historical pre-repair successes are not final proof.
Physical testing and full native firmware pairing are disclosed as not performed,
not added as new software acceptance gates.
