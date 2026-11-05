import importlib.util
import json
import io
import tarfile
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location('parity_reference', Path(__file__).resolve().parents[1] / 'scripts/shared_input_parity_reference.py')
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)

class EvolutionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'Apps').mkdir()
        (self.root / 'Apps/example.c').write_bytes(b'int x = 1;\n')
        self.entry = {'path': 'Apps/example.c', 'rationale': 'Explicit fixture change',
                      'before_git_blob_sha1': M.blob_hash(b'int x = 1;\n'),
                      'after_git_blob_sha1': M.blob_hash(b'int x = 2;\n'),
                      'hunks': [{'start': 0, 'old': ['int x = 1;\n'], 'new': ['int x = 2;\n']}]}
    def test_exact_application(self):
        M.apply_entry(self.root, self.entry)
        self.assertEqual((self.root / 'Apps/example.c').read_bytes(), b'int x = 2;\n')
    def test_changed_baseline(self):
        (self.root / 'Apps/example.c').write_text('int x = 3;\n')
        with self.assertRaisesRegex(ValueError, 'Historical input'):
            M.apply_entry(self.root, self.entry)
    def test_changed_hunk(self):
        self.entry['hunks'][0]['old'] = ['int x = 9;\n']
        with self.assertRaisesRegex(ValueError, 'Exact hunk'):
            M.apply_entry(self.root, self.entry)
    def test_changed_output(self):
        self.entry['hunks'][0]['new'] = ['int x = 9;\n']
        with self.assertRaisesRegex(ValueError, 'Reviewed output'):
            M.apply_entry(self.root, self.entry)
    def test_no_path_escape(self):
        for name in ('../example.c', '/tmp/example.c', 'Apps/../../example.c'):
            with self.assertRaises(ValueError):
                M.checked_path(self.root, name)
    def test_token_change_fails(self):
        for after in (b'1\n+\n+\n', b'2\n++\n', b'"changed"\n'):
            with self.assertRaises(AssertionError):
                M.compare_tokens(b'1\n++\n', after, self.root, 'mutation')
        self.assertTrue((self.root / 'parity-mismatch.diff').exists())
    def reference_manifest(self, revision='a'*40, duplicate=False):
        (self.root / '.historical-revision').write_text('a'*40)
        manifest = self.root / 'manifest.json'
        manifest.write_text(json.dumps({'schema': 1, 'reviewed_source_revision': 'b'*40,
            'references': {'test': {'baseline_revision': revision,
                'files': [self.entry] * (2 if duplicate else 1)}}}))
        return manifest
    def archive_bytes(self):
        stream = io.BytesIO()
        with tarfile.open(fileobj=stream, mode='w') as archive:
            raw = b'int x = 1;\n'
            info = tarfile.TarInfo('Apps/example.c')
            info.size = len(raw)
            archive.addfile(info, io.BytesIO(raw))
        return stream.getvalue()
    def test_lexer_mutations(self):
        pairs = [('a++b', 'a+ +b'), ('1.0', '1 . 0'), ('1e+2', '1e + 2'),
                 ('"a b"', '"ab"'), ("'x'", "'y'"), ('a>>=b', 'a> >=b')]
        for before, after in pairs:
            self.assertNotEqual(M.tokenize(before), M.tokenize(after))
        self.assertEqual(M.tokenize('a ++ b'), M.tokenize('a\n++\tb'))
    def test_source_stability(self):
        before = M.source_snapshot(self.root)
        (self.root/'Apps/example.c').write_text('int x = 3;\n')
        with self.assertRaises(AssertionError):
            M.verify_stable(self.root, before)
    def test_wrong_revision(self):
        with patch.object(M, 'MANIFEST', self.reference_manifest('c'*40)):
            with self.assertRaisesRegex(ValueError, 'Wrong immutable'):
                M.Reference(self.root, 'test')
    def test_duplicate_path(self):
        with patch.object(M, 'MANIFEST', self.reference_manifest(duplicate=True)), patch.object(M.subprocess, 'check_output', return_value=self.archive_bytes()):
            with self.assertRaisesRegex(ValueError, 'Duplicate'):
                M.Reference(self.root, 'test')
    def test_baseline_not_modified(self):
        with patch.object(M, 'MANIFEST', self.reference_manifest()), patch.object(M.subprocess, 'check_output', return_value=self.archive_bytes()):
            ref = M.Reference(self.root, 'test')
            self.addCleanup(ref.temporary.cleanup)
            self.assertEqual((self.root/'Apps/example.c').read_text(), 'int x = 1;\n')
            self.assertEqual((ref.path/'Apps/example.c').read_text(), 'int x = 2;\n')
    def test_modified_checkout_is_not_reference(self):
        manifest = self.reference_manifest()
        (self.root/'Apps/example.c').write_text('int x = 999;\n')
        with patch.object(M, 'MANIFEST', manifest), patch.object(M.subprocess, 'check_output', return_value=self.archive_bytes()):
            ref = M.Reference(self.root, 'test')
            self.addCleanup(ref.temporary.cleanup)
            self.assertEqual((ref.path/'Apps/example.c').read_text(), 'int x = 2;\n')

if __name__ == '__main__':
    unittest.main()
