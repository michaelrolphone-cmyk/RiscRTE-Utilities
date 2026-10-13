# Selected source receiver

`CANVAS_FILE_SOURCE` adds an explicitly granted `file.open.source@1` receiver to
the existing Canvas storage adapter. Runtime owns the token and backing bytes.
The adapter uses the basename only to select the parser, clamps reads to the API
bound, accepts short successful reads, and seeks by token offset. It does not
release Runtime's source or retain a caller volume, path authority or handle.
Retained I/O pins the invocation. Normal launch without a source still offers the
built-in cube and saved OBJ/STL choices; those choices clear selected-file state.

`scripts/build_canvas_apps.py --source-handoff` adds the declaration for the
viewer. The production renderer/parser is unchanged. The existing app test now
covers a token-backed file named `builtin.obj`, short reads, normal saved-model
selection, legacy SD reads and presentation retention. `SANITIZERS=undefined`
selects UBSan when the host's ASan runtime is unavailable. No hardware claim.
