# Frequency shared-app preservation

The application remains one device-neutral module. Native scene components own
layout, focus, input and presentation. The application has no device, backend or
display-resolution selection.

* Original `tone_core.h` remains unchanged: 20–7000 Hz, 16 kHz mono,
  256-sample blocks, phase-preserving edits and the original gain ramp.
* Defaults remain 440 Hz / 100%, frequency steps 10/100/1000 Hz and level
  increments 1%. Start is explicit; Stop, 60-second limit and ownership
  handoffs never automatically restart the tone.
* Alarm progression uses the published copied status/CUE/BLOCKED reservation
  protocol. Owned audio must close successfully before another owner can run.
* RF Context and BLE telemetry progression is independent of healthy tone and
  presentation cadence. Existing namespaces and CFP/rules formats are retained;
  AUDIO_SESSION does not execute automatic presets/rules or background audio.
* Original short power-key Back and neutral rearming remain app semantics;
  long/combined inputs do not become host sleep/exit commands. The negotiated
  idle-only Home contract does not consume manual intent to choose a policy.
* Configured idle duration is read with the original codec. Only actual user
  activity refreshes its anchor. Sleep remains provider-owned and capture/UI
  custody is settled before any resident checkpoint.
* UTC aging uses an explicitly configured provider UTC suffix when available.
  Its failed read never substitutes stale native time. Old providers can use
  a valid native UTC source; unavailable aging is disclosed. Existing raw RTC
  calendars are neither migrated nor rewritten.

Qualification layers are separate: session/controller/background endpoint
tests, actual shared scene/Runtime ELF execution, selected target repeat and
byte-identity proof, and product manifest/provider composition. Host endpoints
and native exit-barrier doubles are not physical audio/sleep verification.
The stopped System header-focus publication is not represented as delivered.

## Publication qualification boundary

The local 9+9 real Runtime witness used a separately qualified generic scene
header-focus repair. That repair is not included in this application change or
in published System contract commit161d267a. The hosted actual-Runtime lane is
left enabled and unweakened against the published provider; its navigation
result remains an explicit dependency qualification gap until that provider
repair is independently deliverable. Controller/target success alone does not
claim full hosted navigation acceptance. No physical-device result is claimed.
