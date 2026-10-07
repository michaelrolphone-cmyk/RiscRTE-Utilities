# X4 paper utility integration

Calculator, Stopwatch and Countdown select the retaining MONO1 portrait
presentation from display capabilities. Watch keeps its existing color UI.
The explicit paper build profile links global Home to default.elf, local Back
to springboard.elf, and capability-gated QuickActions without radio authority.

The deployment versions are Calculator0.1.7, Stopwatch0.1.7 and Countdown0.1.6,
recorded in Apps/paper-utilities.json. These do not rewrite released Watch
manifest identities. Preferences use KV1, Stopwatch KV2, Countdown KV3; use the
product's visual alarm service and exact display/input/RTC/battery bindings.

System Apps is pinned to 2aa0cf63346e507525af884bbfbf6b69313442c4. The shared
sleep hook is opt-in and is not selected for these three utilities.

The integrated Linux run passes128 real application/adapter scenarios under
normal and ASan+UBSan builds. A test-only ambiguous conditional was braced for
GCC; the production behavior was unchanged. Target evidence is emitted beside
each ELF. No physical device qualification is implied.
