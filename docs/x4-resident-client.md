# Selected X4 resident Contexts

`build_x4_resident_client.py --resident-shell-client` builds Contexts 0.1.6 through the explicitly supplied qualified Utilities helper. It keeps the X4 paper editor source at `e25442fc1f0a2b452c1c671e7670f5af5ad4dae6` unchanged. The separately selected RF-only Contexts service is not rebuilt or replaced here.

The foreground keeps native time, reader flip, completed-frame scrolling, model status, draft/save/discard behavior and launch guards. The shared System bridge provides a renderer-free, read-only applicability view. Host checkpoints own controls and policy, pause/release Contexts before modal handoff, preserve open drafts on policy return, and defer policy polls while capture may be active. Host-only sleep/volume/Wi-Fi/Bluetooth grants are removed from this foreground.

Provide `--utilities`, `--system-apps`, `--runtime`, `--display-sdk`, and a separate `--output`, with the pinned GCC 8.4 compiler in `NATIVE_APP_CC`. The selected helper pins System and Runtime. Existing builders, Watch defaults, application source, and service source remain unchanged.

`test_resident_client.py --build OUTPUT --utilities UTILITIES [--sanitize]` compiles the actual application and System adapter using deterministic provider fixtures. It retains the established raster/flip, capability mask, models, scrolling, immutable frames, draft/save/discard, read ambiguity, launch and cleanup tests. Selected checks cover host controls refusal and pause/release/reopen, draft policy preservation, capture poll deferral, pause/release failure fences, missing/default and malformed DND/brightness/volume, admitted visual/sound output, and terminal preference release.

This is local development evidence, not hardware or product installation qualification. No publication, device, USB transport, or GameBoy work is included.
