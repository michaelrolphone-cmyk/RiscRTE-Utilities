# Optional selected-file source receiver

`build_canvas_apps.py --source-handoff` builds Model Viewer 1.3.1 with
`file.open.source@1` in addition to its existing target requirements. The
original `.obj` and `.stl` declarations, built-in model, saved-model picker,
rotation, pan, zoom, shading, touch and navigation remain unchanged. Default
builds remain 1.3.0 without the optional capability.

The receiver gets Runtime's immutable source identity and uses bounded,
seekable `read_at` calls. It never saves the input as `model.obj`, fabricates
an SD path, or allocates the whole selected file. The existing model geometry
limits still apply. Successful short reads are supported, BUSY yields while
checking Home/Back, and RETAINED preserves invocation custody. Runtime owns
scratch cleanup on receiver exit and caller resumption.

Watch's product binds this to its existing private appdata partition, not to a
RAM file. Capacity and flash errors are Runtime responsibilities. A source
larger than available private scratch cannot be staged. No remote range
streaming fallback is currently implemented.

Qualification: actual Canvas entrypoints on Watch and X4, with ASan/UBSan,
cover streamed OBJ, binary STL, 12 MiB+123 bytes, BUSY retry and cancel,
retained read failure, ordinary error cleanup, malformed source names and
normal launch without a handoff. Legacy appdata and SD paths remain in the
same regression run. Hardware touch, radio and real flash power loss remain
physical-device checks.
