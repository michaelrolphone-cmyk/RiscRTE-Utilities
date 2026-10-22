import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'scripts'))
import consolidated_source_guard as guard


class ConsolidationAdmission(unittest.TestCase):
    def setUp(self):
        self.base = {'untouched': {'mode':'100644', 'blob':'0'*40}}
        self.record = {'base':guard.BASE, 'replacement':'controls-power',
            'frozen_baselines_changed':False, 'released':False, 'hardware_qualified':False,
            'source_files':{'Apps/approved.c':{'mode':'100644','blob':'1'*40}}}
        self.current = {**self.base, 'Apps/approved.c':{'mode':'100644','blob':'1'*40},
            f'{guard.DIRECTORY}/controls-power.json':{}, f'{guard.DIRECTORY}/controls-power.md':{}}
    def check(self):
        return guard.validate_delta(self.base,self.current,{'controls-power':self.record})
    def test_exact_source_is_admitted(self): self.assertEqual(len(self.check()),1)
    def test_unrelated_change_is_rejected(self):
        self.current['unreviewed']={'mode':'100644','blob':'2'*40}
        with self.assertRaisesRegex(ValueError,'unadmitted'):self.check()
    def test_deletion_is_rejected(self):
        del self.current['untouched']
        with self.assertRaisesRegex(ValueError,'unadmitted'):self.check()
    def test_blob_change_is_rejected(self):
        self.current['Apps/approved.c']['blob']='2'*40
        with self.assertRaisesRegex(ValueError,'blob/mode'):self.check()
    def test_mode_change_is_rejected(self):
        self.current['Apps/approved.c']['mode']='100755'
        with self.assertRaisesRegex(ValueError,'blob/mode'):self.check()
    def test_documented_resolution_is_exact(self):
        self.record['integration_files']={'Apps/approved.c':{'mode':'100644','blob':'2'*40,
            'original_blob':'1'*40,'reason':'Exact reviewed integration correction'}}
        self.current['Apps/approved.c']['blob']='2'*40
        self.check()
        self.current['Apps/approved.c']['blob']='3'*40
        with self.assertRaises(ValueError):self.check()
    def test_resolution_requires_original_identity_and_reason(self):
        self.record['integration_files']={'Apps/approved.c':{'mode':'100644','blob':'1'*40}}
        with self.assertRaisesRegex(ValueError,'undocumented'):self.check()
    def test_partition_overlap_must_match(self):
        other=copy.deepcopy(self.record);other['replacement']='signal-capture'
        other['source_files']['Apps/approved.c']['blob']='2'*40
        with self.assertRaisesRegex(ValueError,'disagree'):
            guard.admitted_files({'controls-power':self.record,'signal-capture':other})
    def test_missing_metadata_is_rejected(self):
        del self.current[f'{guard.DIRECTORY}/controls-power.json']
        with self.assertRaisesRegex(ValueError,'metadata'):self.check()
    def test_qualification_scope_cannot_expand(self):
        self.record['released']=True
        with self.assertRaisesRegex(ValueError,'scope'):self.check()


class RealGitAdmission(unittest.TestCase):
    def setUp(self):
        import json, subprocess, tempfile
        from unittest import mock
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        def git(*args):
            return subprocess.check_output(['git','-C',str(self.root),*args],text=True,stderr=subprocess.DEVNULL).strip()
        self.git = git
        git('init','-q')
        # Git 2.55 may spawn automatic background maintenance after writes.
        # Keep these disposable repositories hermetic through cleanup.
        git('config','maintenance.auto','false')
        git('config','gc.auto','0')
        git('config','core.fsmonitor','false')
        (self.root/'untouched').write_text('baseline\n')
        self.commit('baseline')
        base=git('rev-parse','HEAD')
        self.patch=mock.patch.object(guard,'BASE',base);self.patch.start();self.addCleanup(self.patch.stop)
        (self.root/'Apps').mkdir();(self.root/'Apps/approved.c').write_text('approved\n')
        self.commit('approved input')
        source=git('rev-parse','HEAD');blob=git('rev-parse','HEAD:Apps/approved.c')
        folder=self.root/guard.DIRECTORY;folder.mkdir(parents=True)
        record={'base':base,'replacement':'controls-power','frozen_baselines_changed':False,
            'released':False,'hardware_qualified':False,'inputs':[{'number':1,'head':source}],
            'source_files':{'Apps/approved.c':{'pr':1,'commit':source,'mode':'100644','blob':blob}}}
        (folder/'controls-power.json').write_text(json.dumps(record))
        (folder/'controls-power.md').write_text('Exact test composition\n')
        self.commit('composition')

    def commit(self, message):
        self.git('add','-A')
        self.git('-c','user.name=Test','-c','user.email=test@example.invalid','commit','-qm',message)

    def check(self):
        return guard.validate(self.root)

    def test_clean_exact_tree(self):
        self.assertEqual(self.check()['source'],self.git('rev-parse','HEAD'))

    def test_assume_unchanged_mutation_rejected(self):
        self.git('update-index','--assume-unchanged','Apps/approved.c')
        (self.root/'Apps/approved.c').write_text('tampered\n')
        with self.assertRaisesRegex(ValueError,'assume-unchanged'):self.check()

    def test_skip_worktree_mutation_rejected(self):
        self.git('update-index','--skip-worktree','Apps/approved.c')
        (self.root/'Apps/approved.c').write_text('tampered\n')
        with self.assertRaisesRegex(ValueError,'skip-worktree'):self.check()

    def test_hidden_untracked_file_rejected(self):
        self.git('config','status.showUntrackedFiles','no')
        (self.root/'Apps/unreviewed.h').write_text('unreviewed\n')
        with self.assertRaisesRegex(ValueError,'clean'):self.check()

    def test_working_mode_checked_when_git_ignores_mode(self):
        self.git('config','core.filemode','false')
        (self.root/'Apps/approved.c').chmod(0o755)
        with self.assertRaisesRegex(ValueError,'blob/mode'):self.check()

    def test_redirecting_git_environment_is_ignored(self):
        import os
        from unittest import mock
        with mock.patch.dict(os.environ,{'GIT_DIR':'/nonexistent','GIT_WORK_TREE':'/nonexistent',
                'GIT_INDEX_FILE':'/nonexistent','GIT_CONFIG_COUNT':'1','GIT_CONFIG_KEY_0':'core.worktree',
                'GIT_CONFIG_VALUE_0':'/nonexistent'}):
            self.check()
            self.assertNotIn('GIT_DIR',os.environ)
            self.assertEqual(os.environ['GIT_NO_REPLACE_OBJECTS'],'1')

    def test_replacement_object_cannot_change_admitted_identity(self):
        original=self.git('rev-parse','HEAD')
        (self.root/'untouched').write_text('substitute\n')
        self.commit('unrelated substitution')
        replacement=self.git('rev-parse','HEAD')
        self.git('checkout','--detach',original)
        self.git('replace',original,replacement)
        self.assertEqual(self.check()['source'],original)
