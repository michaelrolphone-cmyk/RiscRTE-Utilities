# Inherited legacy-overlay witnesses

This is a bounded, byte-preserved historical selection, not a qualification
matrix. It contains **20 original files** and **2 explicitly derived indexes**,
plus this README and the selection inventory. It does not contain the complete
844-file local augmentation, historical binaries, or a complete historical
source checkout. `selection.json` records every selected payload SHA-256 and its
original evidence path; packaging requires this exact inventory and its files.

## Observations and limits

- The first 320×180 touch-driven Alarm run timed out before completing a source
  receipt. Its original PPM shows no visible Dismiss button. The original build
  command, actions and layout are retained. The binary hash remains recorded in
  `baseline.json`, but the binary is not part of this bounded selection. Do not
  describe this first run as source-bound or independently replayed.
- The later Back/Quick run has the unchanged, source-bound receipt. All 145
  compiled-dependency identities and 14 Utilities source hashes are retained in
  it and exactly indexed in `source-identities.json`. These hashes identify the
  observed inputs; the files selected here do not reconstruct their complete
  historical source tree or Python orchestration.
- Its actual Watch Back Alarm case exited 0; Quick controls exited −6 with retained
  invocation state. Both logs, action streams and all eight receipt-bound captures
  are preserved. The receipt's empty `counterexamples` array concerns its narrow
  previsibility detector and does not turn the failed Quick result into a pass.
- The recorded System pin stays `ab901a27c1c5c340b63ac045e0577caeeb56b5d5`.
  No receipt is relabeled to a later public commit. No hardware qualification,
  target execution or comprehensive visual review is claimed.

## Corrected positive matrix

