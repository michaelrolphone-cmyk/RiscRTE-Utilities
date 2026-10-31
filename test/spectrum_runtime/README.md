# Audio Spectrum actual Runtime witness

Production shared app/model/presenter, scene+canvas composition (or standalone
canvas), text-input host and presentation profile execute under actual Runtime.
The observer only copies app state. Native navigation and raw touch events drive
capture, cursor, label form and text cancellation. Microphone, KV2 and physical
output/input endpoints are deterministic doubles. Display receipts remain active
for 80ms; PCM reads during that interval demonstrate capture is not tied to frames.
Each case unloads and reopens the application; no-UI cases require no endpoint I/O.

This is not physical-device proof, a full learning/storage matrix, or resident
Home composition. Those contracts have separate focused tests. The text path
currently exercises Home-cancel without persisting a changed label.

The additional `attention` lane uses the actual extended Runtime resident-shell
API and a separate host ELF with deterministic observation/dispatch callbacks.
Its native capture-exit gate rejects any invocation while PCM is owned. Quiet
recording advances for at least 200 paced blocks without reopening the mic.
The host then reports a long key (ignored), a fresh short key (Back from the
Controls form), an alarm, and another alarm while editing a label. Checks require
no unsafe native barrier call, capture stop before ordinary alarm POLICY,
exactly one resume per alarm, continued PCM while editing, cancellation without
writes, and actual child unload/reopen. This is NOT the production Home observer,
physical alarm scheduler, or complete selected firmware composition; those have
separate qualification.
