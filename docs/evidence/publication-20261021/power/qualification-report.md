# Watch power-status qualification

Task: Complete Watch power-status screen (20a759a6-3a04-41df-9cc3-110219f65bd0).
Observed 2026-10-21 UTC. No production defect was established. Existing implementation is present in the published baseline. This deliverable repairs a stale host test fixture only; it does not change the app, adapter, PMU, product pins, or release.

## Exact released artifact

- Watch 1.1.46 product: ab9ff33975680c5bb8d9a2e7ed346d0300732a4e.
- Store SHA-256: e50d26000dc0366be61abbf480cd39ee98323ea5ef489536baf0cf7c4097b126; independently decoded read-only: 125 files, 61 ELFs.
- Battery version 1.1.3, 155448 bytes, SHA-256 fdcb36ebee771f968c6febe53e1817a712276cd5c54752ad8250b8ecf32c1c6c.
- Battery manifest and boot policy grant board.battery@1, instance 4. Shipped ELF contains the power overview, four detail pages, capability-status labels and error states.
- Battery is an inherited 1.1.39 payload, byte-preserved through the 1.1.43 baseline and 1.1.46. It was not rebuilt with the newer default/Home source lock.
- The producer 24270c36cd4bc1edb7bb2e8d860501eaa5105d43's immutable replay recipe matches the exact released Battery size/hash. That recipe identifies Utilities ca7fe82f43adc4ad8a8fa477a5336c8f5c693770 and System adapter 96742a20af660808cae6cd3a36eadd12a83a460f. These are the primary frozen-source qualification inputs.

## Host checks

1. Actual product PMU source: 8192 status combinations under UBSan, plus sampling failure/recovery and no-write status checks passed. The fixture also contains existing startup fault tests; this does not complete the separate charging-screen task.
2. Released adapter: 256 copied percent/flag combinations, four-byte ABI, missing/inactive provider, failure reset and null guards passed normally and ASan/UBSan.
3. Released app + released adapter: all 20 production-source UI scenarios passed normally and ASan/UBSan (40 runs). Coverage includes known 0%, unavailable estimate, legacy status validity, no battery, USB and thermal flags, charger-complete distinct from gauge 100%, failed-read recovery, Details/Back/Refresh/repeated navigation, failed launch retry, sleep return/refusal/failed retention and custody cleanup. Parent independently reran all 40 successfully.
4. Eight representative host render frames inspected: no visible clipping or layout failure in overview, unknown/error, zero/100%, no battery, basic provider and Details views. Frame-stride guard assertions remained intact.
5. Supplementary (not inherited Battery provenance): Utilities 4fdcad2495c33fe63f5f6faca9b11ced3c739a5b + System bb0cadf3cec3445a64e11513b47b2676f3fdd92d passed the same adapter/UI fixtures. These are the 1.1.46 default-build input lock, not current main.

## Test-only correction and preserved failures

Unmodified test failures are retained in battery-power.log (Runtime suffix initializer warning), battery-power-fixture-fixed.log (poll-count transition clock), and battery-power-timing-fixed.log (50-second stop before 60-second idle). system-power-status.log preserves the environment's LeakSanitizer ptrace error.

The isolated test-only commit changes six source lines and adds 15:
- Use designated Runtime fields, leaving unused append-only suffix fields zero.
- Schedule transitions with the fixture's elapsed-time script_step(), not incidental raster/input capture polls.
- Use the same script clock for the navigation stop assertion.
- Give idle scenarios a 70-second window, crossing the unchanged 60-second idle deadline exactly once.

All status, transition, no-write, stride, retry, nested navigation and custody assertions remain. No production source changed.

Local branch: tasker/watch-power-released-qualification-20261021.
Commit is recorded in local-deliverable.json. Nothing pushed or published.

## Current-source comparison

Compared frozen sources locally with observed Utilities 6b2047500905f8f45087cb49989103a926fc5c63 and System 7270a10. The Battery fixture itself is unchanged in current Utilities, but Apps/battery_power.inc has 97 additions/9 deletions, and current System adapter has 742 additions/14 deletions relative to the frozen adapter. This is a historical qualification patch, not a claim that current main is qualified and not an instruction to roll a product back. Adoption into current sources requires separate target review/tests. Complete diffs are retained.

## Qualification limits and next step

- No new target build: both Xtensa compiler names are absent (toolchain-availability.json). No target instructions executed and no byte-for-byte rebuild attempted.
- The host fixture uses a focused power-status profile. The full released radio/Contexts/low-battery feature composition is not requalified by these checks.
- ASan/UBSan ran with detect_leaks=0 because LeakSanitizer is unsupported under this ptrace environment. LeakSanitizer did not pass.
- Hardware remains not tested. No physical charge current, SOC accuracy, charger behavior, temperature, actual display/touch or sleep power claim is made.
- The separate charging-screen and low-battery task scopes were not changed. No flash, RF, device action, push, PR, merge, tag, catalog or product-pin action occurred.

The existing implementation should remain awaiting qualification rather than completed. The remaining task-specific acceptance is on-device display/status correspondence: compare Battery overview and Details with known battery/input states, ensure unknown/error states do not retain stale status, and verify nested Back/Refresh. Use the already-published baseline only under the separate authorized device-testing flow; this report requests no immediate device action.
