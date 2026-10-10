# Selected X4 resident Utilities

`build_x4_resident_clients.py --resident-shell-client` builds the nine explicit X4 native foreground successors. The default builders and their manifests are unchanged. Shared System owns Quick Actions and idle/low-battery policy; each foreground uses its resident descriptor and explicit settled checkpoints, with no renderer, sheet assets, or product sleep implementation.

Sources retain X4 native UTC, reader orientation, telemetry default-off, Stopwatch verified storage, native alarm editors, HID gestures, and Waterfall capture/edit guards from `9bd572791a8304194ceb2b7542fc9cbd124e911b`. The selected unpadded-hours presentation delta from Watch `1684f9da7eb5c954e15fe719bbc9d2f77496421a` / `d6fe74162a19a0d8ceaf5e1321738c2bf5357273` is conditional and extended to the preserved native/paper controllers. No Watch controller replaces an X4 controller.

The build helper pins System `9f9393eaa9a11b32be0758ef4d503fa8583820a5` and Runtime `83cc1f8f661fb39c5028ce669887629de9423bf7`. Pass the existing GCC 8.4.0 2021r2-patch5 compiler via `NATIVE_APP_CC`, and provide `--system-apps`, `--runtime`, `--display-sdk`, and a separate `--output`. An explicit `--system-revision` supports a future clean qualified dependency; `--development-system` marks an interim dirty dependency in the receipt and is not final custody.

The helper is also consumed explicitly by the selected Productivity and Contexts builders. It stages app/Runtime headers independently, checks target imports and exports, validates each ELF, rejects per-app Quick renderer symbols/text, records exact grants and compiled dependencies, and copies license notices. Client grants omit the host's sleep/storage-volume/Quick-radio authority.

Verification:

- `test_resident_clients.py --build OUTPUT [--sanitize]` runs actual Calculator, Stopwatch, Battery, Alarms and Countdown controllers with the real selected System adapter, including native time, save ambiguity/retry, retention, telemetry pause/release failure, and display behavior.
- `test_stopwatch_hours.py --system-apps SYSTEM` tests original and selected formatting, full production draw strings, state/storage preservation, and retention normally and under ASan/UBSan.
- `test_resident_flag_off.py --build OUTPUT --source THIS_TREE --baseline ANCESTOR --output EVIDENCE` compares target ELF bytes with resident/unpadded selections disabled. All nine prior app builds must remain byte-identical. Existing builder/manifest files remain untouched.
- Existing HID, Scanner and Waterfall model/application checks supplement source-byte preservation; the System resident suites own full host-renderer dispatch and capture custody qualification.

These are local development artifacts. No installation, publication, device execution, USB transport, or GameBoy work is performed. Hardware is not qualified.