The successor explicitly adds `PORTABLE_ALARM_NATIVE` and `PORTABLE_QUICK_NATIVE`.
The [corrected CI matrix at 9bcf8481](https://github.com/michaelrolphone-cmyk/RiscRTE-Utilities/actions/runs/39200079601)
is referenced by exact receipt hashes in
[`corrected-positive-reference.json`](corrected-positive-reference.json): four
modes, 315 passing cases each, including Alarm and Quick on all seven sizes.
The [tracked workflow](../../../../.github/workflows/native-ble-buttons.yml) and
[actual-adapter runner](../../../../scripts/test_ble_buttons_native.py) require
fresh candidate acceptance evidence at `host/{normal-sync,sanitized-sync,normal-async,sanitized-async}/receipt.json`
and `temporal/{normal,sanitized}/receipt.json` in every qualification bundle.
These historical negative witnesses do not count toward those passing matrices.

## Exact selected payload mapping

Paths below are relative to this folder. Origins are relative to the retained
local evidence collection, not claims of published URLs or historical rebuilds.

| Selected payload | SHA-256 | Original evidence path or derivation |
|---|---|---|
| [`baseline.json`](baseline.json) | `3fdb3adb44b28bfcbf02613e8a8e81b5d8b9cf9e55e0ddd497b5e3c5cdc89991` | native-overlay-counterexamples-baseline/baseline.json |
| [`alarm-no-visible-dismiss-320x180.ppm`](alarm-no-visible-dismiss-320x180.ppm) | `64745dfcb9ca6f59697d75632d7636d18ec1c5cd125636a5b3f63cb214f8ed16` | native-overlay-counterexamples-baseline/alarm-no-visible-dismiss-320x180.ppm |
| [`alarm-touch-actions.txt`](alarm-touch-actions.txt) | `a44baa3e758c726dcaa61ae75bec9aaa672331099e7332b58fb23514a56194a4` | native-overlay-counterexamples-baseline/alarm-touch-actions.txt |
| [`alarm-build-command.json`](alarm-build-command.json) | `28b7b4afc500ee2863c3c1772e61c8138f2fceb6ac43ff5d5851990bf37e5f72` | native-overlay-counterexamples-baseline/alarm-build-command.json |
| [`alarm-layout-320x180.json`](alarm-layout-320x180.json) | `ceeb651ba312dac9d91e811a16d13cc09b295b8961da615750592e6324d053c2` | native-landscape-interruptions-first/layout-320x180.json |
| [`back-and-quick/receipt.json`](back-and-quick/receipt.json) | `bd53aed44daccfe1da5a0525e118fefb8e8d31a4f4066e518cc3ccfd6eff824b` | native-landscape-interruptions-back/receipt.json |
| [`back-and-quick/command.json`](back-and-quick/command.json) | `0e6b018ddb851cde4055901f9106192367843e21e24891bfdccf31f8abc7a8c3` | native-landscape-interruptions-back/command.json |
| [`back-and-quick/layout-320x180.json`](back-and-quick/layout-320x180.json) | `ceeb651ba312dac9d91e811a16d13cc09b295b8961da615750592e6324d053c2` | native-landscape-interruptions-back/layout-320x180.json |
| [`back-and-quick/320x180/alarm/run.log`](back-and-quick/320x180/alarm/run.log) | `ef2095e51b45a192e8876688bca74fa16c761cf9563d227a49ba2be8209d2126` | native-landscape-interruptions-back/320x180/alarm/run.log |
| [`back-and-quick/320x180/alarm/actions.txt`](back-and-quick/320x180/alarm/actions.txt) | `2e95d0629a965dafdb686b7191543eb538bbeb54c26b10c462195fc8ec74003e` | native-landscape-interruptions-back/320x180/alarm/actions.txt |
| [`back-and-quick/320x180/alarm/frame-001.ppm.gz`](back-and-quick/320x180/alarm/frame-001.ppm.gz) | `d601dbfc0aed1ea0942ca93f32bb65934fd35ecc8ea1138d7c100df7999c5899` | native-landscape-interruptions-back/320x180/alarm/frame-001.ppm.gz |
| [`back-and-quick/320x180/alarm/frame-002.ppm.gz`](back-and-quick/320x180/alarm/frame-002.ppm.gz) | `11b5d8ca53df42bb34b2f412f61a3bb84d85e0b81a3f56e70995057e802ce66c` | native-landscape-interruptions-back/320x180/alarm/frame-002.ppm.gz |
| [`back-and-quick/320x180/alarm/frame-003.ppm.gz`](back-and-quick/320x180/alarm/frame-003.ppm.gz) | `e743a4ca0eff9f90241d92792cf37d5b44f8b2bacea89d0a33989ddfd4f4b234` | native-landscape-interruptions-back/320x180/alarm/frame-003.ppm.gz |
| [`back-and-quick/320x180/alarm/frame-004.ppm.gz`](back-and-quick/320x180/alarm/frame-004.ppm.gz) | `f31f62771d8c1cb6556f3fbfe3ec119630eb919d7a5e4d6e4388ba4e1b2cd458` | native-landscape-interruptions-back/320x180/alarm/frame-004.ppm.gz |
| [`back-and-quick/320x180/alarm/frame-005.ppm.gz`](back-and-quick/320x180/alarm/frame-005.ppm.gz) | `e743a4ca0eff9f90241d92792cf37d5b44f8b2bacea89d0a33989ddfd4f4b234` | native-landscape-interruptions-back/320x180/alarm/frame-005.ppm.gz |
| [`back-and-quick/320x180/alarm/frame-006.ppm.gz`](back-and-quick/320x180/alarm/frame-006.ppm.gz) | `de164bab735031d284d8e39c5d57e574c90289abd7baeaf75b340085489f3245` | native-landscape-interruptions-back/320x180/alarm/frame-006.ppm.gz |
| [`back-and-quick/320x180/quick-controls/run.log`](back-and-quick/320x180/quick-controls/run.log) | `c25208b030676c870b5ba9ba2c5a7c1f1ca96242eb1f17794af01478d7aa8119` | native-landscape-interruptions-back/320x180/quick-controls/run.log |
| [`back-and-quick/320x180/quick-controls/actions.txt`](back-and-quick/320x180/quick-controls/actions.txt) | `2c07c4350ac3dc50fa91310091f268b8428cb2834c02920ca151221547d4ebde` | native-landscape-interruptions-back/320x180/quick-controls/actions.txt |
| [`back-and-quick/320x180/quick-controls/frame-001.ppm.gz`](back-and-quick/320x180/quick-controls/frame-001.ppm.gz) | `d601dbfc0aed1ea0942ca93f32bb65934fd35ecc8ea1138d7c100df7999c5899` | native-landscape-interruptions-back/320x180/quick-controls/frame-001.ppm.gz |
| [`back-and-quick/320x180/quick-controls/frame-002.ppm.gz`](back-and-quick/320x180/quick-controls/frame-002.ppm.gz) | `11b5d8ca53df42bb34b2f412f61a3bb84d85e0b81a3f56e70995057e802ce66c` | native-landscape-interruptions-back/320x180/quick-controls/frame-002.ppm.gz |
| [`source-identities.json`](source-identities.json) | `832e5f7842659b6eac8379cd166a81761aa42ebac9ea0735cdd0c2cfeaa0f567` | derived only from unchanged back-and-quick/receipt.json |
| [`corrected-positive-reference.json`](corrected-positive-reference.json) | `a4f8b699030340a742a19d35687644ad631d1a0bbe21f5e8b1dae4907ec90686` | downloaded native-ble-buttons-9bcf8481-ci host receipts and build record |
