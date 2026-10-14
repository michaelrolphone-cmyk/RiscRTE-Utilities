# Spectrum retained launch with the full Contexts suffix

The delivered Waveshare 0.1.23 Spectrum policy declared Contexts but no shared-data grants. `cfa_restore` calls `portable_contexts_models_save`; first opening the CFP1 suffix checkpoints Audio namespace 2, RF namespace 3 and rules namespace 4. Namespace 2 was denied first. The native custody adapter correctly retained the invocation on the failed checked acquisition.

The original fixture exposed only the base Contexts ABI. The expanded production-app/adapter fixture exposes the optional suffix and records acquisition order. The denied-policy scenario reproduces the first shared-data denial and asserts zero further Runtime/provider I/O. Successful startup asserts exclusive app/shared-data grants, visits all three namespaces, presents frames, exits and releases resources. Both 240×240 and 410×502 pass normal and ASan/UBSan tests. These are host fixtures, not a hardware qualification.

The System adapter now wires `PORTABLE_CONTEXTS_LIVE` to its terminal custody state and checks that state before polling native exit. Spectrum includes adapter retention in its own temporal fence. Packaging explicitly declares shared-data and grants namespaces 2, 3, 4; Runtime privileges and namespace isolation stay unchanged.

GCC 8.4's LTO linker asserted for this source layout. The preserved-feature non-LTO target build needs all-zero NONE padding removed from the middle as well as the tail of the RELA table. The normalizer preserves every actionable record byte and order, section address and offset; tests check idempotence and refusal of inconsistent DT_RELASZ. The normalized target must pass the ordinary structural and whole-store admission validators.
