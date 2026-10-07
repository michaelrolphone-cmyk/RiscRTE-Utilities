# Waterfall 0.1.2

The shared `waterfall.elf` app displays receive-only 256-pair IQ bursts through
`radio.iq@1`, performs the existing 256-point FFT and scrolls a four-tone
240×200 waterfall on a 240×240 display. LO is fixed at 2440 MHz by the driver;
this is a burst display, not continuous 80 Msps acquisition. It does not touch
modem registers or ROM entry points. Clock remains the Watch default app.

Every successful burst repaints, even when the status remains BURST. The
original 0.1.1 only repainted when the status changed, hiding subsequent rows.
A persistent radio.iq grant does not keep RF active: the guarded driver releases
its native resource lease after each completed burst.

- BACK/top-left exits to Springboard.
- PAUSE/START at top-right stops or explicitly retries reception.
- Idle sleep and foreground radio suspension pause acquisition. Wake does not
  silently restart it; tap START.
- Airplane mode and unavailable radio policy fail closed without capture.
- Busy Wi-Fi/BLE, missing grant, PLL/PBUS/dump failure are visible states.
- Unconfirmed cleanup keeps the invocation/grants pinned and retries without
  display, storage, alarm or other provider I/O. Failed grant release also retries.

The app requires the append-only `suspend` cleanup extension of `radio.iq@1`.
The current Watch profile supplies display/touch, alarm/battery, shared read-only
radio preferences, RTC/Quick Controls and motion-wake integration. The SDR source
inventory is separate from the preserved Calculator/Stopwatch build lane.
The existing shared `solid:f0ec` right-left glyph gives this app a distinct icon
without changing other apps' font assets.

Build: `python scripts/build_waterfall.py --system-apps <exact System Apps pin>`.
Test: `python scripts/test_waterfall.py --system-apps <System Apps checkout>`.
The build requires System Apps `2d16d9dfa7abc50ce916eebc11d81423adef0000` and
Xtensa GCC8.4.0. It retains an ELF, manifest, import/export/hash record and notices.
Watch's current cohort builder adds its exact sleep/navigation profile and
validates the final loader mapping and graph. Old Watch PR #8 install snippets
are superseded; never replace Wi-Fi instance 15 with the IQ driver.

Hardware capture, PLL/sensitivity, actual wake/current use, and Wi-Fi/BLE recovery
on a physical Watch are unrun. Driver/software tests do not qualify those.
