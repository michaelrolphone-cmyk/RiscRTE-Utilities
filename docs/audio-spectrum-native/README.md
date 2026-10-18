# Audio Spectrum native presentation

This is an in-progress, opt-in app migration. No released application or firmware
asset is replaced by this checkpoint.

Baseline Utilities: 12c7440c4c2fd9b6b78e10f11a983947c145b346.
Current shared System: 947895afdb2ecef59654402cf08fe7ecdf92362f.
The existing DSP/model/controller suite's retained SDK is System
fdc15395928a1fa8eee62a2823b3c8777dcf0fa0.

## Geometry checkpoint

`spectrum_native_layout.h` describes capability-pixel rectangles and checked
half-open targets. It does not allocate memory, call providers, or change model
state. The geometry test covers twelve profiles and 100,000 deterministic
positive dimension pairs, including extreme integer dimensions. This is an
arithmetic/bounds test, not a claim of usable UI on a one-pixel display.

The analysis model retains 228 spectrum samples, 166 waterfall frequency bins,
and the 228 by 166 analysis-history ring. History capacity counts analysis
updates, not a fixed elapsed-time duration. A partial history retains its empty
left prefix. Native drawing maps numeric values into display primitives; it does
not scale a completed legacy framebuffer. Cursor pixels and data-bin indexes
must derive independently from frequency.

## Qualification limits

The unchanged 26 DSP/model/controller fixtures passed normally and with
ASan/UBSan using the retained SDK. LeakSanitizer is disabled because this executor
reports ptrace incompatibility. Eight existing modal-policy cases passed with
the current full-feature System profile. These controls do not establish native
view completeness, current target ELF compatibility, or hardware usability.

Native view and target integration is being qualified separately. The original
profile remains the default. All capture, storage, shared-editor and retention
controllers remain required; a reduced renderer-only build is not a release
recipe.
