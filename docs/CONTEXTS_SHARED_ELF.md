# Contexts shared application
The original source inventory at Utilities6b204750 includes the scene controller,
legacy/paper history, shared text input, fingerprint/rule services, and native
qualification. This change reuses the launched scene path; it does not replace
the editor or its record formats.

One target build takes only Runtime and System source inputs. It embeds neither
a product catalog nor target-selected alarm ABI, source mask, setting mask,
display dimensions or face header. Native Watch face IDs are the identical
canonical34-face set used by the converged Home, distinct from overview's
desk_clock_face IDs. Actual launch choices come from admitted app.catalog.

Alarm2 is explicitly negotiated/validated before clean absence permits Alarm1.
Partial acquisition, malformed tables and uncertain release retain custody.
The resident Home's read-only Contexts setting/option queries determine offered
settings; an older/absent host displays unavailable rather than inventing
support. Unsupported saved values are preserved and are not applied.
Workflow launch identities are revalidated against the native catalog generation.

All rendering, geometry, input targets and keyboard presentation stay in
ui.scene/ui.text-input providers. Contexts models, rules, preferences and their
confirmation/retry behavior are unchanged. Missing storage/service/capabilities
stay visible; detection does not write defaults or fabricate samples.

The build emits one contexts.elf and three deployment references to its exact
SHA256. This is not yet an installed firmware cohort. Each product composition
must include the matching scene/text hosts, Contexts service/shared data grants,
native app.catalog and resident Contexts query implementation. Data namespace
bindings remain the original Audio2, RF3, rules4 and preference1.

Qualification: scripts/test_contexts_shared_elf.py tests optional-capability
failures and option preservation using actual app code. Existing native985-case
and paper/shared-input suites run with the common app switch, normal and
ASan/UBSan. CI receipts, target validation and catalog publication are separate;
physical testing is not implied.
