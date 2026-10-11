# Reconstructed native Contexts exit

The original local Utilities checkpoint `4fa0fbb` was not available from GitHub or a retrievable archive. This is an independently reconstructed equivalent on published Utilities source `566a15a86709627c6616092e1bbc781756ef3c27`, not that missing Git commit.

Contexts checks Runtime's read-only persistent host exit before and after scene event polling. That request takes priority over unsaved editor/modal/keyboard state and cancels a queued app launch. Existing service pause/close, scene drain, capability release, and failed-custody retention remain the cleanup path. The application cannot acknowledge or clear the native request; Runtime acknowledges only after complete teardown. An unresponsive application still relies on native recovery.

Composition used for validation:
- Runtime `0906ae66ba0b8dcf5b839846d11d3e0c21f82ae2`
- System Apps `0cefe7538a9ce531a81bd0928c164a7f142edfaf`

The production Contexts app and real scene renderer pass twelve UBSan scenarios: Watch and paper profiles each run the existing normal, failed-storage, and terminal-custody cases plus native exit from Home, pending rendering, and shared keyboard. Native exit cases also assert that queued app launch is suppressed and every acquired capability is released. Both Watch and X4 Xtensa scene-client ELFs compile and pass structural validation.

Run `python3 scripts/test_contexts_scene.py --runtime PATH --system-apps PATH --frames OUTPUT`; build both targets with `scripts/build_contexts_scene.py` and `--target watch` or `--target x4`.

These are source and software results. The final Watch cohort must rebuild Contexts, the scene host, affected adapter-linked apps, and the opt-in crown Runtime profile while preserving current application features. No complete Watch image, hardware timing qualification, flash, merge or release is established here.
