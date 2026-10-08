# Awake Contexts service (development)

`contexts.service@1` is an ordinary Utilities provider, built independently of
the frozen Watch 1.0.12 cohort. It reuses the actual Audio Spectrum and RF
signature decoders, canonical FFTs, room trackers, foreground subtraction and
single-frame event matchers. It never creates saved samples or trains a model.

This increment recognizes saved signature rooms and events. Temporal example
banks and neural checkpoints remain private to their existing owning apps and
are **not imported**. Status reports `temporal_ready=false` and
`neural_ready=false`. Their record kinds are reserved and rejected, not silently
treated as signature records. Frequency-label catalogs are also not imported.

## Source and authority boundaries

The service depends only on `platform.clock@1`, `audio.input@1`, and the existing
configured-burst extension of `radio.iq@1`. It has no filesystem, app-data, KV,
settings, display or alarm authority. Shared System Apps owns the cooperative
client and settings application; Watch owns startup rendezvous and faces. One
Context capability grant is added per participating app. App-owned presets and
manual controls stay outside this provider.

Runtime 0.1.55 rejects app-data namespace sharing in `Runtime.cpp`. Its
`CohortRuntime.inc` additionally rejects a newly introduced provider binding an
already-used KV namespace, even with read-only access. A provider also has only
nine bound keys, each at most 2,048 bytes. Neither API availability nor a
duplicated policy number grants model ownership. This implementation requires
no exception to those checks and no duplicate model storage in the 24 KiB NVS
partition.

## Cold start and model refresh

1. When monitoring is enabled, Clock requests the desired source exports once.
2. Clock begins the source export, then launches Audio Spectrum and/or Waterfall
   through ordinary Runtime app handoff. The owner accepts this existing export
   session. A leftover active export on the next Clock invocation is a failed
   owner launch/init/return and is finished as an error once, never relaunched
   automatically.
3. Before allocating its plot/training state or opening capture, the requested
   owner reads its existing private KV grant. It exports the canonical saved
   preferences and all eight signature records synchronously, then releases
   storage and returns normally to Clock. Missing records produce valid empty
   signatures or the source app's existing virtual preference default; no
   persistent record is written.
4. A source is ready only after every required record validates. Bad records,
   failed reads, missing grants and incomplete exports fail closed. Pending
   requests are cleared on failure, and the source never schedules its own
   retry. A retained grant or failed cleanup keeps the invocation retained.
   Once the queue settles, a failed source does not block the other ready source.
5. Normal foreground app launches retain their existing behavior when no
   request is pending. On ordinary editor exit, an already-requested source is
   refreshed from saved records after the app releases its original grants.
   This incorporates saved edits/deletions without keeping an app pointer.

The service copies decoded signatures into boot-session memory. A successful
identical export preserves model generation; a changed canonical collection
advances it. Independent byte fingerprints avoid the fixed CRC residue obtained
by hashing a complete record including its own CRC. No model survives a reboot
except through its original owner.

## Cooperative lifecycle and observations

The caller supplies a copied enabled/awake/source policy and separate audio/RF
permission flags for each bounded `step`. There is no task, timer, Runtime poll
callback or autonomous capture. Microphone draining accounts 16 frames per
elapsed millisecond and reads only complete owed 256-frame DMA quanta, keeping
the fractional debt for a later call. Native RX has two 256-frame buffers, so
callers must service capture within 16 ms while drawing or servicing input.
The appended `capture_audio` method only drains an already-owned, permitted RX
and updates copied inference state. It never opens capture, accesses RF or
storage, exports models, or applies presets. The full `step` remains the sole
open/configure/selection point.

A drain permits at most two chunks and checks an 8 ms work budget after each
read; an individual existing native read can still take up to its 40 ms bound.
Empty/short reads resynchronize to actual returned input rather than inventing
samples. A canonical audio observation requires a genuine 512-frame window.
The finite 512-frame producer fixture sustains capture checkpoints every 8 ms
with full policy steps 120 ms apart, without overflow or speculative partial
reads. Missing the deadline discards continuity; ordinary 40–120 ms policy
polling alone cannot support continuous audio. Physical DSP and rendering cost
are not inferred from this host result.
RF consumes one genuine configured256-pair burst at most every100ms.
The intervals between RF bursts are unobserved. Complete format and receiver
identity checks precede RF inference.

The common client must pause Contexts before foreground audio/RF, alarm output,
sleep, app handoff, storage retention or incompatible BLE/radio work. Background
monitoring never inhibits idle sleep. Source permissions must reflect manual
radio controls and low-battery policy. The native RF provider still owns the
radio-idle/HCI-idle/SPI/PHY lease proof; `BUSY` is a refusal, not permission for
Contexts to stop someone else's stream. Only its own failed capture cleanup
authorizes an RF suspend retry.

Failed microphone close or RF cleanup returns false. Only cleanup retries and
copied status remain legal until custody is clear; no normal capture or export
continues. Successful pause invalidates live observations and discards partial
signal windows. Resume establishes fresh room evidence. An unpolled gap of 32 ms
or more closes the owned RX and resets observation/DSP state, discarding
overflow-prone queued input. If the bounded drain cannot catch up, status reports
an input error and no current label until it can; background monitoring never
extends an idle deadline. Capture-only calls cannot reopen a closed RX. A failed
close returns false and requires the client to enter its retained invocation
fence before further foreground provider I/O.

Status carries exact source, saved slot, name, generation, confidence, ambiguity,
sample count and age. Room validity excludes held, verifying and ambiguous
tracker states. Observations older than 500 ms are not current; no observation
in the current capture session has age `UINT32_MAX`. Event labels may remain
visible for up to two seconds only while their source observations stay current.
These are matches to user-supplied signatures, not a physical-location claim.

## Preset custody

`claim_preset` verifies a current confirmed source/slot/name/generation and
consumes that source-local room entry **before** the first settings write. The same entry
stays consumed through pause, app switches, unknown/ambiguous observations and
failed/partial writes. A different confirmed identity or model generation
advances `room_entry`, including rooms with no preset; a later return can claim
a new entry. `preset_entry` records the consumed entry. `preset_result` records
applied versus partial once.
Clients must choose a deterministic source when both provide room evidence;
they must not alternate conflicting audio/RF presets on every poll. The client
remains responsible for manual-control and low-battery priority.

## Build and validation

```
python scripts/test_contexts_service.py --drivers /path/drivers --runtime /path/runtime --system-apps /path/system-apps
NATIVE_APP_CC=/path/xtensa-esp32s3-elf-gcc python scripts/build_contexts_service.py --drivers /path/drivers --runtime /path/runtime --system-apps /path/system-apps
```

The owner app build opts into `PORTABLE_CONTEXTS_CLIENT`, includes
`lib/Contexts/include`, and uses the shared adapter's borrowed
`portable_contexts_service()` plus `PortableBackgroundServices.h`. The service
build uses one driver SDK include family to avoid mixing separately copied
`#pragma once` provider declarations.

The provider has bounded static model/DSP/workspace memory and no heap. The
GCC 8.4 target build records ELF identity, imports/exports, section sizes and
source provenance. Initial target BSS is approximately 43 KiB; the final build
evidence gives the exact size. Full cohort peak memory, cadence, battery cost,
microphone/RF behavior and physical Watch qualification remain deployment work.
Host fixtures use real production DSP on generated PCM/IQ and inject storage,
format, staleness and cleanup failures; they do not qualify hardware.
