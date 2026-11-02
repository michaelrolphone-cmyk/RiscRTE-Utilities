# Common Contexts storage and inherited grants

Actual cumulative admission exposed additional original Contexts grants:
RTC clock2 on Watch/Waveshare, runtime realtime1 on X4, and atomic KV1 namespace1
on Waveshare/X4. These remain explicit owner grants. The common manifest keeps
optional integer declarations for all three contracts; it does not grant any
capability by itself. Time rules use time.civil, invocation timing uses the
Runtime monotonic callback/health, and countdown uses the original alarm
owner's status. No extra direct RTC/realtime probe is introduced.

The common ELF now includes the existing atomic user and automatic preference
controller paths, previously accidentally omitted by the common build.
Closed-scene startup detects an authorized atomic namespace1 table without
starting a transaction. A clean absence selects the original legacy KV path.
Present-but-malformed, dirty acquisition, failed release, revocation, busy,
conflict, pending or uncertain transactions never downgrade to legacy writes.
Once selected, existing user-intent and automatic-boundary helpers preserve
their copied submission, collection, cancellation and retention behavior.
Saved keys, namespace IDs and model/rule data formats are unchanged.

Qualification compiles the existing actual controller cases into the common
app, normally and sanitized: user enable/source/timing, busy/delayed/exit/
retained, automatic ordering/noop/low-battery/mask/partial/retained/all-fields,
plus clean-absence legacy writes and acquisition faults. The full common
renderer/input suite and target ELF validation remain required. Actual product
admission remains a separate cumulative Home qualification.
