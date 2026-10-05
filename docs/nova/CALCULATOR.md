# Calculator 0.1.4

The explicit PORTABLE_NOVA_UI profile uses the actual shared Settings palette and
fonts. Sixteen 49×44 arithmetic keys replace the dense 19-key grid. Clear, change
sign and Delete are available in a separate large-button panel. Its Back returns
to arithmetic without queueing an app exit. Root Back uses CALCULATOR_RETURN_APP;
a generic eager return macro is rejected. Fixed-decimal arithmetic is unchanged.
Calculator uses the genuine unique Font Awesome calculator glyph.

Normal/ASan+UBSan tests execute actual touch snapshots through the real adapter:
seven cases: 7+2=9, change sign, nested Back followed by continued input and root
return, Delete/Clear, divide-by-zero, digit recovery, and rejected return followed
by continued input and a successful retry. All cases assert grant/frame cleanup. Existing arithmetic and legacy source
fixtures remain required. The selected-app target builder is
`build_nova_utility.py --app calculator --system-apps <exact pinned checkout>`;
it validates real GCC8.4 Xtensa structure/imports/exports. These are software
checks, not physical touch qualification. No merge, release or device write.

[Actual production-source views](screens/calculator.png).

Reconciled onto Utilities `facc8fd` while preserving its Audio Tools/service code and
existing CI dependency cohorts. Nova uses System Apps
`06bb53cb46c815ef9796eb7999fb36e34420ff0b` (shared prerequisite PR #33).
Local checks also run every existing main-branch native suite and GCC 8.4 target
build. ASan/UBSan is enabled; local LeakSanitizer is disabled because the executor
runs under ptrace. CI keeps the default sanitizer configuration.
