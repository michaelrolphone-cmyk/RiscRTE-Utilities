# Battery 1.0.9

This bundle increment selects the shared Nova list bridge from the exact System
Apps prerequisite. The original Battery source, telemetry model and provider
API are unchanged. Two large 54px rows and 44px Prev/Update/Next controls replace
the white seven-row compact fallback. Unknown readings remain unknown; no
illustrative charge/current values are introduced.

Actual touch-through-adapter paging and refresh pass normal/ASan+UBSan tests,
including frame-stride guards and complete cleanup. Build the selected app with
`build_nova_utility.py --app battery --system-apps <pin>`. Physical gauge accuracy
and power use remain unqualified. No release, merge or device operation.

[Actual production-source views](screens/battery.png).

Stacked on Utilities Stopwatch PR #18, preserving all current-main Audio Tools
and service changes. Shared renderer pin:
`06bb53cb46c815ef9796eb7999fb36e34420ff0b` (System Apps PR #33).
Reconciled repository unit tests (33), all app fixtures including Battery provider
failures/unknown values, and the real GCC 8.4 selected-app build pass.
Local LeakSanitizer is disabled because of executor ptrace; ASan/UBSan remains enabled.
