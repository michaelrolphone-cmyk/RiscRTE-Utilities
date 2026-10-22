"""Real subprocess regressions for bounded, non-semantic legacy-tool recovery."""
from pathlib import Path
import json
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
import qualify_t040_neural_guard as q


class TargetOrchestrationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.builder = self.root/'builder.py'
        self.builder.write_text('''import argparse,json,sys
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--state');p.add_argument('--mode');p.add_argument('--code',type=int);p.add_argument('--output-dir');a=p.parse_args()
state=Path(a.state); n=int(state.read_text())+1 if state.exists() else 1;state.write_text(str(n))
out=Path(a.output_dir);out.mkdir();(out/'input.h').write_text('unchanged input\\n');(out/'alias.h').symlink_to(out/'input.h')
abort=a.mode=='abort' or (a.mode=='once' and n==1)
if abort or a.mode in ('ordinary','mixed'):
 log=out/'native/retained-recipe-build.log';log.parent.mkdir()
 text='collect2: fatal error: ld terminated with signal 6 [Aborted], core dumped\\ncompilation terminated.\\ndouble free or corruption (out)\\n'
 if a.mode=='ordinary':text='undefined reference to a_required_symbol\\n'
 if a.mode=='mixed':text+='ordinary compiler failure\\n'
 log.write_text(text);sys.exit(1)
if a.mode!='incomplete':(out/'qualification.json').write_text('{}')
sys.exit(a.code)
''')

    def tearDown(self): self.temp.cleanup()

    def invoke(self, label='parent-frozen-target', mode='once', code=None):
        if code is None: code = q.TARGET_LABELS[label]
        state = self.root/(label+'.state')
        command = [sys.executable, str(self.builder), '--state', str(state), '--mode', mode,
                   '--code', str(code), '--output-dir', str(self.root/'placeholder')]
        folder = self.root/label
        q.run_target(command, self.root, label, folder)
        return folder

    def complete(self):
        folder = self.invoke()
        self.invoke('candidate-frozen-target', 'complete')
        return folder

    def test_exact_abort_retries_fresh_then_preserves_both_protocols(self):
        folder = self.complete()
        q.check_target_attempts(self.root)
        evidence = self.root/'target-attempts/parent-frozen-target'
        attempts = q.read(evidence/'attempts.json')
        self.assertEqual([a['outcome'] for a in attempts], ['legacy-linker-abort','completed'])
        self.assertTrue((evidence/'attempt-1.tar.gz').is_file())
        commands = [q.read(evidence/'runs'/f'parent-frozen-target-attempt-{i}.json')['command'] for i in (1,2)]
        self.assertNotEqual(commands[0][-1], commands[1][-1])
        self.assertEqual(q.normalized_target_command(commands[0]), q.normalized_target_command(commands[1]))
        self.assertTrue(Path(commands[0][-1]).parent.name.startswith('t040-'))
        self.assertFalse(Path(commands[0][-1]).exists())
        self.assertFalse((folder/'alias.h').is_symlink())
        self.assertEqual((folder/'alias.h').read_bytes(), (folder/'input.h').read_bytes())
        # Protocol completion cannot substitute for the unchanged full target validator.
        with self.assertRaises(KeyError): q.validate_target(folder, source='a'*40, parity=True)

    def test_ordinary_failure_is_not_retried(self):
        with self.assertRaisesRegex(ValueError,'accepted complete'): self.invoke(mode='ordinary')
        self.assertEqual((self.root/'parent-frozen-target.state').read_text(), '1')
        self.assertTrue((self.root/'target-attempts/parent-frozen-target/attempt-1.tar.gz').is_file())

    def test_additional_error_is_not_the_exact_allocator_abort(self):
        with self.assertRaises(ValueError): self.invoke(mode='mixed')
        self.assertEqual((self.root/'parent-frozen-target.state').read_text(), '1')

    def test_allocator_failure_is_bounded_to_three_attempts(self):
        with self.assertRaises(ValueError): self.invoke(mode='abort')
        self.assertEqual((self.root/'parent-frozen-target.state').read_text(), '3')
        self.assertEqual(len(list((self.root/'target-attempts/parent-frozen-target').glob('*.tar.gz'))), 3)

    def test_expected_exit_without_receipt_is_not_success(self):
        with self.assertRaises(ValueError): self.invoke(mode='incomplete')
        self.assertEqual((self.root/'parent-frozen-target.state').read_text(), '1')

    def test_candidate_release_pass_is_not_relabelled_required_refusal(self):
        with self.assertRaises(ValueError): self.invoke('candidate-frozen-target', 'complete', code=0)
        self.assertEqual((self.root/'candidate-frozen-target.state').read_text(), '1')

    def test_changed_failed_archive_rejected(self):
        self.complete()
        p = self.root/'target-attempts/parent-frozen-target/attempt-1.tar.gz'
        p.write_bytes(p.read_bytes()+b'changed')
        with self.assertRaisesRegex(ValueError,'archive changed'): q.check_target_attempts(self.root)

    def test_changed_attempt_command_rejected(self):
        self.complete()
        p = self.root/'target-attempts/parent-frozen-target/runs/parent-frozen-target-attempt-1.json'
        record = q.read(p); record['command'].insert(1, '--changed-compiler-or-input')
        q.write(p, record)
        with self.assertRaisesRegex(ValueError,'identity changed'): q.check_target_attempts(self.root)

    def test_missing_attempt_or_relabelled_failure_rejected(self):
        self.complete()
        p = self.root/'target-attempts/parent-frozen-target/attempts.json'
        records = q.read(p); records[0]['outcome'] = 'completed'; q.write(p, records)
        with self.assertRaisesRegex(ValueError,'Ordinary target failure'): q.check_target_attempts(self.root)


if __name__ == '__main__': unittest.main()
