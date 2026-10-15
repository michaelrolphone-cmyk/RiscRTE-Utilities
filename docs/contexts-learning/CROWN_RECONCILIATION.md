# Contexts learning and persistent native exit

Contexts app 0.4.2 reconciles completed learning work from PR58
(`cabbfe191821bb666e9d79c0e5bf79bc36500eb1`) with the reconstructed
persistent native exit in Utilities
`9e8705fb98c61cc89f21068b97c5e2512f0083e4`.

The learning controller, finishing/saving feedback, automatic verified
persistence, 45-second room / 120-second event failure limits, and resident
idle/policy inhibition remain the PR58 production implementation. The
read-only native exit request retains priority over unsaved editors,
shared keyboards, foreground learning, automatic saving, and queued launches.
Existing orderly cleanup and terminal custody fencing remain in place.
No duplicate controller or alternative learning service is introduced.

The full and RF-only service production sources and manifests are unchanged
from PR58, so their version remains 0.3.2. The client version is 0.4.2 to
identify the combined behavior. Neither boot authority nor provider grants
are changed by this source reconciliation.

## Regression coverage

`test_contexts_scene.py` runs the shipping app with the production scene
renderer in Watch and paper profiles. Each profile covers normal operation,
failed storage, retained-custody fencing, 45-second room failure, 120-second
event failure, delayed usable input, verified save retry, and persistent
native exit from Home, a pending refresh, the shared keyboard, active
learning, and the ready-to-save route. Every native-exit case rejects a
queued launch; learning and ready-to-save exits also assert that automatic
saving has not run. Run once normally and once with `--sanitize` for ASan
and UBSan. Raster checks preserve the paper progress ring's partial arc.

The production fingerprint suite runs both full and RF-only configurations
normally and under ASan/UBSan. It covers normal learning, absent input,
discarded/gapped evidence, delayed usable input, bounded event failure,
and decoded exported profile persistence. The existing service, owner
export, retained RF, contract, Watch target, and X4 target checks also apply.

CI uses exact Runtime `3b90cd11cf0ff4e8116bb54d2d9eb9c7c67a148b`, System Apps
`f1db70fcc3ba9df2a3551a711a9d3b0b714a5f8a`, and Drivers
`aac1cb342696e96345d4546e2c69023834c6c8d4`. Product composition must rebuild
the client with its own exact catalog, face header, and System Apps revision,
preserve authority, and qualify the whole store separately. Software tests
and valid target ELFs do not establish physical hardware qualification.
