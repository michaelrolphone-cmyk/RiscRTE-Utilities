# Shared alarm ready-step selection

`--alarm-ready-drain` is an opt-in selection forwarded to the committed System
foreground helper by both Utilities builders. It requires the tagged foreground
alarm profile; an older helper without explicit support is rejected, not ignored.
The helper owns the flags, eight-millisecond between-phase budget and provenance.
The budget cannot preempt one synchronous provider call. Pending work, input,
terminal custody and existing alarm persistence remain authoritative.

Omitting the option preserves the previous selection. Parser/forwarding tests
exercise both modes and unsupported helper rejection. Product admission must
verify actual compiled definitions and the shared helper receipt; these tests
alone are not device performance evidence.
