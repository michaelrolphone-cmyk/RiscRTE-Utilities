#!/usr/bin/env python3
"""Publish reviewed Watch component tags; never move refs or release assets."""
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import urllib.error
import urllib.parse
import urllib.request

RELEASE_CONFIGS = {
    'Watch1.0.1': Path('release/watch-1.0.1-components.json'),
    'Watch1.0.2': Path('release/watch-1.0.2-components.json'),
}
REPOSITORIES = {
    'michaelrolphone-cmyk/RiscRTE-System-Apps',
    'michaelrolphone-cmyk/RiscRTE-Utilities',
    'michaelrolphone-cmyk/RiscRTE-Productivity',
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git(*args):
    return subprocess.check_output(['git', *args], text=True).strip()


class GitHub:
    def __init__(self, repository, token):
        self.base = 'https://api.github.com/repos/' + repository
        self.token = token

    def request(self, path, data=None):
        request = urllib.request.Request(
            self.base + path,
            data=None if data is None else json.dumps(data).encode(),
            headers={'Authorization': 'Bearer ' + self.token,
                     'Accept': 'application/vnd.github+json',
                     'X-GitHub-Api-Version': '2022-11-28'},
        )
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                return json.load(response)
        except urllib.error.HTTPError as error:
            if error.code == 404 and data is None:
                return None
            raise


def validate_config(config, repository, release='Watch1.0.1'):
    require(repository in REPOSITORIES and config['repository'] == repository,
            'Unexpected owning repository')
    require(release in RELEASE_CONFIGS and config['release'] == release,
            'Unexpected Watch component release')
    require(re.fullmatch('[0-9a-f]{40}', config['source_sha']), 'Require immutable source SHA')
    require(config['required_workflows'], 'Require integrated-source CI')
    for path in config['required_workflows']:
        require(re.fullmatch(r'\.github/workflows/[A-Za-z0-9_-]+\.ya?ml', path),
                'Invalid workflow path')
    require(config['components'], 'Empty component manifest')
    tags = []
    for component in config['components']:
        require(component['kind'] in ('app', 'service'), 'Invalid component kind')
        require(re.fullmatch('[a-z0-9][a-z0-9_-]*', component['id']), 'Invalid component ID')
        require(re.fullmatch(r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)',
                             component['version']), 'Invalid component version')
        path = PurePosixPath(component['manifest'])
        require(not path.is_absolute() and '..' not in path.parts and
                str(path).endswith('.json') and ':' not in str(path), 'Invalid manifest path')
        tags.append(f"{component['kind']}-{component['id']}-v{component['version']}")
    require(len(set(tags)) == len(tags), 'Duplicate component tags')
    require(len({(c['kind'], c['id']) for c in config['components']}) == len(tags),
            'Multiple versions of the same component')
    return tags


def verify_source(config, default_branch, git_command=git):
    source = config['source_sha']
    # The checkout is the integrated CI head, with full history and origin refs.
    git_command('merge-base', '--is-ancestor', source, 'HEAD')
    git_command('merge-base', '--is-ancestor', 'HEAD', 'refs/remotes/origin/' + default_branch)
    require(git_command('rev-parse', source + '^{commit}') == source, 'Not a source commit')
    for component in config['components']:
        manifest = json.loads(git_command('show', source + ':' + component['manifest']))
        require(manifest.get('version') == component['version'],
                'Manifest version mismatch: ' + component['manifest'])
        # App manifests historically identify the app by its ELF filename.
        if component['kind'] == 'app':
            require(manifest.get('file_name') == component['id'] + '.elf',
                    'Manifest app identity mismatch: ' + component['manifest'])
        else:
            require(manifest.get('id') == component['id'], 'Manifest service identity mismatch')


def verify_ci(config, default_branch, api):
    for path in config['required_workflows']:
        query = urllib.parse.urlencode({'head_sha': config['source_sha'],
                                        'event': 'push', 'per_page': 100})
        # The endpoint returns newest runs first. Do not mask a newer failed/running
        # attempt with an older success. Each named workflow must have a default push.
        response = api.request('/actions/workflows/' + urllib.parse.quote(path.rsplit('/', 1)[1]) + '/runs?' + query)
        require(response is not None, 'Required CI workflow missing: ' + path)
        runs = [run for run in response['workflow_runs']
                if run['head_sha'] == config['source_sha'] and
                run['head_branch'] == default_branch and run['event'] == 'push' and
                run['path'].split('@')[0] == path]
        require(runs, 'No default-branch push CI for source: ' + path)
        latest = max(runs, key=lambda run: (run['run_number'], run.get('run_attempt', 1)))
        require(latest['status'] == 'completed' and latest['conclusion'] == 'success',
                'Required source CI not successful: ' + path)


def tag_target(api, tag):
    ref = api.request('/git/ref/tags/' + urllib.parse.quote(tag, safe=''))
    if ref is None:
        return None
    obj = ref['object']
    for _ in range(8):
        if obj['type'] == 'commit':
            return obj['sha']
        require(obj['type'] == 'tag', 'Existing tag is not a commit/tag: ' + tag)
        annotated = api.request('/git/tags/' + obj['sha'])
        require(annotated is not None, 'Missing annotated tag object: ' + tag)
        obj = annotated['object']
    raise ValueError('Excessive annotated tag nesting: ' + tag)


def publish(config, repository, default_branch, api, git_command=git, release='Watch1.0.1'):
    tags = validate_config(config, repository, release)
    verify_source(config, default_branch, git_command)
    verify_ci(config, default_branch, api)
    # Preflight ALL refs first so a known later collision never causes partial writes.
    targets = {tag: tag_target(api, tag) for tag in tags}
    for tag, target in targets.items():
        require(target in (None, config['source_sha']), 'Immutable tag collision: ' + tag)
    for tag, target in targets.items():
        if target is None:
            try:
                api.request('/git/refs', {'ref': 'refs/tags/' + tag,
                                          'sha': config['source_sha']})
            except urllib.error.HTTPError as error:
                if error.code != 422:
                    raise
                # A racing identical publisher is safe; no force/update fallback.
                require(tag_target(api, tag) == config['source_sha'],
                        'Tag creation conflict: ' + tag)
        require(tag_target(api, tag) == config['source_sha'], 'Tag verification failed: ' + tag)
        print(tag + ' -> ' + config['source_sha'])


def main():
    repository = os.environ['GITHUB_REPOSITORY']
    event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    default_branch = event['repository']['default_branch']
    run = event['workflow_run']
    require(os.environ['GITHUB_EVENT_NAME'] == 'workflow_run' and
            run['event'] == 'push' and run['head_branch'] == default_branch and
            run['head_repository']['full_name'] == repository and
            run['status'] == 'completed' and run['conclusion'] == 'success',
            'Only successful owning default-branch push CI may publish')
    require(git('rev-parse', 'HEAD') == run['head_sha'], 'Checkout is not the trusted CI head')
    release = os.environ.get('WATCH_COMPONENT_RELEASE', 'Watch1.0.1')
    require(release in RELEASE_CONFIGS, 'Unknown Watch component release')
    config = json.loads(RELEASE_CONFIGS[release].read_text())
    api = GitHub(repository, os.environ['GH_TOKEN'])
    publish(config, repository, default_branch, api, release=release)


if __name__ == '__main__':
    main()
