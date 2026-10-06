import copy
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import urllib.error

SPEC = importlib.util.spec_from_file_location('publisher', Path(__file__).parents[1] / 'scripts/publish_watch_101_components.py')
p = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(p)
SHA = 'a' * 40
OTHER = 'b' * 40
REPO = 'michaelrolphone-cmyk/RiscRTE-Utilities'


def config():
    return {'release': 'Watch1.0.1', 'repository': REPO, 'source_sha': SHA,
            'required_workflows': ['.github/workflows/build.yml'],
            'components': [{'kind': 'app', 'id': 'stopwatch', 'version': '0.1.4',
                            'manifest': 'Apps/stopwatch.json'},
                           {'kind': 'service', 'id': 'alarm-service', 'version': '0.4.0',
                            'manifest': 'Services/alarm_service/points-manifest.json'}]}


class API:
    def __init__(self):
        self.refs = {}
        self.created = []
        self.run = {'head_sha': SHA, 'head_branch': 'main', 'event': 'push',
                    'path': '.github/workflows/build.yml', 'run_number': 9,
                    'status': 'completed', 'conclusion': 'success'}
        self.runs = [self.run]
        self.race = False

    def request(self, path, data=None):
        if '/actions/workflows/' in path:
            return {'workflow_runs': self.runs}
        if path.startswith('/git/ref/tags/'):
            return self.refs.get(path.split('/git/ref/tags/')[1])
        if path.startswith('/git/tags/'):
            return {'object': {'type': 'commit', 'sha': SHA}}
        if path == '/git/refs':
            tag = data['ref'].removeprefix('refs/tags/')
            self.refs[tag] = {'object': {'type': 'commit', 'sha': data['sha']}}
            if self.race:
                raise urllib.error.HTTPError('https://api.github.com', 422, 'race', {}, None)
            self.created.append(data)
            return self.refs[tag]
        raise AssertionError(path)


def git(*args):
    if args[0] == 'show':
        if args[1].endswith('stopwatch.json'):
            return '{"version":"0.1.4","file_name":"stopwatch.elf"}'
        return '{"version":"0.4.0","id":"alarm-service"}'
    return SHA


