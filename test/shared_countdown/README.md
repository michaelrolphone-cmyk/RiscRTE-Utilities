# Countdown actual Runtime integration

Runs production Runtime13eca114, System scene734f3171, Countdown app/control and existing scheduler as unloadable host ELFs. Hardware/KV are deterministic doubles; AppDataFiles and loader/provider graph are real.

Four combinations (800x480 MONO1 UTC,240x240 RGB UTC,410x502 RGB UTC,240x240 RGB raw) each run demand, retained-provider and denied-UI modes. One app ELF is byte-identical across all cases within each build. Two GUI invocations edit301seconds, persist draft, unload, restore, start and unload. A semantic launcher actor then steps the actual scheduler at20ms cadence until alert, dismisses and cancels; no GUI/hardware calls occur after unload. noUI makes zero hardware calls/writes. This is not physical qualification, full resident Home integration or UI cancel proof.

Run `python3 scripts/test_countdown_runtime.py --runtime RUNTIME --system-apps SYSTEM --output NEW_DIR [--sanitize]`. Optional `--scheduler-source` uses an externally pinned schedules-only scheduler instead of current mixed service. Broad source hashes and stagedSDK hashes are retained; source changes during run fail.

For an existing exact Runtime executable only, `COUNTDOWN_REUSE_RUNTIME=1` rebuilds all small DSOs and verifies Runtime/test/hardware and SDK inputs. Use a fresh test-data directory or archive/remove generated appdata before replay; never reuse real user data. Results describe reused executable, not a new C++ build. Initial sanitizer attempts failed before execution under fulltmp; retain logs separately.
