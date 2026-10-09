import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('contract', ROOT / 'scripts/check_contexts_contract.py')
contract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)


class ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.current = (ROOT / 'lib/Contexts/include/ContextsServiceV1.h').read_bytes()
        # A committed exact copy of the public dependency is not required:
        # tests receive the same explicitly pinned checkout as the production gate.
        import os
        cls.public = (Path(os.environ['CONTEXTS_SYSTEM_APPS']) /
                      'lib/PortableApps/include/ContextsServiceV1.h').read_bytes()

    def test_selected_pair(self):
        contract.verify(self.current, self.public)

    def test_contract_mutations_refused(self):
        mutations = [
            (b'#define CONTEXTS_AUDIO 1u', b'#define CONTEXTS_AUDIO 4u'),
            (b'bool (*pause)(void *);', b'int32_t (*pause)(void *);'),
            (b'uint32_t sources;', b'uint64_t sources;'),
            (b'bool enabled,awake,audio_allowed,radio_allowed;', b'bool awake,enabled,audio_allowed,radio_allowed;'),
            (b'CONTEXTS_MODEL_UNAVAILABLE', b'CONTEXTS_MODEL_UNAVAILABLE=9'),
            (b', CONTEXTS_MODEL_UNAVAILABLE', b''),
            (b'CONTEXTS_IMPORT_UNAVAILABLE', b'CONTEXTS_IMPORT_UNAVAILABLE, EXTRA'),
            (b'"contexts.service"', b'"contexts.//service"'),
        ]
        for old, new in mutations:
            with self.subTest(old=old):
                self.assertIn(old, self.current)
                with self.assertRaises(ValueError):
                    contract.verify(self.current.replace(old, new), self.public)

    def test_dependency_change_refused(self):
        with self.assertRaises(ValueError):
            contract.verify(self.current, self.public + b'\n')

    def test_comment_only_change(self):
        contract.verify(self.current + b'\n/* harmless comment */\n', self.public)


if __name__ == '__main__':
    unittest.main()