class PublisherTests(unittest.TestCase):
    def test_watch_102_requires_explicit_selection(self):
        c = config()
        c['release'] = 'Watch1.0.2'
        api = API()
        with self.assertRaisesRegex(ValueError, 'Unexpected Watch'):
            p.publish(c, REPO, 'main', api, git)
        self.assertEqual([], api.created)
        p.publish(c, REPO, 'main', api, git, release='Watch1.0.2')
        self.assertEqual(2, len(api.created))
        p.publish(c, REPO, 'main', api, git, release='Watch1.0.2')
        self.assertEqual(2, len(api.created))

    def test_unknown_future_release_is_not_admitted(self):
        c = config()
        c['release'] = 'Watch9.9.9'
        with self.assertRaisesRegex(ValueError, 'Unexpected Watch'):
            p.validate_config(c, REPO, release='Watch9.9.9')

    def test_watch_102_keeps_collision_and_ci_guards(self):
        c = config()
        c['release'] = 'Watch1.0.2'
        api = API()
        api.run['conclusion'] = 'failure'
        with self.assertRaisesRegex(ValueError, 'CI not successful'):
            p.publish(c, REPO, 'main', api, git, release='Watch1.0.2')
        self.assertEqual([], api.created)
        api.run['conclusion'] = 'success'
        api.refs['service-alarm-service-v0.4.0'] = {'object': {'type': 'commit', 'sha': OTHER}}
        with self.assertRaisesRegex(ValueError, 'collision'):
            p.publish(c, REPO, 'main', api, git, release='Watch1.0.2')
        self.assertEqual([], api.created)

    def test_watch_102_manifest_uses_two_verified_workflows(self):
        import json
        c = json.loads((Path(__file__).parents[1] / p.RELEASE_CONFIGS['Watch1.0.2']).read_text())
        tags = p.validate_config(c, REPO, release='Watch1.0.2')
        self.assertEqual(10, len(tags))
        self.assertIn('app-audio_spectrum-v0.4.2', tags)
        self.assertIn('app-ble_scanner-v0.1.0', tags)
        self.assertEqual(c['source_sha'], '186a1a9a44c0286ec3f742de1b7cace8951f1399')
        self.assertEqual(c['required_workflows'], ['.github/workflows/build.yml', '.github/workflows/ble-scanner.yml'])

    def test_create_and_repeat_are_idempotent(self):
        api = API()
        p.publish(config(), REPO, 'main', api, git)
        self.assertEqual(2, len(api.created))
        p.publish(config(), REPO, 'main', api, git)
        self.assertEqual(2, len(api.created))

    def test_preflight_late_collision_creates_nothing(self):
        api = API()
        api.refs['service-alarm-service-v0.4.0'] = {'object': {'type': 'commit', 'sha': OTHER}}
        with self.assertRaisesRegex(ValueError, 'collision'):
            p.publish(config(), REPO, 'main', api, git)
        self.assertEqual([], api.created)

    def test_identical_annotated_tags_are_accepted(self):
        api = API()
        api.refs['app-stopwatch-v0.1.4'] = {'object': {'type': 'tag', 'sha': OTHER}}
        p.publish(config(), REPO, 'main', api, git)
        self.assertEqual(1, len(api.created))

    def test_racing_identical_create_verified(self):
        api = API()
        api.race = True
        p.publish(config(), REPO, 'main', api, git)
        self.assertEqual(2, len(api.refs))

    def test_failed_or_incomplete_ci_blocks(self):
        for status, conclusion in [('completed', 'failure'), ('in_progress', None), ('completed', 'cancelled')]:
            api = API()
            api.run.update(status=status, conclusion=conclusion)
            with self.assertRaisesRegex(ValueError, 'CI not successful'):
                p.publish(config(), REPO, 'main', api, git)
            self.assertEqual([], api.created)

    def test_old_ci_success_does_not_mask_new_failure(self):
        api = API()
        older = dict(api.run, run_number=8)
        api.run['conclusion'] = 'failure'
        api.runs.append(older)
        with self.assertRaises(ValueError):
            p.publish(config(), REPO, 'main', api, git)

    def test_wrong_source_branch_event_or_workflow_does_not_count(self):
        for field, value in [('head_sha', OTHER), ('head_branch', 'feature'), ('event', 'pull_request'), ('path', '.github/workflows/fake.yml')]:
            api = API()
            api.run[field] = value
            with self.assertRaisesRegex(ValueError, 'No default-branch'):
                p.publish(config(), REPO, 'main', api, git)
            self.assertEqual([], api.created)

    def test_wrong_version_or_identity_blocks(self):
        for value in ['{"version":"9.0.0","file_name":"stopwatch.elf"}', '{"version":"0.1.4","file_name":"wrong.elf"}']:
            api = API()
            def bad_git(*args):
                return value if args[0] == 'show' else SHA
            with self.assertRaisesRegex(ValueError, 'mismatch'):
                p.publish(config(), REPO, 'main', api, bad_git)
            self.assertEqual([], api.created)

    def test_unmerged_source_blocks(self):
        api = API()
        def unmerged(*args):
            raise subprocess.CalledProcessError(1, ['git', *args])
        with self.assertRaises(subprocess.CalledProcessError):
            p.publish(config(), REPO, 'main', api, unmerged)
        self.assertEqual([], api.created)

    def test_source_and_metadata_ancestry_are_checked(self):
        calls = []
        def checked(*args):
            calls.append(args)
            return git(*args)
        p.verify_source(config(), 'main', checked)
        self.assertIn(('merge-base', '--is-ancestor', SHA, 'HEAD'), calls)
        self.assertIn(('merge-base', '--is-ancestor', 'HEAD', 'refs/remotes/origin/main'), calls)

    def test_scope_config_validation(self):
        for field, value in [('release', 'Watch1.0.2'), ('source_sha', 'main'), ('repository', 'other/repo'), ('required_workflows', [])]:
            c = config()
            c[field] = value
            with self.assertRaises(ValueError):
                p.validate_config(c, REPO)
        for field, value in [('id', '../bad'), ('manifest', '../secret.json'), ('version', 'main'), ('kind', 'firmware')]:
            c = config()
            c['components'][0][field] = value
            with self.assertRaises(ValueError):
                p.validate_config(c, REPO)
        c = config()
        c['components'].append(copy.deepcopy(c['components'][0]))
        with self.assertRaises(ValueError):
            p.validate_config(c, REPO)

    def test_main_rejects_untrusted_event_before_network(self):
        import json
        for change in [{'event': 'pull_request'}, {'head_branch': 'feature'}, {'head_repository': {'full_name': 'attacker/repo'}}, {'conclusion': 'failure'}]:
            run = {'event': 'push', 'head_branch': 'main', 'head_repository': {'full_name': REPO}, 'status': 'completed', 'conclusion': 'success'}
            run.update(change)
            with tempfile.TemporaryDirectory() as directory:
                event_path = Path(directory) / 'event.json'
                event_path.write_text(json.dumps({'repository': {'default_branch': 'main'}, 'workflow_run': run}))
                with patch.dict('os.environ', {'GITHUB_REPOSITORY': REPO, 'GITHUB_EVENT_NAME': 'workflow_run', 'GITHUB_EVENT_PATH': str(event_path)}):
                    with self.assertRaisesRegex(ValueError, 'Only successful'):
                        p.main()


if __name__ == '__main__':
    unittest.main()
