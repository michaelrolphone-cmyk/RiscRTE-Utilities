# Contexts 0.4: the launched app uses NOVA components

The previous `app_main` selected `cu_run()` from `contexts_ui.inc`. That renderer
was a plain paged list. The old screenshot test called `ctx_draw()` on a different,
legacy path. Those images did not verify what users launched.

`Apps/contexts.c` now opens a `ui.scene` component document directly. It does not
link the portable display/touch adapter. `contexts_scene_view.inc` describes Home,
Library tabs, item details, naming, training, context rules, settings, workflows,
activity and confirmations as semantic components. `contexts_controller.inc`
keeps the service-backed editor and confirmed persistence. Geometry, icons, fonts,
scrolling, keyboard, and input targets belong to System's scene-host 0.3.0.

The same source targets Watch and X4. Product build inputs select the installed
app catalog, Watch face catalog, source mask, alarm ABI and supported settings.
The X4 package preserves unsupported saved settings but disables their controls.
Nearby-device identity learning remains unavailable; room/event learning and
battery/time/charger triggers are supported. Learning progress and sample counts
come from the service; no simulated sample data or random capture animation ships.

`test_contexts_scene.py` calls the actual `app_main`, wires it to the production
scene host, and sends raw touch/navigation input. It covers tabs during a pending
frame, training with Auto off, plus/minus controls, workflows, keyboard, failed
save/retry, safe modal actions, unavailable storage and terminal display retention.
Captured PPMs come from that execution. Transport fixtures are not physical tests.
The obsolete alternative-renderer screenshot tests have been replaced.

Build with `scripts/build_contexts_scene.py --runtime ... --system-apps ...
--target watch|x4 --catalog ... --output ...`. Watch optionally supplies
`--face-header <product>/apps/clock/faces/ContextsFaces.h`. Both target ELFs pass
structural validation. The selected System tree must contain scene-host 0.3.0.

`compose_contexts_scene.py` upgrades an existing shared-scene store. It preserves
all apps, providers and shared data bindings; adds Contexts grants for `ui.scene`
and `time.civil`; and replaces Contexts plus the presenter. The shared namespaces
remain Audio 2, RF 3, rules 4. It never writes model or rule files. Its output is a
component stage, not a flash image. The product packager must validate the whole
cohort and bind it to the native image. Do not distribute the ELF alone to a store
with the old scene-host or missing grants.
