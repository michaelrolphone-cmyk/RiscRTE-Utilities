# Shared Alarms application

`alarms.elf` is the canonical semantic application registered in `shared_scene_apps`. Its source contains no device selector or presentation geometry. The shared target builder compiles one module and verifies the repeated build and identical X4, T-Watch and Waveshare package selections. This packaging proof does not establish installed product selection or physical behavior.

The scene provider owns presentation and input. The resident-shell bridge owns shared controls and idle/policy transitions. Alarms checkpoints its existing domain/navigation journal before handing over, closes the scene completely before dispatch, and reopens the logical route afterward. Pending commands inhibit automatic policy/idle handoff; explicit controls preserve their exact journal. Unreadable recovery records are never overwritten merely to open controls. Runtime identity and retained outcomes fence further calls.

Status changes coalesce while the provider reports raster/input custody. Without lifecycle tracking, only explicit user actions trigger publication. Unsupported volume, automatic refresh and shared-controls capabilities are disclosed separately from alarm and storage errors. An unavailable volume capability disables editing even on a restored volume route.

`ui.scene`, `alarm.control`, and `storage.app-data` are optional manifest capabilities. Missing dependencies exit cleanly; there is no raw hardware UI fallback. Existing alarm service scheduling, saved records and namespace ownership are unchanged. The separate headless service remains the authority after the editor unloads.

Qualification includes focused lifecycle/storage regressions and actual production Runtime/loader/provider runs over 800x480 monochrome, 240x240 RGB565 and 410x502 RGB565, including missing UI. Hosted qualification adds sanitizer execution and canonical target ELF admission. No device flash, physical-input or installed-product acceptance is implied by host tests.

### Semantic preference transactions (0.3.1)

A namespace can expose legacy key-value reads while intentionally forbidding
legacy writes (for example an atomic settings backend). Volume intent therefore
has an additive `AlarmControlPreferencesV1` descriptor: the control provider
validates the original opaque command and projects a copied key/before/after
plan, without receiving or retaining an application capability. The API1 prefix
and existing durable command format remain unchanged.

When the explicitly authorized `storage.preferences` grant is available, the
application uses the owning System `PortablePreferenceAtomic` helper. It saves
the exact command first, compares captured current bytes with the plan, submits
once, and collects an accepted operation before releasing any capability or
handing off the foreground. Context loss or ambiguous token custody retains the
invocation. Conflict and uncertainty preserve the journal. On restart, an exact
match to the desired bytes reconciles without another submission. Only after a
confirmed write does a read/refresh-only control reconciliation method check
the exact desired bytes and refresh the existing scheduler. An interleaved writer
causes STALE; this path can never fall through to another legacy write.
No old settings namespace or scheduler bytes are moved, and no Runtime guard is
weakened. Raw-clock products authorize the existing settings namespace1; a
visual-only product does not acquire a volume capability it does not use.

Focused transaction tests cover pending collection, conflict, uncertain receipt,
committed uncertainty, checkpoint reload, busy refusal, ambiguous tokens, owner
loss, failed journal writes, malformed plans and non-preference commands. These
are not physical-device qualification; the source and package receipts separately
identify actual Runtime, target build and product composition evidence.
