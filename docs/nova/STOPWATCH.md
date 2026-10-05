# Stopwatch 0.1.4

The explicit Nova profile shares Settings' fonts, cyan palette and rounded
buttons. A large HH:MM:SS value and separate fractional field replace the
monochrome bitmap text. Start/Pause, Reset/Confirm and Retry are 64×48 buttons.
The original monotonic/RTC recovery, approximate flag, reset confirmation,
persistence, frozen uncertain pause and retained-sleep behavior are unchanged.

Normal/ASan+UBSan real-touch adapter tests cover start/pause and confirmed reset;
existing model, provider-failure and retained tests remain required. Build the
selected app with `build_nova_utility.py --app stopwatch --system-apps <pin>`.
No physical timing, power or touch qualification, release, merge or device write.

[Actual production-source views](screens/stopwatch.png).

Stacked on Utilities Calculator PR #17, preserving current-main Audio Tools and
service changes. Shared renderer pin:
`06bb53cb46c815ef9796eb7999fb36e34420ff0b` (System Apps PR #33).
The normal/ASan+UBSan daily app fixtures and 33 repository unit tests pass after
this reconciliation, along with the selected real GCC 8.4 Nova target validation.
Local LeakSanitizer is disabled because of executor ptrace; ASan/UBSan remains enabled.
