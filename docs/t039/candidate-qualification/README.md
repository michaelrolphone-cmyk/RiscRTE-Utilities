# T039 qualification-only lane

This lane tests the exact captured shared Buttons repair without changing or
passing off the frozen `native-ble-buttons-0.1.30-dev1` release contract. Every
existing source file and frozen gate must remain identical to captured head
`39e9247528b39519397e3857d75074dddc4cac82`. Only these four candidate-lane files
may be added. The original PR #88 branch and evidence remain untouched.

## Distinct outcomes

1. The unchanged frozen builder runs on source parent
   `6b2047500905f8f45087cb49989103a926fc5c63` and must pass exact selector-off
   parity. It also runs against the T039 candidate and must fail with the exact
   historical parity refusal. Its original `failure.json`, failed return code
   and `selector_off_exact_baseline_bytes: false` remain in the evidence.
2. Both builds use unchanged pinned external sources and GCC 8.4.0
   `esp-2021r2-patch5`. All three target ELFs must pass structural Runtime
   loader/import/export checks. The sole changed compiled input between source
   parent and candidate must be the captured `Apps/ble_hid_app.inc`.
3. Parent selector-off tokens must match the frozen baseline in all four
   Buttons/Touchpad color/paper compositions. Candidate Buttons tokens must
   differ and candidate Touchpad tokens must remain identical. Native geometry
   executes normally and with ASan/UBSan.
4. Candidate full-feature host controls execute across all seven profiles in
   sync/async and normal/sanitized modes. All 16 temporal cases, including
   retained-custody cases 7 and 10, and mixed-contact case 16 execute normally
   and sanitized. Fifty production FT6336U/Buttons queue controls execute in
   each mode. Existing model/app and color/paper renderer controls also run.
5. A source-bound evidence inventory and SHA-256 receipt are checked after
   artifact upload/download. The verifier receives the producer's receipt
   digest separately, rather than trusting a replacement manifest inside the
   downloaded archive.

## Non-distributable boundary

The output is named `t039-qualification-only-<source SHA>`. It has no new app
version, no installable candidate manifest, no package or release publisher, and
an explicit `NOT_FOR_INSTALLATION.txt`. Target ELF files are compiler/loader
inspection evidence only. Existing manifests included within source snapshots
are provenance, not labels for the changed target bytes.

The original native qualification workflow remains unchanged and remains free
to fail its frozen parity gate for this controller repair. This lane never
suppresses that result or claims release qualification. Hardware, physical
panels/touch, RF, real-host key acceptance, target instruction execution,
deployment and device-fit acceptance remain unqualified.

## Reproduction

Run `scripts/qualify_t039_ble_candidate.py qualify --help` for the exact pinned
checkout inputs. Use a clean committed candidate checkout and a new output
directory. The workflow `.github/workflows/t039-ble-candidate.yml` supplies each
pin explicitly and has read-only permissions with no dispatch/release job.
The workflow's first run, not this document, determines pass/fail status.
