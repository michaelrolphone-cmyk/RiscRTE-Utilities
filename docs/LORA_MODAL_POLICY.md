# LoRa shared-editor policy

The optional `PORTABLE_TEXT_INPUT_POLICY` selector restores predecessor modal
power behavior through the shared System checkpoint bridge. It accompanies
`PORTABLE_LORA_SHARED_TEXT_INPUT` on the selected Watch successor. There is no
selected X4 LoRa payload.

The ordinary adapter input loop stays suspended while the shared host owns
input. Routine checkpoints retain input continuity; a possible idle sleep fully
detaches the host before dispatch. The editor keeps its draft, 240-byte message
limit and numeric RF field validation. Done remains a draft operation; Send and
RF Apply retain their existing authority. No message/model/storage format changes.

Unlike continuous RF/Spectrum capture, LoRa listening has no predecessor idle
inhibit. A 65-second idle editor must sample battery 13 times, attempt idle sleep
once, and stop an active listener through the existing orderly radio cancellation.
The modal loop still services `lm_step` before that sleep. Wake does not silently
restart listening. Background Contexts/BLE service calls follow existing session
restrictions; callback counts do not prove physical RF/audio activity.

Pause mode is retained across AGAIN; a failed resume retains custody without
repeating policy. Unsupported checkpoint descriptors close cleanly, leaving the
original draft unchanged. Home and Runtime exit during a pending pause remain
clean cancellation paths. No alternate local keyboard is introduced.

`test_lora_policy.py` runs focused policy cases with actual app/adapter/shared
hosts and the complete selected 0.1.14 defines. `test_lora_shared_input.py` runs
all compose, numeric, long-message, send/history, missing-host, delayed-close and
retention workflows in normal and ASan/UBSan modes, comparing resulting image
hashes. `test_lora_policy_parity.py` proves disabled-policy C token identity and
unchanged model bytes. Existing model/controller tests remain unchanged.

The selected predecessor already uses NOVA text faces 0/1/2. Its exact font
closure and compiler flags are retained; this correction removes no fonts.
Target flag-off byte identity, static stack storage, complete cohort admission,
lossless metadata parity and publication are separate qualification gates.
No physical radio, PMU, sleep/wake, flash or hardware delivery claim is made.
