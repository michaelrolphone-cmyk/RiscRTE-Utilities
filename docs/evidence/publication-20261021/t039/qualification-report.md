Title: Fix BLE shortcut repeats after sampled holds
Repository: michaelrolphone-cmyk/RiscRTE-Utilities
Base: main
Head: tasker/t039-ble-delivery
Draft: true
Local commit: 9c6bae70910cf1f14f17ee78e21ab77268e87b9a
Publication: pending explicit approval; no remote mutation attempted

## Summary

Fix T039's remaining mixed-contact command-delivery defect without changing secure pairing or the HID provider. When a sampled hold ends and another complete tap enters the queue before the next app poll, the old controller releases the first hold but loses the second shortcut.

The controller now distinguishes an already-reported hold from an unreported queued tap. It neutralizes the old hold, preserves later validated pairs, and services the HID owner before dispatching each pair. Existing identity, timing, bounds, sequence, security, subscription, generation and native visibility gates remain. Failure continues through the existing checked session cleanup.

No Drivers changes, version bump, release, tag, catalog, firmware, accepted artifact replacement or hardware operation is included. Touchpad color and paper source tokens are identical to the unchanged base.

## Reproduction

On unchanged base 6b2047500905f8f45087cb49989103a926fc5c63, use production FT6336U 0.2.1 and the actual Buttons controller:

- DOWN at 25 ms is sampled and sends Ctrl+Alt+Delete.
- UP at 30 ms, then DOWN/UP at 35/45 ms, queue before the 60 ms app poll.
- Baseline emits 2 reports instead of the required 4, even after further pumps.
- The repair emits both press/neutral pairs in order, on separate pumps.

The historical October 14 retained-invocation launch report predates the separately delivered shared optional-storage repair. This PR does not claim to re-fix that launch issue or establish its current physical status.

## Validation

- 269 Python unit tests pass.
- 50 real-FT6336U queue/safety cases pass normally and under ASan/UBSan, including 18 new mixed-contact cases.
- Real app + FT6336U + NimBLE exact ATT FIFO passes report and boot protocols, with saved-LTK reconnect and no delayed watchdog duplicates, normal and sanitized.
- Unchanged NimBLE provider protocol and port suites pass normal and sanitized.
- App/model/gesture tests and color/paper production renderer controls pass normal and sanitized.
- Native mixed contacts pass 7/7 physical-size profiles per mode; existing expanded-recipe temporal controls pass 98/98 per mode.
- Native geometry/controller tests pass seven profiles normal and sanitized.
- git diff --check passes. Independent review and selected independent normal/sanitized reruns found no issue.

The first native sanitizer attempts failed because LeakSanitizer cannot run under ptrace. The retained evidence distinguishes those failures from later successful AddressSanitizer/UndefinedBehaviorSanitizer runs with leak detection disabled. Leaks are not qualified.

See docs/t039/MIXED_CONTACT_REPEATS.md for exact dependency pins, commands, scope and remaining qualification. The local evidence bundle includes source hashes, baseline failure, per-case receipts and logs.

## Deliberate qualification limits

The immutable native-ble-buttons-0.1.30-dev1 pipeline requires selector-off identity with its retained predecessor. This intended shared-controller change is not that frozen artifact. The gate and publication conditions are untouched; a separately versioned candidate and qualification recipe are required before publishing artifacts. Do not overwrite or repoint accepted artifacts to make this patch fit the old identity.

Target ELF builds, the complete native release pipeline and remote CI have not run for this patch. Expanded host temporal tests omit full-feature retained-failure cases 7 and 10. Physical panel/touch, Bluetooth RF, real host key acceptance, host switching, deployment and hardware acceptance remain open.

This is a draft source-repair PR only. No merge or firmware release is authorized.
