# T069 buffered mode diagnostics

The reported Waterfall mode-switch crash remains unreproduced. Native 410×502 full-history mode transitions and retained startup paths pass host sanitizers; these checks do not establish hardware stability.

PORTABLE_SPECTRUM_STAGE_TRACE enables a fixed six-line RAM ring copied through Runtime diagnostic checkpoints and sparse native buffered diagnostics. Mode transitions rearm two capture and two submitted-frame samples. Markers cover startup memory/allocation, mode before/after, capture before/after, frame acquisition, render and submission. Values are numeric stage metadata; no audio/IQ samples, paths or labels are collected. No heap or storage is used by tracing. Checkpoint denial disables subsequent external trace calls. Callers suppress diagnostics after retained custody. RAM checkpoints do not survive reset; native journal recovery depends on reset support.

Mode transitions allocate no application memory. Audio uses fixed arrays. RF has optional PSRAM allocation at startup; the selected watch API uses bounded resident bulk storage. This is diagnostic instrumentation, not a confirmed crash correction.
