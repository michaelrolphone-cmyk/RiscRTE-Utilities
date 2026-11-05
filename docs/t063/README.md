# Waveshare startup alarm deferral

The published color alarm profile treats native NVS's typed no-I/O BUSY result as ALARM_STORAGE during its initial preference/configuration reads. Its valid error screen then requires manual Retry even though the transient exclusion has ended. This is reproduced at each of nine startup read phases.

The new explicit `wave-startup-busy` production build selects `ALARM_STARTUP_BUSY_RETRY`, retaining the deployed color profile's Points, Denver RTC conversion, volume, DND and verified clock-change control. Only pre-anchor startup reads defer typed BUSY, returning ALARM_PENDING without changing phase or showing an error. Real IO, malformed data and ambiguous writes retain existing semantics. The unselected Watch and tagged X4 profiles are unchanged.

Target GCC 8.4 output contains the startup_storage_busy state; its compacted ELF passes 8 production loader alignments and 2528 relocation-site checks with exact sections/redzones. The same production source/defines pass normal/ASan/UBSan behavior tests, and all existing clock-change, Points, startup/catalog and sleep cases pass. This does not claim Xtensa instruction execution or hardware testing.
