# Selected Wave atomic preferences integration

`python scripts/test_alarms_atomic_runtime.py --runtime <selected-Wave-Runtime> --system-apps <scene-System-source> --output <new-directory> [--sanitize]`

This isolated fixture compiles production Runtime, ProviderGraph, unloadable Alarms, alarm.control, raw-clock alarm scheduler and scene provider. It enables the selected product's atomic/preferences broker features and binds the app's `storage.preferences` API 1 instance 1, private draft namespace 65, and alarm.control instance 0. No existing general scene/scheduler fixture is modified.

At native 410x502, deterministic navigation enters the actual volume editor, increments 50 to 60, and saves. Runtime routes the real app transaction through AtomicSettingsEndpoint/NativeAtomicSettingsAdapter to the selected source's NativeAtomicKeyValue and NativeReplicaCore. The worker runs on a separate std::thread after three pending yields. Disk, allocator, display, input, clock, and worker scheduling are test fixtures, not an ESP native settings worker or physical hardware.

Cases:
- Successful commit, actual app unload/reopen, final saved draft and volume 60.
- Disk publishes the durable root but its immediate readback fails. The real native core returns UNCERTAIN. The app's pending opaque command and uncertain result survive actual unload. The fixture reloads the native core from the same disk (storage-process restart boundary), then the next actual app invocation navigates to RETRY EXACT SAVE. It reconciles the committed value without submitting another intent. This is not a complete device power-cycle simulation.

Both cases require exactly one durable intent, one native worker operation, three pending yields, two app finalizations and unloads, volume 60, and a cleared final draft. Any legacy write to namespace 1 aborts; unrelated scheduler writes remain allowed and counted. The generated policy is a focused capability composition, not the entire shipped product boot image. Source dependency hashes and compiler commands are retained in dependencies.json; ASan/UBSan uses leak detection disabled for this host environment.

No target ELF, release publication, or device installation is asserted by this test.
