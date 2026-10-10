# Preserved native profile witnesses

These files are byte-for-byte copies of the two existing native UTC base
profiles from Utilities commit
`86f10c953bc51b0977d2b517d7565f24fd07cb0d` ("Import exact archived .53
Utilities BLE .18 source baseline"). They also match the profiles at the
UI-decoupling test-repair base `3cc0729f7a6f96460094f3918d6143943dcf2f6a`.
They are test witnesses, not new build profiles or manifest changes.

The previous test attempted to read these paths from
`45cffd33f6d258017cf784ed6b4534654c221736`; that reference does not supply
these files in the recovered source history. The replacement preserves the
whole-file custody assertion using a verifiable baseline that actually contains
both files. Checking witnesses into the test tree removes the dependency on
historical Git objects, including in shallow CI checkouts and source archives.

## Provenance

- Source: `Apps/native-utc-utilities.json`
  - Git blob: `26f8fcf943140e1dda75e904d2eb90a21d20b159`
  - Bytes: 3299
  - SHA-256: `87ac5ab88ff2a8bf60f94ff6331b16f843247310ea28cc12db1dc17350d2f3fc`
- Source: `Apps/native-utc-alarms.json`
  - Git blob: `832dea588009436b35297c0b7a1a7832912a02ba`
  - Bytes: 2612
  - SHA-256: `ab6d0716a9a49d4891c13c4d0ec80c9a3d4bad372d04784a2e7bbf37ffc0815b`

The fixtures were extracted with `git show <source-commit>:<source-path>`;
they were not regenerated or reformatted from the current manifests.
`test_native_paper_motion.py` checks each witness against its pinned SHA-256,
then compares every current profile byte with the witness. Do not refresh these
fixtures merely to accommodate profile drift. The existing opt-in motion
version, grant, launch-guard, and native custody-fence checks remain separate.
