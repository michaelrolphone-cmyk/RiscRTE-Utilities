# Awake Contexts service 0.1.1 (development)

`contexts.service@1` is an ordinary Utilities provider, built independently of
the frozen Watch 1.0.12 and 1.0.13 cohorts. It reuses the actual Audio Spectrum and RF
signature decoders, canonical FFTs, room trackers, foreground subtraction and
single-frame event matchers. It never creates saved samples or trains a model.

This increment imports saved temporal example banks and validated neural
checkpoints through their original owners. Temporal recognition retains every
positive and negative sequence and the existing bounded DTW gates. Neural
refinement can resolve only eligible ambiguous/negative temporal candidates;
missing or stale checkpoints leave temporal inference intact. Frequency-label
catalogs remain outside this import contract.

The coordinated source allocation is Contexts/service **0.1.1**, Audio Spectrum
**0.4.12** and Waterfall **0.2.8**. Waterfall 0.2.7 belongs to the independent X4
cohort. These allocations were checked against live Utilities main, tags and
open PRs on 2026-10-08. No new Watch product version is assigned here.

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
   KV before acquiring its existing private AppData2 (audio) or AppData3 (RF).
   Two canonical temporal banks and the optional neural checkpoint are copied
   from fresh stat/read revisions using the owner's existing readback buffer.
   A successful read must return exactly the requested revision and size.
   Missing KV records produce valid empty
   signatures or the source app's existing virtual preference default; no
   persistent record is written.
4. Signature readiness requires every required signature record to validate. Bad records,
   failed reads, missing grants and incomplete exports fail closed. Pending
   requests are cleared on failure, and the source never schedules its own
   retry. A retained grant or failed cleanup keeps the invocation retained.
   Once the queue settles, a failed source does not block the other ready source.
   Temporal readiness independently requires both banks to be accounted for
   and valid; confirmed absence is an empty bank, while partial/stale/unread
   exports are rejected. Neural readiness additionally binds the complete bank
   CRCs/generations, exact eligibility and held-example masks, dimensions,
   checked promotion metadata, finite weights and RF receiver identity.
5. Normal foreground app launches retain their existing behavior when no
   request is pending. On ordinary editor exit, an already-requested source is
   refreshed from saved records after the app releases its original grants.
   This incorporates saved edits/deletions without keeping an app pointer.

The service copies decoded signatures into boot-session memory. A successful
identical export preserves model generation; a changed canonical collection
advances it. Independent byte fingerprints avoid the fixed CRC residue obtained
by hashing a complete record including its own CRC. No model survives a reboot
except through its original owner.

AppData RETAINED and failed AppData release invoke the existing Runtime terminal
invocation fence, then return directly from the owner app. No later service
finish/status, storage operation, release, diagnostic or fini occurs in that
invocation. An abandoned export is resolved by the next launcher invocation.
Without the Runtime fence suffix, optional AppData import is reported unsupported
before any read. The ordinary RF editor uses the same terminal return discipline;
its older-Runtime fallback only retains its stack and yields, without another
background-service call.

## Optional API and inference state

The API1 prefix through `capture_audio` and the entire copied status layout stay
unchanged. `CONTEXTS_SERVICE_V1_SIZE` is the required prefix; new owners and UI
size-check the appended `export_model_error` and `model_details` methods. An old
provider continues to support signatures and never receives AppData reads or
new record kinds from a new owner. Older clients can use the new provider.

Temporal event slots are a separate label collection. Event names are copied
from that collection directly, never looked up through the signature/room
catalog. Temporal/neural replacement has its own generation and cannot create a
new room-preset entry. Model details distinguish missing, stale and failed
imports; positive/negative counts and matching work are diagnostics.

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

Audio temporal columns average two genuine canonical FFT windows into the
existing 64 ms representation. RF columns preserve actual timestamps and mark
every independent burst's unobserved interval; identity changes, timestamp
regression/wrap, pause or capture loss reset partial inference. No dropped
interval is interpolated. With a nonempty temporal library, event publication
uses its temporal/neural result and explicit unknown/ambiguity. Results expire
from the observed event window's end, not from a later matching completion.

