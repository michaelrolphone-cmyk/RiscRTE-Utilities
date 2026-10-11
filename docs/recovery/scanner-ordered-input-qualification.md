# Recovered X4 Scanner input qualification

Result: 48/48 production Scanner/shared-host scenarios pass, covering 24 cases normally and with ASan/UBSan. LSan is disabled (`ASAN_OPTIONS=detect_leaks=0`); UBSan stops on the first error. Host GCC: cc (Debian 14.2.0-19) 14.2.0.

## Source scope

- Utilities: 1499896e3c9a56f985325043bcea192435f6c497, plus the two-file test/runner patch.
- System: b6da27eb856b7be8d04eeaf641d1a401a61584f6, plus the separately owned two-line scene header-contact fix.
- Runtime SDK: 274bc66f193cbe29018d2a85c9400cb0dce8aacc.
- Drivers (read-only): 4088b6892c2e2654b0342f04a7d191068e4a8e2e; BLE sensor source SHA256 7ccbd31e6253db7183fe9178f87c2c825b37c5cf59ac27bb61a15f46ac2d79e2.
- Delivered .51 Scanner profile: 0.2.19; selected X4 successor: 0.2.20. The unrelated generic source sidecar remains 0.2.4.
- All delivered feature defines, capability requirements, and grants are checked unchanged. Original SDK header hashes are verified before current-source staging. Every remapped host compilation input is recorded: 101 dependencies, including production app, adapter, shared scene/text hosts, provider, and fixture.
- No target ELF was compiled, no device/radio/endpoint was accessed, and no source was published. Host qualification is not hardware qualification. Original parent working trees were not edited.

## Fixture/runner changes

The recovered adapter requires ordered reports. The old fixture emitted events only while text input was active and otherwise changed only snapshots. Its gestures advanced on raw-poll counts, which renderer checkpoints now also increase.

The fixture now emits ordered DOWN/MOVE/UP reports in app and modal phases, maintains a bounded subscriber queue and matching snapshot sequence, and uses simulated wall time plus app/scan/text readiness. Capture-only polls do not advance gesture time or overwrite queued edges. Navigation remains single-consumption. Each normal scenario asserts all scripted actions completed. A simulated-time deadline fails stalled modal sessions.

The portrait orientation check samples a straight Q-key border and interior, avoiding the recovered rounded corner. Pointer Cancel targets the visible Back center. Cleanup, secure radio cleanup, original-name preservation on cancellation/unavailability, exact saved strings, repeated modal use, Runtime-loss freeze, failed-cleanup retention, and refused retained re-entry assertions remain active.

## Production defect independently isolated

Against unmodified System b6da27eb856b7be8d04eeaf641d1a401a61584f6, 42/48 cases pass. Pointer Cancel, failed-unsubscribe cancellation, and repeated naming fail in both normal and sanitized runs. `finish_contacts()` clears the generic header contact whenever the keyboard-contact queue is empty, so a Back DOWN is lost before the later UP.

The scene owner's fix returns immediately when that queue is empty. With only that two-line production change, 48/48 pass. The only changed compiled dependency between baseline and fixed runs is System/Services/scene_host/host.c:

- Before: abb5025b9ca42d6b5b6c91bf005a03070139106e5d6b0361fbd007f0f3f22351
- After: 5922080ddec668196c1ef46fbcfaa4615bd17e15bf51b605657a393da1c803f6

The System owner separately qualifies this fix and owns its integration. The Utilities patch contains no production changes.

## Cases

Idle; scan/stop; nested Back; enable/scan; name Save; pointer Cancel; navigation Back; unavailable editor; failed-cleanup retained editor; Back return; repeated save/cancel/save; Home; naming Home; resident Controls; and Runtime loss at text acquire, open, poll, close, release, key-value acquire, get, put, release, and failed text acquire. Successful teardown also checks native-time callback availability after clean app return and before finalization. Frame orientation, complete frame size, and physical stride guards remain checked.

The final repeated-name screenshot shows `Kitchen sensorWW`, proving the middle cancelled edit did not persist.

## Reproduce

python /tmp/scanner-input-qualification-20261010/utilities/scripts/test_x4_ble_shared_text.py --build /workspace/shared/points-source-recovery/051/source-validation/targets/ble --drivers /tmp/radio-iq-source-recovery --system-apps /tmp/scanner-input-qualification-20261010/system --runtime /workspace/shared/recovered-runtime-0106 --output /tmp/scanner-input-qualification-20261010/fixed-evidence

## Artifacts

- scanner-ordered-input-qualification.patch: 89635918096f52c4722857f2d38d55f328bf4b740091ce40abdfd2181ab9bc98
- scene-header-contact-fix.patch (separate owner): d17631983fa854565866291001045037c8c9faa423f5928d5174d8a4f13f2254
- baseline-evidence/summary.json: c66e64918a13de8212aa4c33089c81bcf777fa167d235271a9dd5568accbaaac
- fixed-evidence/summary.json: 6fd2ec39eb012cc43dce11c15c89cac3c245ea0c0a2775e6dddd9406acc36ccf

Summary files contain all host compile commands, input hashes, per-case results, action hashes and log hashes. Per-case actions and logs are included in the compact evidence archive; full frames and executable/object files remain in the local evidence directories.
