#!/usr/bin/env python3
"""Host-only Utilities routing checks using the actual selected System recipe."""
import argparse
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from build_portable_apps import foreground_idle_module, argument_parser, PIN

p=argparse.ArgumentParser()
p.add_argument('--system-apps',type=Path,required=True)
args,unittest_args=p.parse_known_args()
SYSTEM=args.system_apps.resolve()


class UtilitiesForegroundIdleBuildTest(unittest.TestCase):
    def arguments(self, extra=()):
        return argument_parser().parse_args(['--system-apps',str(SYSTEM),'--navigation',*extra])

    def test_omission_does_not_require_new_system_builder(self):
        with tempfile.TemporaryDirectory() as empty:
            self.assertIsNone(foreground_idle_module(Path(empty),self.arguments(),argparse.ArgumentParser()))
        self.assertEqual(PIN,'911be9e8042f1bcc46038fb70189eebe4ca106c5')
        self.assertEqual(self.arguments().system_apps_revision,PIN)

    def test_selected_routes_to_actual_system_recipe(self):
        a=self.arguments(['--foreground-idle-helper-dir','helpers',
                         '--foreground-idle-runtime-dir','runtime',
                         '--foreground-idle-drivers-sdk-dir','drivers',
                         '--foreground-idle-fallback-seconds','15'])
        module=foreground_idle_module(SYSTEM,a,argparse.ArgumentParser())
        self.assertEqual(Path(module.__file__).resolve(),SYSTEM/'scripts/portable_foreground_idle_build.py')
        self.assertEqual(module.SOURCE_NAMES,('foreground_light_sleep.c','foreground_idle_preferences.c'))
        self.assertTrue(module.selected(a))
        self.assertEqual(a.foreground_idle_fallback_seconds,15)

    def test_partial_and_invalid_fallback_are_rejected_by_system(self):
        variants=[['--foreground-idle-fallback-seconds','15'],
                  ['--foreground-idle-helper-dir','helpers']]
        variants += [['--foreground-idle-helper-dir','helpers',
                      '--foreground-idle-runtime-dir','runtime',
                      '--foreground-idle-drivers-sdk-dir','drivers',
                      '--foreground-idle-fallback-seconds',v] for v in ('4','3601')]
        for argv in variants:
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                foreground_idle_module(SYSTEM,self.arguments(argv),argparse.ArgumentParser())

    def test_uncommitted_recipe_cannot_bypass_selected_pin(self):
        a=self.arguments(['--foreground-idle-helper-dir','helpers',
                         '--foreground-idle-runtime-dir','runtime',
                         '--foreground-idle-drivers-sdk-dir','drivers'])
        with patch('build_portable_apps.subprocess.check_output',return_value=b'other committed recipe'), \
                contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
            foreground_idle_module(SYSTEM,a,argparse.ArgumentParser())

    def test_selected_without_shared_builder_is_clear_error(self):
        with tempfile.TemporaryDirectory() as empty:
            with contextlib.redirect_stderr(io.StringIO()),self.assertRaises(SystemExit):
                foreground_idle_module(Path(empty),self.arguments(['--foreground-idle-helper-dir','helpers']),argparse.ArgumentParser())


if __name__=='__main__':
    unittest.main(argv=[sys.argv[0],*unittest_args])
