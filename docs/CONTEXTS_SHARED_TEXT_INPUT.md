# Contexts shared text input

Working lifecycle successor: the final published System pin and complete product
qualification are pending. The frozen 0.4.3 receipts remain historical evidence;
the lifecycle gap below is not yet cleared for a delivery candidate.

The selected Watch and X4 build is the existing scene-based Contexts application.
This change only transfers its text editor to the installed `ui.text-input@1`
provider. It does not replace the NOVA component UI with the portable adapter.

## Exact predecessor and selection

Both products previously selected Contexts 0.4.2. Its `Apps/contexts.c`,
`contexts_controller.inc`, and `contexts_scene_view.inc` match Utilities
`4beb9778d300ec9f6b18448e1ec86317e063c98c`; their last Contexts source change was
`fc0bdbbbc600ce891e60c44d091fce0586ade7d8`.

Watch's predecessor ELF is 55,696 bytes, SHA-256
`4896654c86b0c1c203af5e07a20425157a512c48b68dd5bca1c86756ca7dfe9a`.
Its selected build includes the complete product app catalog and the original
`ContextsFaces.h`. X4 selects the immutable SD package at
`Apps/contexts/image-f2fd480a5d3135cf01decbaec4b7288285cee5677e29251d12badcb10928babb/contexts.elf`,
not merely the flat fallback file. Its inherited defines are:

- `CONTEXTS_ALARM_API=2`
- `CONTEXTS_SETTING_MASK=0x274u`
- `PORTABLE_NATIVE_TIME_TOOLBAR`
- `PORTABLE_CONTEXTS_SOURCE_MASK=CONTEXTS_RADIO`

The product recipes preserve these selections and catalogs, opt in with
`PORTABLE_CONTEXTS_SHARED_TEXT_INPUT`, and select Contexts 0.4.4 with exactly one
additional manifest requirement and boot grant: `ui.text-input@1`, instance 0.
The generic 0.4.2 manifest is unchanged; the flag-off build retains the previous
scene-keyboard path.
Product composition must update X4's selected immutable image explicitly.

## Lifecycle correction from 0.4.3

The frozen 0.4.3 qualification covered text/save/cancel/custody and complete
cohort admission, but its modal branch bypassed resident POLL and POLICY work.
That deferred the previous idle/low-battery checkpoints while editing. The
0.4.4 successor repairs this omission using the optional shared-host checkpoint
suffix and `PortableTextInputCheckpointClient.h`; it does not rewrite the
analysis pipeline or the frozen predecessor receipts.

Resident polling retains the original 50 × 10 ms cadence and the existing
foreground-learning/save-state idle/policy inhibits. Presentation or held input
only suppresses resident handoff: ordinary input and logical UI state continue
advancing every loop. After an explicit settled policy request, the host releases display custody
while retaining draft/layer/caps and same-session passive input/navigation. Contexts pauses and
closes its backend before dispatch, reopens it afterward, then resumes the host
with redraw as the old close/open path did. Routine policy sampling retains
passive input queues and navigation claims; only an actual native sleep/focus transition requests the
historical wake-input neutralization. The shared resident bridge maps that reply
without changing the predecessor scene helper. New activity before pause returns
BUSY and invalidates the stale local request; a fresh POLL reports that activity
before another policy dispatch. Home and runtime loss are checked at each
handoff boundary. A missing lifecycle suffix closes cleanly to Retry/Cancel.

Original scene keyboards already suppressed the top-edge shared-controls
gesture. Existing nonkeyboard controls handling is retained.

## Behavior and custody

- Custom learned place/event names, existing item/profile names, context-rule
  names, and trigger names retain their 16-character limits (17-byte requests).
- Workflow messages retain their 32-character limit (33-byte requests).
- The shared client uses printable ASCII, matching the former scene keyboard.
  Empty/invalid names still follow the original controller validation.
- The existing controller determines what Save means: a custom name proceeds
  to capture introduction; item/profile rename uses existing verified persistence;
  rule/trigger names and messages remain edits until their original Save action.
- Cancel returns through the existing name-editor Back path. No accepted draft
  reaches the controller until the host closes, its capability is released, and
  the app scene resumes successfully.
- Home cancels pending text, clears queued workflow launches, skips reopening
  the editor, and exits through normal Contexts cleanup. Native Home is checked
  before input, after host polling, during resume and before acceptance. Pending
  text closure is drained; uncertain custody is retained, never abandoned.
- Runtime liveness and terminal retention fence subsequent provider callbacks,
  including loss inside the native-exit query itself.
- Input polling is independent of display state. A pending e-ink frame may
  delay final close/commit, but cannot gate logical typing or cancellation.
  The Contexts service/capture cadence continues during the shared editor.
- An unavailable text provider or a failed/invalid submission leaves a scene
  Retry/Cancel state. There is no app-local keyboard fallback or copied HID code.

Watch text entry adopts the installed common keyboard styling. This editor
is not pixel-identical to the former Contexts component keyboard.
All other semantic component pages, geometry ownership, scene-host fonts and
glyph metrics remain unchanged. The controller, model/rules formats, storage
namespaces, capture/learning analysis, source masks, action settings, catalogs,
and learned profile data are not rewritten by this migration. Automatic workflow
launches remain pending while the name editor owns input.

## Build and verification

The generic source build supports:

```
python scripts/build_contexts_scene.py --runtime <runtime> \
  --system-apps <system> --target watch --shared-text-input \
  --catalog <catalog.json> --face-header <ContextsFaces.h> --output <fresh-dir>
python scripts/build_contexts_scene.py --runtime <runtime> \
  --system-apps <system> --target x4 --shared-text-input \
  --catalog <catalog.json> --output <fresh-dir>
```

These generic commands do not substitute for the product's exact-pinned build,
store composition, and complete-cohort admission. The device repositories hold
the selected product recipes and receipts. The lifecycle successor requires
the backward-compatible opt-in checkpoint suffix in text host 0.1.7; its exact
published System commit is recorded in the selected product build receipts.
Existing scene-host fonts, native images and unrelated app/provider bytes are
retained; the common opt-in scene/text checkpoint changes are separately qualified by
System tests.
No paused SD-settings changes are dependencies of this work.

`scripts/test_contexts_shared_input.py` exercises the launched app against the
production scene and text hosts, and faults the transport/lifecycle boundaries.
It exercises modal policy on all five editors, activity/input races, pending
frames, learning inhibits, missing suffixes, busy/exit/retained policy replies,
and Home/liveness loss during enable, snapshot, pause, dispatch and resume.
Run the same scenarios with `--sanitize`. Transport fixtures do not establish
physical hardware behavior. Target structural validation and cohort admission
do not execute Xtensa instructions. Device receipts separately state packing,
stack inspection, selected sources and the remaining verification limits.

The product delivery hold is 2026-10-12 23:30 UTC. Source publication is not binary
delivery or flashing. Full-initial images erase internal saved data if flashed;
no such flash is part of this change.
