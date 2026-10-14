# T039 Buttons command delivery

Baseline Watch 1.1.30 and Wave 0.1.33 deliver the same Buttons 0.1.23 binary;
X4 0.1.116 delivers 0.1.25 with the same single-contact recovery. Two completed
queued taps were treated as a touch gap and yielded zero HID reports while READY.
The production FT6336U fixture reproduces that failure before the change.

A bounded 16-entry pending-action queue now preserves validated short contacts,
including successive same-button taps, and submits one press/neutral pair per
HID poll. This leaves transport service time between shortcuts. Session changes,
subscription changes, explicit cancellation, overflow and uncertain custody clear
pending input; no command is replayed into a new host or generation. Existing
100 ms contact/timing and neutral gates remain. Four assignments, persistence,
held inputs, mouse actions, configuration, pairing and Forget are preserved.

32 real-touch app scenarios pass normal and ASan/UBSan. The existing app/model,
secure pairing, cleanup, persistence and raster tests pass. An independent central
receives exact ATT keyboard press/neutral ordering for 15 queued shortcuts before
and after saved-LTK reconnect through production Buttons, FT6336U and NimBLE.
This is synthetic controller evidence, not physical radio or OS interoperability.

T037 reconciliation: Watch/X4 already contain shared HID 0.1.4; Wave has 0.1.2.
Use the qualified 0.1.4 dependency so Wave retains the existing Forget/host-switch
race recovery, without changing pairing policy or native controller code.
