"""Standalone Battery cannot publish missing or detached qualification."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import prepare_native_battery_release as prepare
import publish_native_battery as publish
import build_native_battery_qualification as recipe
import test_battery_native_temporal as temporal
import test_battery_native as reduced


def write(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,indent=2)+'\n')
    return recipe.digest(path.read_bytes())


def fixture(root):
    source='a'*40
    app=root/'app';app.mkdir()
    data=bytearray(52);data[:7]=b'\x7fELF\x01\x01\x01';data[16:20]=b'\x03\x00\x5e\x00'
    (app/'battery.elf').write_bytes(data)
    elf_sha=recipe.digest(data)
    command=recipe.define_arguments(True)
    command_sha=write(app/'command.json',command)
    manifest_sha=write(app/'battery.json',prepare.expected_manifest())
    native={'mode':'native','sha256':elf_sha,'size_bytes':len(data),'build_defines':command,
            'exports':sorted(recipe.EXPORTS),'imports':['memcpy'],'source_sha256':{key:'b'*64 for key in prepare.REDUCED_PRODUCTION_INPUTS}}
    target={'utilities_commit':source,'working_tree_dirty':False,'external_git_pins':recipe.PINS,
            'required_capabilities':prepare.expected_manifest()['requires'],'modules':[native]}
    target_sha=write(root/'target/qualification.json',target)
    cases=[(c,240,240) for c in (0,1,2,3,4,6,7,8,9,10,11,12,13)]
    for mode in ('temporal','snapshot','original','selector-off'):
        for sanitized in (False,True):
            name=mode+'-'+('sanitized' if sanitized else 'normal')
            triples=cases if mode in ('original','selector-off') else cases+[(c,w,h) for c in range(14,25) for w,h in ((240,240),(410,502),(128,128),(320,180))]
            results=[]
            for c,w,h in triples:
                filename=f'{c}-{w}-{h}.log'; path=root/name/filename;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('PASS\n')
                results.append({'case':c,'width':w,'height':h,'exit_code':0,'log':filename,'log_sha256':recipe.digest(path.read_bytes())})
            pins=dict(recipe.PINS)
            if mode=='original':pins['system']='9d2c55b32e88c0b12b1fa16200f399164767f5d1'
            write(root/name/'receipt.json',{'original_failure_control':mode=='original','selector_off':mode=='selector-off',
                  'snapshot':mode=='snapshot','sanitized':sanitized,'target_receipt_sha256':target_sha,'source_unchanged':True,
                  'required_capabilities':target['required_capabilities'],'results':results,'external_git_pins':pins,
                  'compiled_dependency_sha256':native['source_sha256']})
    sizes=((128,128),(160,240),(240,240),(320,180),(410,502),(502,410),(320,480),(480,320),(800,480))
    for asynchronous in (False,True):
        for sanitized in (False,True):
            name=('async' if asynchronous else 'host')+'-'+('sanitized' if sanitized else 'normal')
            write(root/name/'receipt.json',{'async_rgb565':asynchronous,'sanitized':sanitized,'source_unchanged_during_run':True,
                  'results':[{'case':c,'width':w,'height':h} for c in range(33) for w,h in sizes],
                  'compiled_dependency_sha256':native['source_sha256']})
    controls=prepare.validate_controls(root,target)
    layout={'sha256':elf_sha,'file_size_bytes':len(data),'linked_device_memory_fit_qualified':False,
            'section_loader':{'data_allocation_request_bytes':99}}
    layout_sha=write(app/'elf-layout.json',layout)
    result={x:True for x in ('ordinary_application_admitted','missing_import_rejected','unsafe_state_rejected','driver_role_rejected')}
    result.update(alignment_placements_checked=64,data_allocation_request_bytes=99)
    admission={'runtime_commit':recipe.PINS['runtime'],'elf_sha256':elf_sha,'manifest_sha256':manifest_sha,
               'layout_sha256':layout_sha,'linked_device_memory_fit_qualified':False,'target_instructions_executed':False,
               'runs':[{'sanitized':s,'result':copy.deepcopy(result)} for s in (False,True)]}
    admission_sha=write(app/'elf-memory-admission.json',admission)
    sf=root/'source/utilities/fixture.c';sf.parent.mkdir(parents=True);sf.write_text('source\n')
    record={'source_commit':source,'working_tree_dirty':False,'tag':prepare.TAG,'version':prepare.VERSION,
            'external_git_pins':recipe.PINS,'required_capabilities':target['required_capabilities'],
            'hardware_qualified':False,'firmware_replacement':False,'legacy_manifest_unchanged':True,
            'target_receipt_sha256':target_sha,'controls':controls,'elf_sha256':elf_sha,'elf_size_bytes':len(data),
            'manifest_sha256':manifest_sha,'command_sha256':command_sha,'layout_sha256':layout_sha,
            'admission_sha256':admission_sha,'source_files':{'source/utilities/fixture.c':recipe.digest(sf.read_bytes())}}
    write(app/'battery-build-record.json',record)
    return source


class BatteryReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name);self.source=fixture(self.root)
    def tearDown(self):self.temp.cleanup()
    def test_complete_receipt_passes(self):
        self.assertEqual(publish.validate(self.root,self.source)['version'],'1.2.0')
    def test_wrong_source_rejected(self):
        with self.assertRaises(ValueError):publish.validate(self.root,'0'*40)
    def test_manifest_is_distinct_and_fifteen_capabilities(self):
        self.assertEqual(json.loads((ROOT/prepare.MANIFEST).read_text()),prepare.expected_manifest())
        self.assertEqual(len(prepare.expected_manifest()['requires']),15)
        self.assertNotEqual(json.loads((ROOT/'Apps/battery.json').read_text())['version'],prepare.VERSION)
    def test_each_control_is_required(self):
        for name in json.loads((self.root/'app/battery-build-record.json').read_text())['controls']:
            path=self.root/name/'receipt.json';data=path.read_bytes();path.unlink()
            with self.assertRaises(FileNotFoundError):publish.validate(self.root,self.source)
            path.write_bytes(data)
    def test_failed_temporal_case_rejected(self):
        p=self.root/'snapshot-sanitized/receipt.json';r=json.loads(p.read_text());r['results'][0]['exit_code']=1;write(p,r)
        with self.assertRaisesRegex(ValueError,'Failed/incomplete'):publish.validate(self.root,self.source)
    def test_same_count_duplicate_case_rejected(self):
        p=self.root/'temporal-normal/receipt.json';r=json.loads(p.read_text());r['results'][1]=r['results'][0];write(p,r)
        with self.assertRaisesRegex(ValueError,'coverage'):publish.validate(self.root,self.source)
    def test_temporal_production_drift_rejected(self):
        p=self.root/'temporal-sanitized/receipt.json';r=json.loads(p.read_text());r['compiled_dependency_sha256']['utilities:Apps/battery_power.inc']='c'*64;write(p,r)
        with self.assertRaisesRegex(ValueError,'Temporal source differs'):publish.validate(self.root,self.source)
    def test_log_tamper_rejected(self):
        (self.root/'temporal-normal/0-240-240.log').write_text('tampered')
        with self.assertRaisesRegex(ValueError,'Changed temporal log'):publish.validate(self.root,self.source)
    def test_every_direct_artifact_tamper_rejected(self):
        for filename in ('battery.elf','battery.json','command.json','elf-layout.json','elf-memory-admission.json'):
            p=self.root/'app'/filename;data=p.read_bytes();p.write_bytes(data+b' ')
            with self.assertRaises(ValueError):publish.validate(self.root,self.source)
            p.write_bytes(data)
    def test_source_bundle_tamper_rejected(self):
        (self.root/'source/utilities/fixture.c').write_text('changed')
        with self.assertRaisesRegex(ValueError,'Changed source evidence'):publish.validate(self.root,self.source)
    def test_incomplete_admission_rejected_even_with_updated_hash(self):
        p=self.root/'app/elf-memory-admission.json';r=json.loads(p.read_text());r['runs'].pop();sha=write(p,r)
        p=self.root/'app/battery-build-record.json';r=json.loads(p.read_text());r['admission_sha256']=sha;write(p,r)
        with self.assertRaisesRegex(ValueError,'Incomplete production admission'):publish.validate(self.root,self.source)
    def test_empty_ui_dependency_map_rejected(self):
        p=self.root/'host-normal/receipt.json';r=json.loads(p.read_text());r['compiled_dependency_sha256']={};write(p,r)
        with self.assertRaisesRegex(ValueError,'Missing UI production'):publish.validate(self.root,self.source)
    def test_wrong_ui_size_same_count_rejected(self):
        p=self.root/'async-sanitized/receipt.json';r=json.loads(p.read_text());r['results'][0]['width']=999;write(p,r)
        with self.assertRaisesRegex(ValueError,'UI coverage'):publish.validate(self.root,self.source)
    def test_missing_temporal_production_dependency_rejected(self):
        p=self.root/'temporal-normal/receipt.json';r=json.loads(p.read_text());r['compiled_dependency_sha256'].pop('utilities:Apps/battery_power.inc');write(p,r)
        with self.assertRaisesRegex(ValueError,'Missing full temporal'):publish.validate(self.root,self.source)
    def test_existing_release_exact_identity_required(self):
        r=json.loads((self.root/'app/battery-build-record.json').read_text())
        p={'tag':prepare.TAG,'source_commit':self.source,'elf_sha256':r['elf_sha256'],'manifest_sha256':r['manifest_sha256']}
        publish.verify_existing(p,self.source,r)
        for field in ('elf_sha256','manifest_sha256'):
            wrong=dict(p);wrong[field]='0'*64
            with self.assertRaisesRegex(ValueError,'differs from the qualified'):publish.verify_existing(wrong,self.source,r)
    def test_nested_ci_dependency_roots_take_precedence(self):
        root=ROOT; stage=root/'dist/inputs'; roots={owner:stage/owner for owner in recipe.PINS}
        baseline=stage/'utilities-baseline'; original=root/'dist/original/utilities'
        for owner,base in roots.items():
            self.assertEqual(temporal.dependency_owner(base/'example.c',root,roots,baseline),(owner,'example.c'))
        self.assertEqual(temporal.dependency_owner(original/'Apps/battery.c',root,roots,baseline,original),('utilities-original','Apps/battery.c'))
        self.assertEqual(temporal.dependency_owner(baseline/'Alarm.h',root,roots,baseline),('utilities-baseline','Alarm.h'))
        self.assertEqual(temporal.dependency_owner(root/'Apps/battery.c',root,roots,baseline),('utilities','Apps/battery.c'))
        with self.assertRaises(ValueError):temporal.dependency_owner(Path('/unowned/source.c'),root,roots,baseline)
        catalog=root/'dist/host/catalog.c'
        self.assertEqual(reduced.dependency_identity(roots['system']/'adapter.c',roots['system'],catalog),'system:adapter.c')
        self.assertEqual(reduced.dependency_identity(catalog,roots['system'],catalog),'generated:catalog.c')
        self.assertEqual(reduced.dependency_identity(root/'Apps/battery.c',roots['system'],catalog),'utilities:Apps/battery.c')
    def test_ci_is_main_only_and_dependencies_pinned(self):
        text=(ROOT/'.github/workflows/native-battery.yml').read_text()
        self.assertIn("if: github.event_name == 'push' && github.ref == 'refs/heads/main'",text)
        self.assertIn('needs: qualification',text)
        self.assertIn('persist-credentials: false',text)
        for pin in recipe.PINS.values():self.assertIn(pin,text)

if __name__=='__main__':unittest.main()
