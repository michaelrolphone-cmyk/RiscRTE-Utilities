# Native Calculator source qualification

The opt-in PORTABLE_CALCULATOR_NATIVE_UI controller lays out and rasterizes
Calculator directly at reported color-display dimensions. Keys, Tools, Back,
result and status have independently computed native rectangles. Native font
faces are selected to fit each rectangle; long display strings are shortened
visually without changing calculator_core state. The existing calculator core,
keyboard traversal, Tools commands, nested Back and refused-launch recovery
remain shared. No historical arithmetic semantics are changed.

The private System Calculator owner8 hooks make each main/Tools hit map visible
only after its matching frame completes. Pre-visible/held contacts are fenced;
DOWN and UP must belong to the same painted control. Raw queue gaps, changed
maps and cross-control movement cancel pending touch actions. An unchanged
repaint retains its map. Display leases and pending pixels remain adapter-owned.

Legacy and paper builds retain their original path and pins. utility_frame.h
is unchanged. The native build is an explicit source qualification option;
this change does not update product firmware recipes or released assets.

Commands (exact System source pin in sdk/calculator-native-sources.json):

    python3 scripts/test_calculator_native.py --system-apps ../system --app calculator
    python3 scripts/test_calculator_native.py --system-apps ../system --app calculator --snapshot
    ASAN_OPTIONS=detect_leaks=0 python3 scripts/test_nova_utility.py --system-apps ../system --app calculator
    python3 scripts/build_nova_utility.py --system-apps ../system --app calculator --native-calculator

The native test runs the actual production app and adapter. Each direct and
snapshot matrix covers12 scenarios across five sizes, synchronous/120ms display
latency, normal and ASan/UBSan modes. It checks arithmetic/error/reset/Tools,
return recovery, startup held input, adjacent-key crossing, queue gaps, keyboard,
repeated module entry, stride guards, pending-frame byte immutability and complete
grant/subscription/frame cleanup. Receipts contain source hashes. Existing14
legacy Calculator cases and36 System feature-off compositions are separate
regression controls. Physical fit, touch and performance remain untested.
