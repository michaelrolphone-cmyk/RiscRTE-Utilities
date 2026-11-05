#!/usr/bin/env python3
"""Focused, host-only fail-closed contracts for the development Viewer qualifier."""
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('viewer_qualification', HERE / 'build_native_viewer_qualification.py')
q = importlib.util.module_from_spec(spec)
spec.loader.exec_module(q)


class RepositoryContracts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='viewer-contract-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '-q')
        self.git('config', 'user.name', 'Qualification fixture')
        self.git('config', 'user.email', 'qualification@example.invalid')
        (self.root / 'Apps').mkdir()
        (self.root / 'Apps/model_viewer.c').write_text('int viewer;\n')
        (self.root / 'Apps/model_viewer_controls.h').write_text('#pragma once\n')
        (self.root / 'Apps/unrelated.c').write_text('int unrelated;\n')
        (self.root / '.gitignore').write_text('*.ignored\n')
        self.git('add', '.')
        self.git('commit', '-qm', 'Fixture baseline')
        self.pin = self.git('rev-parse', 'HEAD').strip()
        self.paths = ['Apps/model_viewer*']

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args], text=True, stderr=subprocess.PIPE)

    def verify(self):
        return q.verify_repository(self.root, self.pin, self.paths)

    def test_clean_exact_source_and_glob_closure(self):
        self.assertEqual(set(self.verify()), {'Apps/model_viewer.c', 'Apps/model_viewer_controls.h'})

    def test_short_commit_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'full lowercase'):
            q.verify_repository(self.root, self.pin[:8], self.paths)

    def test_wrong_commit_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unsupported repository HEAD'):
            q.verify_repository(self.root, '0' * 40, self.paths)

    def test_modified_relevant_source_is_rejected(self):
        (self.root / 'Apps/model_viewer.c').write_text('int changed;\n')
        with self.assertRaisesRegex(ValueError, 'Dirty'):
            self.verify()

    def test_modified_unrelated_source_is_outside_scope(self):
        (self.root / 'Apps/unrelated.c').write_text('int changed;\n')
        self.verify()

    def test_untracked_relevant_header_is_rejected(self):
        (self.root / 'Apps/model_viewer_injected.h').write_text('int injected;\n')
        with self.assertRaisesRegex(ValueError, 'untracked'):
            self.verify()

    def test_ignored_relevant_file_is_rejected(self):
        (self.root / 'Apps/model_viewer.ignored').write_text('injected\n')
        with self.assertRaisesRegex(ValueError, 'ignored'):
            self.verify()

    def test_staged_relevant_change_is_rejected(self):
        path = self.root / 'Apps/model_viewer.c'
        path.write_text('int changed;\n')
        self.git('add', 'Apps/model_viewer.c')
        path.write_text('int viewer;\n')
        with self.assertRaisesRegex(ValueError, 'Dirty'):
            self.verify()

    def test_assume_unchanged_cannot_hide_modified_bytes(self):
        self.git('update-index', '--assume-unchanged', 'Apps/model_viewer.c')
        (self.root / 'Apps/model_viewer.c').write_text('int changed;\n')
        with self.assertRaisesRegex(ValueError, 'pinned Git bytes'):
            self.verify()

    def test_committed_source_symlink_is_rejected(self):
        path = self.root / 'Apps/model_viewer_controls.h'
        path.unlink()
        path.symlink_to('model_viewer.c')
        self.git('add', '.')
        self.git('commit', '-qm', 'Source symlink fixture')
        self.pin = self.git('rev-parse', 'HEAD').strip()
        with self.assertRaisesRegex(ValueError, 'symlink'):
            self.verify()


