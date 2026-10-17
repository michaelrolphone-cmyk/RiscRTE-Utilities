"""Negative controls for immutable Alarms packaging; synthetic evidence is test-only."""
import copy
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_native_alarms_qualification as recipe
import prepare_native_alarms_release as prepare
import publish_native_alarms as publish


def write(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,indent=2)+'\n')
    return recipe.digest(path.read_bytes())


def fixture(root):
    source = 'a'*40
    source_files = {}
    source_sha = recipe.digest(b'synthetic source\n')
    modules = []
    for app in recipe.APPLICATIONS:
        for mode in ('baseline','legacy','native'):
            data = bytearray(52); data[:7] = b'\x7fELF\x01\x01\x01'; data[16:20] = b'\x03\x00\x5e\x00'
            data[-1] = int(mode == 'native')
            dest = root/'target'/mode; dest.mkdir(parents=True,exist_ok=True)
            (dest/(app+'.elf')).write_bytes(data)
            command = recipe.define_arguments(mode == 'native')
            command_sha = write(dest/(app+'-command.json'),command)
            symbols = ''.join('00000000 T '+s+'\n' for s in sorted(recipe.EXPORTS))+' U memcpy\n'
            (dest/(app+'-dynamic-symbols.txt')).write_text(symbols)
            dependencies = recipe.required_closure(app) | (recipe.NATIVE_CLOSURE if mode == 'native' else set())
            modules.append({'app':app,'mode':mode,'native_ui':mode=='native','sha256':recipe.digest(data),'size_bytes':len(data),
                'imports':['memcpy'],'exports':sorted(recipe.EXPORTS),'command_sha256':command_sha,'build_defines':command,
                'source_sha256':{k:source_sha for k in dependencies},'structural_loader_admission':'passed','target_instructions_executed':False})
            if mode == 'native':
                for identity in dependencies:
                    owner,relative = identity.split(':',1)
                    dest_source = root/'source'/owner/relative; dest_source.parent.mkdir(parents=True,exist_ok=True)
                    dest_source.write_bytes(b'synthetic source\n'); source_files[str(dest_source.relative_to(root))] = source_sha
    target = {'utilities_commit':source,'working_tree_dirty':False,'external_git_pins':recipe.PINS,
        'required_capabilities':prepare.expected_manifest()['requires'],'legacy_exact_baseline_bytes':True,
        'legacy_applications':list(recipe.APPLICATIONS),'external_working_trees_used':False,'historical_dot59_elf_reproduction':False,
        'hardware_qualified':False,'firmware_assembled':False,'published':False,'modules':modules}
    target_sha = write(root/'target/qualification.json',target)
    native_sources = {}
    for m in modules:
        if m['mode'] == 'native':native_sources.update(m['source_sha256'])
    # Deliberately synthetic case; never written into the production contract.
    contract = {'schema':1,'temporal':[{'mode':'temporal','applications':list(recipe.APPLICATIONS),
                                    'cases':['synthetic-fixture'],'sizes':[[240,240]]}]}
    contract_sha = write(root/'control-fixture.json',contract)
    coverage = []
    for asynchronous in (False,True):
        for sanitized in (False,True):
            name = ('async' if asynchronous else 'host')+'-'+('sanitized' if sanitized else 'normal')
            triples = [(a,c,w,h) for a in recipe.APPLICATIONS for c in prepare.HOST_CASES for w,h in prepare.HOST_SIZES]
            coverage.append((name,asynchronous,sanitized,triples,None))
    for sanitized in (False,True):
        coverage.append(('temporal-'+('sanitized' if sanitized else 'normal'),True,sanitized,
                         [(a,'synthetic-fixture',240,240) for a in recipe.APPLICATIONS],'temporal'))
    for name,asynchronous,sanitized,triples,mode in coverage:
        results = []
        for index,(app,case,w,h) in enumerate(triples):
            log = root/name/(str(index)+'.log');log.parent.mkdir(parents=True,exist_ok=True);log.write_text('fixture PASS\n')
            results.append({'app':app,'case':case,'width':w,'height':h,'exit_code':0,'log':log.name,'log_sha256':recipe.digest(log.read_bytes())})
        write(root/name/'receipt.json',{'sanitized':sanitized,'async_rgb565':asynchronous,'source_unchanged':True,
            'target_receipt_sha256':target_sha,'required_capabilities':target['required_capabilities'],
            'external_git_pins':recipe.PINS,'mode':mode,'compiled_dependency_sha256':native_sources,'results':results})
    app_dir = root/'app';app_dir.mkdir()
    native = next(m for m in modules if (m['app'],m['mode']) == ('alarms','native'))
    (app_dir/'alarms.elf').write_bytes((root/'target/native/alarms.elf').read_bytes())
    manifest_sha = write(app_dir/'alarms.json',prepare.expected_manifest())
    (app_dir/'command.json').write_bytes((root/'target/native/alarms-command.json').read_bytes())
    result = {k:True for k in ('ordinary_application_admitted','missing_import_rejected','unsafe_state_rejected','driver_role_rejected')}
    result.update(alignment_placements_checked=64,data_allocation_request_bytes=99)
    reporter = root/'admission/report_native_settings_memory.py';reporter.parent.mkdir();reporter.write_text('fixture inspector\n')
    runtime_input = root/'admission/runtime/fixture.c';runtime_input.parent.mkdir();runtime_input.write_text('fixture runtime\n')
    memory = {}
    for app in recipe.APPLICATIONS:
        m = next(m for m in modules if (m['app'],m['mode']) == (app,'native'))
        layout = {'sha256':m['sha256'],'file_size_bytes':m['size_bytes'],'linked_device_memory_fit_qualified':False,
                  'section_loader':{'data_allocation_request_bytes':99}}
        layout_sha = write(root/'admission'/(app+'-elf-layout.json'),layout)
        admission = {'app':app,'runtime_commit':recipe.PINS['runtime'],'runtime_source_sha256':{'fixture.c':recipe.digest(runtime_input.read_bytes())},
            'shared_inspector_sha256':recipe.digest(reporter.read_bytes()),'elf_sha256':m['sha256'],'layout_sha256':layout_sha,
            'required_capabilities':target['required_capabilities'],'linked_device_memory_fit_qualified':False,
            'target_instructions_executed':False,'runs':[{'sanitized':s,'result':copy.deepcopy(result)} for s in (False,True)]}
        if app == 'alarms':admission['manifest_sha256'] = manifest_sha
        admission_sha = write(root/'admission'/(app+'-elf-memory-admission.json'),admission)
        memory[app] = {'layout_sha256':layout_sha,'admission_sha256':admission_sha}
        if app == 'alarms':
            write(app_dir/'elf-layout.json',layout);write(app_dir/'elf-memory-admission.json',admission)
    record = {'source_commit':source,'working_tree_dirty':False,'tag':prepare.TAG,'version':prepare.VERSION,
        'external_git_pins':recipe.PINS,'required_capabilities':target['required_capabilities'],'hardware_qualified':False,
        'firmware_replacement':False,'legacy_manifest_unchanged':True,'target_receipt_sha256':target_sha,
        'controls_profile_sha256':contract_sha,'controls':prepare.validate_controls(root,target),'source_files':source_files,
        'elf_sha256':native['sha256'],'elf_size_bytes':native['size_bytes'],'command_sha256':native['command_sha256'],
        'manifest_sha256':manifest_sha,'layout_sha256':memory['alarms']['layout_sha256'],
        'admission_sha256':memory['alarms']['admission_sha256'],'memory_admissions':memory}
    write(app_dir/'alarms-build-record.json',record)
    return source


class AlarmsReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory();self.root = Path(self.temp.name)
        path = str(self.root/'control-fixture.json')
        self.patches = [mock.patch.object(prepare,'CONTROL_PROFILE',path), mock.patch.object(publish,'CONTROL_PROFILE',path)]
        for p in self.patches:p.start()
        self.source = fixture(self.root)
    def tearDown(self):
        for p in self.patches:p.stop()
        self.temp.cleanup()
    def change(self,path,fn):
        p = self.root/path;r = json.loads(p.read_text());fn(r);return write(p,r)
    def test_complete_synthetic_receipt_passes(self):
        self.assertEqual(publish.validate(self.root,self.source)['version'],'0.3.0')
    def test_unfinished_temporal_contract_fails_closed(self):
        write(self.root/'control-fixture.json',{'schema':1,'temporal':[]})
        with self.assertRaisesRegex(ValueError,'not finalized'):
            prepare.validate_controls(self.root,json.loads((self.root/'target/qualification.json').read_text()))
    def test_wrong_source_rejected(self):
        with self.assertRaisesRegex(ValueError,'source identity'):publish.validate(self.root,'0'*40)
    def test_manifest_distinct_and_fifteen_capabilities(self):
        self.assertEqual(json.loads((ROOT/prepare.MANIFEST).read_text()),prepare.expected_manifest())
        self.assertEqual(len(prepare.expected_manifest()['requires']),15)
        self.assertNotEqual(json.loads((ROOT/'profiles/native-alarms-alarms-baseline-manifest.json').read_text())['version'],prepare.VERSION)
    def test_each_control_required(self):
        for name in json.loads((self.root/'app/alarms-build-record.json').read_text())['controls']:
            p=self.root/name/'receipt.json';data=p.read_bytes();p.unlink()
            with self.assertRaises(FileNotFoundError):publish.validate(self.root,self.source)
            p.write_bytes(data)
    def test_failed_case_rejected(self):
        self.change('host-normal/receipt.json',lambda r:r['results'][0].update(exit_code=1))
        with self.assertRaisesRegex(ValueError,'Failed/incomplete'):publish.validate(self.root,self.source)
    def test_duplicate_case_rejected(self):
        self.change('async-sanitized/receipt.json',lambda r:r['results'].__setitem__(1,r['results'][0]))
        with self.assertRaisesRegex(ValueError,'coverage'):publish.validate(self.root,self.source)
    def test_wrong_geometry_rejected(self):
        self.change('host-normal/receipt.json',lambda r:r['results'][0].update(width=999))
        with self.assertRaisesRegex(ValueError,'coverage'):publish.validate(self.root,self.source)
    def test_source_drift_rejected(self):
        self.change('host-normal/receipt.json',lambda r:r['compiled_dependency_sha256'].update({'utilities:Apps/alarm_app_shared.h':'c'*64}))
        with self.assertRaisesRegex(ValueError,'production source differs'):publish.validate(self.root,self.source)
    def test_missing_full_dependency_rejected(self):
        self.change('temporal-normal/receipt.json',lambda r:r['compiled_dependency_sha256'].pop('runtime:sdk/app/RiscRuntimeV1.h'))
        with self.assertRaisesRegex(ValueError,'Missing full production'):publish.validate(self.root,self.source)
    def test_detached_receipt_rejected(self):
        self.change('host-normal/receipt.json',lambda r:r.update(target_receipt_sha256='0'*64))
        with self.assertRaisesRegex(ValueError,'Unbound control'):publish.validate(self.root,self.source)
    def test_wrong_pin_rejected(self):
        self.change('host-normal/receipt.json',lambda r:r['external_git_pins'].update(system='0'*40))
        with self.assertRaisesRegex(ValueError,'source pins differ'):publish.validate(self.root,self.source)
    def test_log_tamper_rejected(self):
        (self.root/'temporal-normal/0.log').write_text('tampered')
        with self.assertRaisesRegex(ValueError,'Changed control log'):publish.validate(self.root,self.source)
    def test_log_escape_rejected(self):
        self.change('host-normal/receipt.json',lambda r:r['results'][0].update(log='../outside.log'))
        with self.assertRaisesRegex(ValueError,'Invalid evidence path'):publish.validate(self.root,self.source)
    def test_each_direct_artifact_tamper_rejected(self):
        for name in ('alarms.elf','alarms.json','command.json','elf-layout.json','elf-memory-admission.json'):
            p=self.root/'app'/name;data=p.read_bytes();p.write_bytes(data+b' ')
            with self.assertRaises(ValueError):publish.validate(self.root,self.source)
            p.write_bytes(data)
    def test_countdown_baseline_tamper_rejected(self):
        p=self.root/'target/legacy/countdown.elf';p.write_bytes(p.read_bytes()+b'x')
        with self.assertRaisesRegex(ValueError,'Target ELF changed'):publish.validate(self.root,self.source)
    def test_countdown_memory_tamper_rejected(self):
        (self.root/'admission/countdown-elf-memory-admission.json').write_text('{}')
        with self.assertRaisesRegex(ValueError,'Changed memory evidence'):publish.validate(self.root,self.source)
    def test_missing_native_source_bundle_rejected(self):
        self.change('app/alarms-build-record.json',lambda r:r['source_files'].pop('source/utilities/Apps/countdown.c'))
        with self.assertRaisesRegex(ValueError,'Source bundle omits'):publish.validate(self.root,self.source)
    def test_missing_target_variant_rejected(self):
        target=json.loads((self.root/'target/qualification.json').read_text());target['modules'].pop()
        with self.assertRaisesRegex(ValueError,'target matrix'):prepare.validate_target(self.root,target)
    def test_undefined_weak_symbol_rejected(self):
        p=self.root/'target/native/alarms-dynamic-symbols.txt';p.write_text(p.read_text()+' w evil\n')
        with self.assertRaisesRegex(ValueError,'Undefined weak'):publish.validate(self.root,self.source)
    def test_existing_release_requires_exact_source(self):
        record=json.loads((self.root/'app/alarms-build-record.json').read_text())
        published={'tag':prepare.TAG,'source_commit':self.source,'elf_sha256':record['elf_sha256'],
                   'manifest_sha256':record['manifest_sha256'],'build_record_sha256':recipe.digest((self.root/'app/alarms-build-record.json').read_bytes())}
        publish.verify_existing(published,self.source,record)
        with self.assertRaisesRegex(ValueError,'tag/source'):publish.verify_existing(published,'b'*40,record)
    def test_ci_has_no_publication_job_and_pins_exact_dependencies(self):
        text=(ROOT/'.github/workflows/native-alarms.yml').read_text()
        self.assertNotIn('contents: write',text)
        self.assertNotIn('publish-development:',text)
        self.assertIn('--validate-only',text)
        self.assertIn('persist-credentials: false',text)
        for pin in recipe.PINS.values():self.assertIn(pin,text)


if __name__ == '__main__':unittest.main()
