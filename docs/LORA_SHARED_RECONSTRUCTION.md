# LoRa shared reconstruction — work in progress

Original task cf71e334-ff2c-4272-b1f5-c83d9e651960 retains its eight-point estimate and six original acceptance clauses. The prior unpublished session/lifecycle implementation was lost and must be redone; historical test summaries do not qualify this source.

## Source custody

Utilities baseline: bf1117910f6d0f0e9fd48bc2a28bc03b1f8ad61e.
Published LoRa model, raw radio ABI, 32-byte profile encoding, explicit RF policy, 16-packet session history and 240-byte ASCII draft remain reused.
The new session fences every radio callback against authority loss.
The new presenter describes semantic nodes only; providers own physical geometry and pagination.

The generic alarm and audio-background helpers are exact unmodified source from Scanner PR114 commit f057c6cf5b424d2973408c0824412a947b6fe95d, with source-owner coordination. Their existing names remain to avoid an unqualified shared rename. Their generic published service contracts are reused, not their Scanner app/controller. LoRa background policy must be freshly qualified: context audio may proceed, automatic RF/broadcast stays paused, owners settle before output/storage/focus boundaries.

The controller reuses published resident attention/idle/alarm protocol patterns. It preserves explicit-only transmission, cancellation uncertainty, user radio choice, existing profile bytes and namespace9. Namespace1 is read-only. Draft/history remain invocation-local as in the baseline; no new persistent schema or silent migration is introduced. Civil time is obtained through time.civil, so provider deployment owns timezone/physical clock differences.

## Qualification status

New source and tests are checkpointed, not yet qualified. Required remaining evidence: focused normal/sanitized tests; actual controller and scheduler interruption/retention cases; actual Runtime graph/absence and custody; repeat target ELF and byte-identical X4/T-Watch/Waveshare selections; product binding/catalog/publication reconciliation and physical-status disclosure. A source-only compile is not a behavior pass.

No claim of preserved complete behavior or delivered product is made before those stages pass.