class BoundaryContracts(unittest.TestCase):
    def symbol(self, name, section='UND', binding='GLOBAL', visibility='DEFAULT'):
        return f'1: 00000000 0 FUNC {binding} {visibility} {section} {name}\n'

    def valid_symbols(self):
        return self.symbol('risc_runtime_get_api') + self.symbol('app_main', '6')

    def test_only_public_runtime_imports_and_entry_export(self):
        self.assertEqual(q.symbols(self.valid_symbols()), ({'risc_runtime_get_api'}, {'app_main'}))

    def test_private_import_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unexpected module ABI'):
            q.symbols(self.valid_symbols() + self.symbol('private_firmware_api'))

    def test_extra_export_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unexpected module ABI'):
            q.symbols(self.valid_symbols() + self.symbol('canvas_open', '6'))

    def test_undefined_weak_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'binding'):
            q.symbols(self.valid_symbols() + self.symbol('optional_api', binding='WEAK'))

    def test_hidden_global_export_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'visibility'):
            q.symbols(self.valid_symbols() + self.symbol('private_api', '6', visibility='HIDDEN'))

    def test_missing_runtime_import_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unexpected module ABI'):
            q.symbols(self.symbol('app_main', '6'))

    def test_unknown_compiler_bytes_are_rejected(self):
        with tempfile.TemporaryDirectory(prefix='viewer-tool-') as temp:
            fake = Path(temp) / 'xtensa-esp32s3-elf-gcc'
            fake.write_text('#!/bin/sh\necho ' + q.COMPILER_VERSION + '\n')
            with self.assertRaisesRegex(ValueError, 'Unsupported target tool identity'):
                q.verify_tool_digest('gcc', fake)

    def test_strict_flags_never_suppress_warnings_or_link_helpers(self):
        self.assertTrue({'-Wall', '-Wextra', '-Werror', '-nostdlib', '-nostartfiles'} <= set(q.FLAGS))
        self.assertFalse(any(flag.startswith(('-Wno', '-l', '-DPORTABLE')) for flag in q.FLAGS))

    def test_environment_drops_ambient_build_injection(self):
        with patch.dict(os.environ, {'CPATH': '/tmp/host-headers', 'GCC_EXEC_PREFIX': '/tmp/fake',
                                     'CFLAGS': '-w', 'LD_PRELOAD': '/tmp/fake.so'}):
            self.assertFalse({'CPATH', 'GCC_EXEC_PREFIX', 'CFLAGS', 'LD_PRELOAD'} & q.environment().keys())

    def test_owned_dependency_digest_is_recorded(self):
        with tempfile.TemporaryDirectory(prefix='viewer-closure-') as temp:
            path = Path(temp) / 'source.h'
            path.write_bytes(b'owned')
            closure, observed = q.dependency_closure('unit.o: ' + str(path) + '\n',
                                                   {path: 'utilities:Apps/model_viewer.h'}, Path(temp) / 'toolchain')
            self.assertEqual(closure, {'utilities:Apps/model_viewer.h': q.sha(b'owned')})
            self.assertEqual(observed, {path: q.sha(b'owned')})

    def test_pinned_compiler_header_is_accepted(self):
        with tempfile.TemporaryDirectory(prefix='viewer-compiler-header-') as temp:
            path = Path(temp) / 'include/stdint.h'
            path.parent.mkdir()
            path.write_bytes(b'reviewed header')
            with patch.object(q, 'TOOLCHAIN_HEADERS', {'include/stdint.h': q.sha(b'reviewed header')}):
                closure, _ = q.dependency_closure('unit.o: ' + str(path), {}, Path(temp))
            self.assertEqual(closure, {'toolchain:include/stdint.h': q.sha(b'reviewed header')})

    def test_changed_compiler_header_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix='viewer-compiler-header-') as temp:
            path = Path(temp) / 'include/stdint.h'
            path.parent.mkdir()
            path.write_bytes(b'changed header')
            with patch.object(q, 'TOOLCHAIN_HEADERS', {'include/stdint.h': q.sha(b'reviewed header')}):
                with self.assertRaisesRegex(ValueError, 'compiler header identity'):
                    q.dependency_closure('unit.o: ' + str(path), {}, Path(temp))

    def test_unlisted_compiler_header_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix='viewer-compiler-header-') as temp:
            path = Path(temp) / 'injected.h'
            path.write_bytes(b'unlisted')
            with self.assertRaisesRegex(ValueError, 'compiler header identity'):
                q.dependency_closure('unit.o: ' + str(path), {}, Path(temp))

    def test_unowned_dependency_is_rejected(self):
        with tempfile.TemporaryDirectory(prefix='viewer-closure-') as temp:
            path = Path(temp) / 'injected.h'
            path.write_bytes(b'injected')
            with self.assertRaisesRegex(ValueError, 'Unowned'):
                q.dependency_closure('unit.o: ' + str(path), {}, Path(temp) / 'toolchain')

    def test_host_fixture_dependency_is_rejected_even_if_owned(self):
        with tempfile.TemporaryDirectory(prefix='viewer-closure-') as temp:
            path = Path(temp) / 'tests/fixture.h'
            path.parent.mkdir()
            path.write_bytes(b'fixture')
            with self.assertRaisesRegex(ValueError, 'Host fixture'):
                q.dependency_closure('unit.o: ' + str(path), {path: 'utilities:tests/fixture.h'}, Path(temp))

    def test_identical_repeated_bytes_and_closure_pass(self):
        q.verify_repetition(b'elf', {'input': 'hash'}, b'elf', {'input': 'hash'})

    def test_changed_repeated_elf_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'deterministic'):
            q.verify_repetition(b'elf', {'input': 'hash'}, b'different', {'input': 'hash'})

    def test_changed_repeated_closure_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'deterministic'):
            q.verify_repetition(b'elf', {'input': 'hash'}, b'elf', {'input': 'changed'})

    def test_invalid_target_elf_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'ELF32'):
            q.elf_memory(b'host binary')

    def test_stale_output_is_rejected_before_source_checks(self):
        import argparse
        with tempfile.TemporaryDirectory(prefix='viewer-output-') as temp:
            out = Path(temp)
            (out / 'qualification.json').write_text('{}')
            args = argparse.Namespace(output_dir=out, system=Path('/missing-system'),
                                      runtime=Path('/missing-runtime'), utilities_commit='0' * 40)
            with self.assertRaisesRegex(ValueError, 'stale receipts'):
                q.qualify(args)
            self.assertEqual((out / 'qualification.json').read_text(), '{}')

    def test_output_cannot_alias_source_repository(self):
        import argparse
        args = argparse.Namespace(output_dir=q.ROOT / 'build', system=Path('/missing-system'),
                                  runtime=Path('/missing-runtime'), utilities_commit='0' * 40)
        with self.assertRaisesRegex(ValueError, 'separate'):
            q.qualify(args)


if __name__ == '__main__':
    unittest.main(verbosity=2)
