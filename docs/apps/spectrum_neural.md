# Spectrum 0.4.5: cautious acoustic learning and independent room tracking

This is a future Utilities increment. It does not change the frozen Watch 1.0.6
source, its dependency pin, firmware images, release catalogs or device data.

## What changes

- Room recognition feeds its existing 32-frame slow average directly from raw
  canonical FFT frames. The event detector's protected floor is independent;
  its 1,024-frame upward hold can no longer hide a different saved room.
- Integer room averaging moves at least one power unit toward a changed band.
  Old low-level bands can decay to zero, and a newly quiet room can be learned.
  The existing transient rejection, 64-frame switching hysteresis, normalized
  shape matching, saved sums and subtraction remain in place.
- Monitor distinguishes unconfirmed `MAYBE` candidates and `AMBIGUOUS` from a
  selected event. A selected result displays one event. Percentages remain
  temporal similarity scores, not neural probabilities or measured accuracy.
- A small on-watch-trained neural candidate can resolve plausible but confused
  temporal events. It stays inactive unless separate recordings demonstrate an
  improvement and confusing nonmatches remain rejected.

## Teaching and fallback

Teach at least two event labels. Each needs three positive recordings and two
confusing `NOT THIS SOUND` recordings, within the existing six-example limit.
Choose genuine similar nonmatches rather than unrelated silence or a distant
pitch. No recording is created, relabeled, replaced or deleted automatically.

For each eligible label, the newest positive and newest negative recording are
held out. Neither is used to train the model or normalize its inputs. The other
examples train a candidate in interruptible one-example idle updates. Recording,
ongoing event matching, touch/navigation, room capture, pending writes, audio
warm-up and foreground sound above the user's event floor take precedence.
Tiny high-SNR bins below that floor do not starve learning. The existing temporal
matcher continues operating throughout preparation, training and checking.

After 256 epochs, the candidate must improve on the simpler temporal matcher on
the held-out examples, with no new wrong labels or rejected correct answers.
Each held-out negative must actually exercise the plausible temporal region;
an easy distant sound alone cannot qualify unknown-result calibration. A second
check uses the complete saved collection, excludes each queried recording from
its own template comparison, and requires improvement without regression again.
This catches apparent held-out gains that disappear after all saved negative
examples participate in live matching.

Only a passing candidate can resolve an ambiguous or confusing-negative result.
At least 80% temporal similarity is still required, and every plausible rival
must be represented in the eligible trained labels. Confident existing temporal
answers and unknown inputs outside that region are left alone. The neural
output must pass both a 0.75 score threshold and a 0.20 lead. Those are bounded
model decision scores, not probabilities. The model can distinguish a taught
positive from a broadly similar negative template only after the negative
training and rejection checks pass. A contradictory or unresolved collection
keeps the simpler matcher active.

`NEURAL READY` means the collection checks passed. `TEST CONFUSING SOUNDS` asks
for useful negative evidence; `NO NEURAL GAIN` means the candidate did not
justify replacing that part of matching. Neither is a storage error. Editing
or reloading the collection invalidates an old model before rebuilding.

## Model and cost

Inputs are 64 average log-band values, eight normalized envelope samples, and
eight duration, attack/decay, impact, peak-phase, frequency and clipped/tonal
values. There are 16 hidden units and eight label-specific outputs, with 1,432
trainable binary32 values. Training-only bounded normalization has another 160
values. The smooth rational activation needs no external math-library imports.

Both active and candidate models, normalization preparation and incremental
validation state fit 16,920 bytes. Each idle call performs at most one training
example, or one 64-unit temporal validation tick. No full epoch runs in one call.
All work and storage are bounded by eight labels, six examples and 64 columns.
These bounds do not establish physical Watch latency, battery cost or accuracy.

## Checkpoint and source-data safety

A validated model is checkpointed once on promotion, not per learning update.
The independent `spectrum-neural.snn` cache is 6,436 bytes, explicitly encoded
little endian, CRC-checked and tied to the canonical contents and generations
of both event banks. Architecture, held-out selection, acceptance results and
finite parameter bounds are validated before loading the active model.

Missing or stale caches retrain from the saved examples. Corrupt/unreadable
caches are not overwritten automatically; learning can still run in memory.
Full, stale or uncertain cache writes leave the authoritative samples unchanged,
do not block ordinary exit, and do not create an automatic retry loop. `RAM ONLY`
marks a cache problem. Explicit Retry can load a successfully committed but
previously uncertain checkpoint without writing it twice. All cache I/O uses
existing microphone-suspended storage paths. A retained provider result stops
further app I/O, rendering and grant release through the existing barrier.

SQT2 event banks, SPSGv1 room/snapshot records, frequency labels, preferences and
private namespaces are unchanged. No cache failure deletes or migrates samples.
Interrupted in-memory candidate training may restart after an app relaunch;
the last valid matching cache can be loaded without retraining.

## Software verification

- The new room-channel fixture spans every selectable FFT and RAW/AUTO/MANUAL,
  held event floors, short bursts, quiet transitions, silence and unchanged
  saved profiles.
- A synthetic canonical-feature fixture improves held-out decisions from 2/4
  to 4/4 and checks confusing/unknown results, insufficient data, interruption,
  deterministic training, corrupt input and unchanged samples.
- A separate actual-PCM fixture runs FFT, background, salience, segmentation,
  temporal matching and learning. Its two similar resonant events improve
  held-out decisions from 2/4 to 4/4, classify all ten stored examples correctly,
  and preserve correct decisions on nine fresh lower-level positive/unknown
  recordings plus an unrelated octave. These are generated signals, not a
  measured physical-acoustic success rate.
- The real app fixture checks cached activation, Monitor selection, editing,
  idle priorities, full/uncertain/corrupt/read failures, exact retry, unchanged
  event-bank bytes, microphone resume and the retained cleanup barrier.
- The model codec rejects all single-byte mutations and CRC-correct hostile
  dimensions, metrics, NaNs, infinities, parameter ranges and stale identities
  without partially changing a live model.

Existing normal/sanitized acoustic, storage, speech, renderer and target checks
remain applicable. Physical room discrimination, microphone behavior, Watch
training time and energy use still require device qualification. This increment
does not add speaker identity, transcription, commands or automatic actions.
