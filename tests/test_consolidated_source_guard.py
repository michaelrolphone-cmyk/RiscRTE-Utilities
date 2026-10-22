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
