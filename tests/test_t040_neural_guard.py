"""Fail-closed tests for the additive, non-distributable T040 evidence lane."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import qualify_t040_neural_guard as q


class GuardEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(); self.root=Path(self.temp.name)
    def tearDown(self): self.temp.cleanup()
    def put(self,name,data='evidence\n'):
        p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(data);return p
    def test_source_and_frozen_files_are_exact_inherited_bytes(self):
        self.assertEqual(q.CAPTURED,'390b8ee8580db7d7b3e37b99c38cb10378d6d413')
        for p,h in q.GUARD_FILES.items(): self.assertEqual(q.digest(ROOT/p),h)
        self.assertEqual(q.digest(ROOT/q.FIXTURE),q.FIXTURE_SHA)
        for p in q.FROZEN_FILES:
            self.assertEqual(q.digest(ROOT/p),q.FROZEN_SOURCE_SHA256[p])
    @mock.patch('consolidated_source_guard.available', new=lambda root: False)
    def test_missing_and_changed_guard_rejected(self):
        with mock.patch.object(q,'git',return_value=b''),mock.patch.object(q,'digest',return_value='0'*64):
            with self.assertRaisesRegex(ValueError,'Captured guard'):q.source_guard()
    @mock.patch('consolidated_source_guard.available', new=lambda root: False)
    def test_changed_frozen_file_rejected(self):
        with mock.patch.object(q,'git',side_effect=[b'',b'M\tApps/audio_spectrum.c\n']):
            with self.assertRaisesRegex(ValueError,'add qualification'):q.source_guard()
    def test_dirty_source_rejected(self):
        with mock.patch.object(q,'git',return_value=b'?? ignored-source.c\n'):
            with self.assertRaisesRegex(ValueError,'clean'):q.source_guard()
    def test_unsafe_paths_rejected(self):
        for p in ('','../x','/tmp/x','a//b','a/./b','a\\b'):
            with self.assertRaises(ValueError):q.safe(self.root,p)
    def test_git_snapshot_preserves_unicode_and_space_paths(self):
        names='lib/μ.h\0lib/spaced name.h\0'.encode()
        with mock.patch.object(q,'git',side_effect=[names,b'first',b'second']) as git:
            q.copy_git(self.root,'pinned',self.root/'snapshot',['lib'])
        self.assertIn('-rz',git.call_args_list[0].args)
        self.assertEqual((self.root/'snapshot/lib/μ.h').read_bytes(),b'first')
        self.assertEqual((self.root/'snapshot/lib/spaced name.h').read_bytes(),b'second')
    def test_empty_git_selection_is_safe(self):
        with mock.patch.object(q,'git',return_value=b''):
            q.copy_git(self.root,'pinned',self.root/'snapshot',['absent'])
        self.assertFalse((self.root/'snapshot').exists())
    def test_external_symlink_rejected(self):
        (self.root/'link').symlink_to('/tmp')
        with self.assertRaises(ValueError):q.safe(self.root,'link/evil')
    def test_inventory_rejects_symlinks(self):
        p=self.put('real');(self.root/'alias').symlink_to(p)
        with self.assertRaisesRegex(ValueError,'symlinks'):q.inventory(self.root)
    def test_inventory_binds_added_removed_changed_bytes(self):
        self.put('a');first=q.inventory(self.root)
        self.put('a','changed');self.assertNotEqual(q.inventory(self.root),first)
        self.put('a');self.put('b');self.assertNotEqual(q.inventory(self.root),first)
        (self.root/'b').unlink();self.assertEqual(q.inventory(self.root),first)
        (self.root/'a').unlink();self.assertNotEqual(q.inventory(self.root),first)
    def test_receipt_is_outside_its_own_inventory(self):
        self.put('a');first=q.inventory(self.root);self.put('candidate-evidence.json','{}')
        self.assertEqual(q.inventory(self.root),first)
    def test_expected_exit_codes_are_exact(self):
        r=q.required_runs();self.assertEqual(len(r),18)
        self.assertEqual({n:v for n,v in r.items() if v}, {
            'runs/candidate-frozen-target.json':1,'runs/candidate-native-parity.json':1,
            'runs/candidate-shared-parity.json':1,'runs/neural-before-normal.json':-6,
            'runs/neural-before-sanitized.json':-6})
    def test_absent_control_rejected(self):
        with self.assertRaisesRegex(ValueError,'Missing, extra'):q.check_runs(self.root,{})
    def test_wrong_exit_and_extra_control_rejected(self):
        for code in (0,-11,134):
            r=q.required_runs();r['runs/neural-before-normal.json']=code
            with self.assertRaisesRegex(ValueError,'wrong-exit'):q.check_runs(self.root,r)
        r=q.required_runs();r['runs/fabricated.json']=0
        with self.assertRaisesRegex(ValueError,'extra'):q.check_runs(self.root,r)
    def neural_logs(self):
        for mode in ('normal','sanitized'):
            self.put('runs/neural-before-'+mode+'.log',
                'shared capture kind 1: neural ticks 0 -> 1839, checkpoint writes 0 -> 1, microphone closes 0 -> 1\nAssertion event_neural.ticks==ticks\n')
            self.put('runs/neural-guarded-'+mode+'.log',
                'Neural app: shared room/event capture and pause fence idle training/promotion/checkpoint; cancel/save resume; unchanged banks and retained cleanup PASS\n')
    def test_complete_neural_controls(self):self.neural_logs();q.check_neural(self.root)
    def test_wrong_baseline_failure_rejected(self):
        self.neural_logs();self.put('runs/neural-before-normal.log','Segmentation fault\n')
        with self.assertRaisesRegex(ValueError,'Wrong baseline'):q.check_neural(self.root)
    def test_truncated_or_sanitizer_neural_log_rejected(self):
        self.neural_logs();self.put('runs/neural-guarded-sanitized.log','PASS\n')
        with self.assertRaisesRegex(ValueError,'complete'):q.check_neural(self.root)
        self.neural_logs();p=self.root/'runs/neural-guarded-sanitized.log';p.write_text(p.read_text()+'runtime error: bad\n')
        with self.assertRaisesRegex(ValueError,'Sanitizer'):q.check_neural(self.root)
    def test_wrong_head_rejected_before_reading_other_evidence(self):
        q.write(self.root/'candidate-evidence.json',{'source_commit':'a'*40,'captured_head':q.CAPTURED,
                'guard_source':q.GUARD_SOURCE,'source_parent':q.PARENT})
        with self.assertRaisesRegex(ValueError,'Wrong source'):q.validate(self.root,'b'*40)
    def test_distributable_claim_rejected(self):
        q.write(self.root/'candidate-evidence.json',{'source_commit':'a'*40,'captured_head':q.CAPTURED,
                'guard_source':q.GUARD_SOURCE,'source_parent':q.PARENT,'qualification_only':True,'distributable':True})
        with self.assertRaisesRegex(ValueError,'Overstated'):q.validate(self.root,'a'*40)
    def test_preserves_65_scenario_host_contract(self):
        self.assertEqual(len(q.frozen_package.SCENARIOS),65)
        self.assertEqual(len(q.frozen_package.PROFILES),5)
        self.assertEqual(len([x for x in q.frozen_package.SCENARIOS if x.startswith('native-full-history-')]),4)
    def test_wrong_host_source_rejected(self):
        q.write(self.root/'host/240x240/qualification.json',{'source_commit':'bad'})
        with self.assertRaisesRegex(ValueError,'Incomplete host'):q.check_hosts(self.root,'good')
    def test_target_baseline_and_external_pins_unchanged(self):
        self.assertEqual(q.frozen.BASE,'12c7440c4c2fd9b6b78e10f11a983947c145b346')
        self.assertEqual(q.SHARED_BASE,'65bffc27f96088db3c2f76ee95ae812b223459f7')
    def test_workflow_has_no_publish_and_exact_transport_digest(self):
        text=(ROOT/'.github/workflows/t040-neural-guard.yml').read_text()
        self.assertIn('contents: read',text);self.assertNotIn('contents: write',text)
        self.assertNotIn('publish_native_spectrum',text)
        self.assertNotIn('--native-manifest',text)
        self.assertIn('pull_request.head.sha || github.sha',text)
        self.assertIn('steps.evidence.outputs.receipt_sha256',text)
        self.assertIn('actions/download-artifact',text)
    def test_runner_never_changes_frozen_baseline(self):
        text=(ROOT/'scripts/qualify_t040_neural_guard.py').read_text()
        self.assertNotIn('frozen.BASE =',text);self.assertNotIn('--publish',text)
        self.assertNotIn('--native-manifest',text)
        self.assertEqual(q.ADDITIONS,{'.github/workflows/t040-neural-guard.yml','scripts/qualify_t040_neural_guard.py',
                                     'tests/test_t040_neural_guard.py','docs/t040/candidate-qualification/README.md'})


if __name__=='__main__':unittest.main()