Capture-only checkpoints form feature windows but never run DTW or neural
prediction. Full `step` alternates eight-unit matching slices, permits at most
128 work units, checks a 4 ms matching budget and drains already-owned audio
between slices. A fixed-size coarse comparison remains atomic; physical timing
still requires qualification. The finite 512-frame fixture injects matching
cost and verifies capture-only calls never advance matching work.

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
python scripts/test_contexts_rf_retained.py --system-apps /path/system-apps
NATIVE_APP_CC=/path/xtensa-esp32s3-elf-gcc python scripts/build_contexts_service.py --drivers /path/drivers --runtime /path/runtime --system-apps /path/system-apps
```

The owner app build opts into `PORTABLE_CONTEXTS_CLIENT`, includes
`lib/Contexts/include`, and uses the shared adapter's borrowed
`portable_contexts_service()` plus `PortableBackgroundServices.h`. The service
build uses one driver SDK include family to avoid mixing separately copied
`#pragma once` provider declarations.

The provider has bounded static model/DSP/workspace memory and no heap. The
GCC 8.4 target build records ELF identity, imports/exports, section sizes and
source provenance. Draft 0.1.1 target BSS is 327,752 bytes, including 283,832
bytes of temporal inference state. It keeps one canonical inference library per
source and active neural weights, excluding editor transaction snapshots,
recording buffers, candidate trainers and validation state. Owner export reuses
existing readback buffers and adds no heap allocation. Full cohort peak memory, cadence, battery cost,
microphone/RF behavior and physical Watch qualification remain deployment work.
Host fixtures use real production DSP on generated PCM/IQ and inject storage,
format, staleness and cleanup failures; they do not qualify hardware.

## Validation stack study

Temporal validation recomputes derived metadata from the immutable frame sequence
using a 22-byte audio summary or a 28-byte RF summary. The same calculation backs
normal summarization. It no longer makes a full automatic example copy. Example,
frame, library and saved-record layouts are unchanged; no heap or persistent
scratch was introduced. Differential tests retain the prior calculation and
check acceptance, summaries, mutation rejection and encoded records.

The isolated comparison uses the exact Watch `fa4729f` target recipe, including
its five-app LTO policy, with only the two temporal headers changed. Separate
`-fstack-usage -fdump-ipa-cgraph` builds reproduce the normal compacted ELF bytes.
Fresh baseline builds also reproduce all six original packaged ELFs. These
analysis flags are not part of the delivery recipe.

| Compiler-visible chain | Before | After |
| --- | ---: | ---: |
| Audio example validation | 2,592 | 128 |
| RF example validation | 2,976 | 144 |
| Service model import | 6,288 | 3,600 |
| Service full step | 4,944 | 2,512 |
| Service capture-only entry | 4,464 | 2,032 |

The same conservative foreground/editor/render-callback calculation, including
816 bytes of exact native caller frames and an arithmetic-helper allowance,
falls from 15,680 to 10,688 bytes at the largest RF path. The configured executor
stack is 16,384 bytes. Native capability/provider, libc, interrupt and unresolved
indirect-call frames remain excluded; this is not a physical stack high-water or
complete target stack-safety qualification.

Compacted file size grows by 192 bytes for the service, 40 for Audio Spectrum and
20 for Waterfall. Contexts and both Clock files stay byte-identical. BSS remains
unchanged in all six components, including the service's 327,752 bytes. This is
an isolated component comparison, not a rebuilt Watch store or release image.

The model/client contract is tested locally against System Apps checkpoint
4440e5f5f65846d37e60c3b0c8b799baec103f1c. Its public equivalent must replace the
older Contexts workflow pin before publishing this slice. Frozen Watch stores,
images, source revisions and historical native inputs are not rewritten.
