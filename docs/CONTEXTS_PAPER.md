# X4 Contexts paper 0.1.3

This is an explicit local development profile of the existing Contexts editor.
The default Watch application and both full/RF-only service manifests remain
0.1.2. No product, boot grants, Clock/default application or release is changed.
The RF-only service remains built from the existing source with only
`platform.clock@1` and `radio.iq@1` dependencies.

## Sources and build

- Utilities base: `fc503e0f5b1f50c79a5cdeca7bef79549522ddff`
- X4 System base: `0df9d4abdceba74311494614bdb4154645aeeac6`
- Selected local System hooks: `45215c6c57ca8e48a424efd7e37e827205696a18`
- Canonical Contexts contract source: `77ee763f258c87bd712441c567171fc3bb42dbaf`
- Runtime native/Light SDK: `c77e717571a80bc18009b9c3a98532fee32e6fd2`
- Tagged Alarm SDK: `637e13b0bce62ad49b756bec2468a6271d163fc7`
- X4 helper/typed SDK source: `e32937d6580da5ad6a9325227d38b8d731a33c0e`

Build with the existing GCC 8.4 esp-2021r2-patch5 installation. From Utilities:

```sh
NATIVE_APP_CC=/workspace/scratch/c744abbbbd60/watch-build-tools/platformio-core/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-gcc \
python3 scripts/build_contexts_app.py --profile x4-paper \
  --system-apps ../x4-contexts-client-hooks-020 \
  --runtime ../runtime-lazy-retained-providers \
  --x4-idle-source ../x4-hid-input-020/minimal/apps/portable_idle_sleep.c \
  --x4-idle-sdk ../x4-hid-input-020/build/drivers-020/sdk
```

The builder validates the target ELF, exact export set and bounded import set;
records compiler, actual source revisions/dirty states, every compiled dependency
hash, helper hash and physical grants; and copies the genuine font/icon licenses.
Output is `dist/contexts-paper/contexts.elf`, `contexts.json`,
`contexts.boot-policy.json`, `build-evidence.json` and `licenses/`.
The default `scripts/build_contexts_app.py --system-apps ...` Watch recipe is
unchanged. Its target ELF and 16 production screenshots match the baseline bytes.

## Grants and integration boundary

`Apps/contexts-paper.json` is the selected profile. Generated boot policy requires:

| Capability | API | Instance |
| --- | ---: | ---: |
| display.output | 1 | 3 |
| input.touch.raw | 1 | 4 |
| input.navigation | 1 | 6 |
| board.battery | 1 | 7 |
| runtime.realtime | 1 | 0 |
| storage.key-value | 1 | 1 |
| alarm.service | 2 | 0 |
| contexts.service | 1 | 0 |
| x4.power | 1 | 17 |
| storage.volume | 1 | 9 |
| net.wifi | 1 | 15 |
| bluetooth.hci | 1 | 16 |

Runtime realtime is read-only. Namespace 1 holds existing time, reader, Quick
Controls, radio, idle and Contexts records; no new model namespace is introduced.
The service has separate clock/RF grants supplied by the central integrator.
The app never acquires `radio.iq` or Audio directly. Model reload requests the
service's supported source set, then hands off to `default.elf`; the central
Clock/owner rendezvous must already be selected before product activation.
Local root Back goes to `springboard.elf`. Home goes to `default.elf` through the
shared adapter and asks to discard an unsaved draft before leaving.

These are admission inputs, not product activation. Canonical System header
publication remains blocked; all new commits and artifacts remain local.
The final Runtime union and real X4 hardware need central qualification.

## Controls and state

Actual retaining MONO1 display capabilities select the paper presentation.
The X4 native 800×480 surface uses shared 90-degree rotation to logical 480×800;
reader flip transforms both presentation and physical input. The existing Watch
240×240 Nova path remains the default and uses its original manifests.

The paper UI provides monitoring on/off, live or last room/event status,
Audio/RF model readiness (signature, temporal, neural), explicit model reload,
eight saved preset slots, saved-room identity selection, enable/disable, action
values, verified Save, frozen uncertain-save Retry, and explicit draft discard.
RF-only Audio displays Source unavailable. Missing, stale, failed and older
signature-only providers remain distinct. A clean absent service displays
Monitor unavailable; invalid or ambiguous custody remains terminal.

X4's action mask follows actual capabilities: idle and DND are available when
supported; absent frontlight, sound, notification output and face catalog remain
unavailable. X4's automatic policy remains Light-only and exposes no sleep-mode
or deep-timer action. Existing unsupported saved fields remain intact and may
be explicitly removed; automatic application cannot write unsupported actions.
Opening the app does not create defaults, modify presets or train models.

The app borrows its service only at settled foreground points, reacquiring after
Light or handoff. Quick Controls, alarm/sleep and background-capture fences remain
in the shared System adapter. Native custody uses the canonical retaining
Runtime wrapper. A dirty or unconfirmed cleanup causes no later I/O or frees.

Lists use shared continuous touch scrolling and raster clipping with fixed
header/footer controls. A contact retains the identity of the completed image
at DOWN. The latest dirty state survives pending async frames; neither frame
memory nor storage is modified by a tap while presentation is busy. Model age,
sample counts and other invisible counters do not trigger redundant paper
refreshes. Status refreshes never repaint an unrelated draft editor page.

## Validation and visual review

```sh
python3 scripts/test_contexts_paper.py \
  --system-apps ../x4-contexts-client-hooks-020 \
  --runtime ../runtime-lazy-retained-providers \
  --x4-idle-sdk ../x4-hid-input-020/build/drivers-020/sdk
python3 scripts/test_contexts_app.py --system-apps ../contexts-rf-only-system
python3 scripts/test_contexts_service.py --drivers ../watch-power-drivers \
  --runtime ../runtime-lazy-retained-providers --system-apps ../x4-contexts-client-hooks-020
python3 scripts/test_contexts_rf_retained.py --system-apps ../x4-contexts-client-hooks-020
python3 -m unittest discover -s tests -p 'test_*.py'
```

The paper fixture compiles the actual app, X4 adapter, fonts/icons, Quick
Controls and typed headers. Normal and ASan/UBSan runs cover both reader
orientations, actual app_main physical touch/Home routing, draft interruption,
scroll and modal cancellation, capability gating, stale models, frozen uncertain
writes/retry, unread records, service disappearance/recovery, immutable async
frames, completed-image contact identity, Light resume/reborrow, root Back and
launch refusal/retry. 25 raster scenes per orientation have byte-identical
logical pixels after flip. The separate shared System fixture covers 124
normal/sanitized lifecycle cases, including terminal cleanup and malformed or
absent service admission. Utilities inventory regression has 83 passing tests.
ASan/UBSan are enabled; LeakSanitizer is disabled for the executor environment.

The supplied NOVA-7 interactive 480×800 HTML was executed to emit its own SVG
view functions and rasterized with Inkscape. Orbitron/Rajdhani font files came
from the existing shared System assets. Headless Chromium could not create
its process socket in this executor; no browser screenshot is claimed.
Reference Points/edit pixels and the production Contexts pixels were visually
inspected. Shared one-bit headings, row rules, inversion, genuine FontAwesome
chevrons, fixed chrome and clear unavailable/error text were checked.
See `evidence/contexts-paper-0.1.3/` for reviewed images and the test output under
`build/contexts-paper/`. No physical panel/RF timing, live CI, firmware flash,
release or publication is claimed.
