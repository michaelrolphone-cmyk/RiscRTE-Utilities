"""Synthetic fail-closed packaging controls; real target admission is separate."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from types import SimpleNamespace
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_native_ble_buttons_qualification as recipe
import prepare_native_ble_buttons_release as prepare
import publish_native_ble_buttons as publish


def put(path, content=b'fixture\n'):
    path.parent.mkdir(parents=True,exist_ok=True); path.write_bytes(content)
    return prepare.digest(path)


def fixture(root):
    source = 'a'*40
    manifest = prepare.expected_manifest()
    write = prepare.write
    files = {}
    def staged(identity,content=b'fixture\n'):
        owner,name = identity.split(':',1)
        path = root/'source'/owner/name
        sha = put(path,content); files[path.relative_to(root).as_posix()] = sha
        return sha
    closure = {name:staged(name) for name in recipe.REQUIRED|recipe.NATIVE_REQUIRED}
    for name in recipe.REQUIRED|recipe.NATIVE_REQUIRED:
        if name.startswith('system:'):staged(name.replace('system:','system-baseline:',1))
        if name.startswith('utilities:'):staged(name.replace('utilities:','utilities-baseline:',1))
    for name in prepare.EXTRA_SOURCE: staged('utilities:'+name,(ROOT/name).read_bytes())
    for path in sorted((ROOT/prepare.NEGATIVE_WITNESSES).rglob('*')):
        if path.is_file():staged('utilities:'+path.relative_to(ROOT).as_posix(),path.read_bytes())
    manifest_sha = prepare.digest(root/'source/utilities'/prepare.MANIFEST)
    legacy_sha = staged('utilities:Apps/ble_buttons.json',b'legacy manifest\n')
    staged('utilities-baseline:Apps/ble_buttons.json',b'legacy manifest\n')
    script_sha = staged('utilities:scripts/build_native_ble_buttons_qualification.py')
    recipe_sha = staged('system:'+recipe.RECIPE,json.dumps({'ble_buttons':{'command':['cc']}}).encode())
    recipe.RECIPE_SHA256 = recipe_sha
    generated = {}
    for name in recipe.GENERATED_NAMES:
        content = 'fixture '+name+'\n';sha=staged('generated:'+name,content.encode())
        generated[name]={'source':'system:replay-fixtures.json','key':name,'sha256':sha}
    generated_spec={name:{'text':'fixture '+name+'\n','sha256':proof['sha256'],'template_sha256':proof['sha256']} for name,proof in generated.items()}
    generated_sha=staged('system:replay-fixtures.json',json.dumps(generated_spec).encode())
    closure['generated:empty_catalog.c']=generated['empty_catalog.c']['sha256']
    shared_sha = staged('utilities:scripts/test_ble_buttons_native.py',(ROOT/'scripts/test_ble_buttons_native.py').read_bytes())
    modules = []
    for mode in ('baseline','legacy','native'):
        elf = root/'target'/mode/'ble_buttons.elf'
        sha = put(elf,(b'native ELF' if mode == 'native' else b'baseline ELF')*8)
        command_sha = put(elf.parent/'command.json',json.dumps(['cc',*(recipe.NATIVE if mode=='native' else []),
                                                              'source.c','-lgcc','-o',str(elf)]).encode()+b'\n')
        modules.append({'mode':mode,'sha256':sha,'size_bytes':elf.stat().st_size,'source_sha256':closure,
            'command_sha256':command_sha,'structural_loader_admission':'passed','target_instructions_executed':False,
            'native_ui':mode=='native','build_defines':recipe.NATIVE if mode=='native' else [],
            'exports':sorted(recipe.EXPORTS),'imports':['risc_runtime_get_api']})
    target = {'modules':modules,'utilities_base':recipe.BASE,'baseline_system_pin':recipe.BASE_SYSTEM,'utilities_commit':'b'*40,
        'exact_external_git_pins':recipe.PINS,'native_opt_in':True,'native_selectors':recipe.NATIVE,
        'selector_off_exact_baseline_bytes':True,'legacy_manifest_exact_base':True,'native_manifest':manifest,
        'native_manifest_sha256':manifest_sha,'capability_contract':manifest['requires'],
        'legacy_manifest_sha256':legacy_sha,'hardware_qualified':False,'firmware_assembled':False,
        'retained_flags_removed':[], 'historical_recipe_current_deployed_bytes_proven':False,'predecessor_version':'0.1.29',
        'recipe_sources':{'system:'+recipe.RECIPE:recipe_sha,'system:replay-fixtures.json':generated_sha},'generated_sources':generated,'script_sha256':script_sha,'retained_recipe_sha256':recipe_sha}
    write(root/'target/qualification.json',target)
    for mode,sanitized,asynchronous,temporal,scenarios in prepare.CONTROL_RUNS:
        folder=root/mode
        sources={identity:staged(identity) for identity in prepare.HOST_INPUTS}
        results=[];captures={}
        for profile in prepare.PROFILES:
            width,height=map(int,profile.split('x'))
            for scene in sorted(scenarios):
                base=profile+'/'+scene
                log=base+'/run.log'; capture=base+'/frame.ppm.gz'
                log_sha=put(folder/log);captures[capture]=put(folder/capture,b'synthetic pixels')
                results.append({'width':width,'height':height,'scene':scene,'exit_code':0,
                                'log':log,'log_sha256':log_sha,'captures':[capture]})
        write(folder/'receipt.json',{'sanitized':sanitized,'async_rgb565':asynchronous,'temporal':temporal,
            'system_git_pin':recipe.PINS['system'],'source_unchanged_during_run':True,
            'full_profile':True,'full_feature_profile':True,'system_working_tree_allowed':False,'target_qualification_sha256':prepare.digest(root/'target/qualification.json'),
            'hardware_tested':False,'target_instructions_executed':False,'build_defines':recipe.NATIVE,
            'results':results,'required_scenarios':sorted(scenarios),'counterexamples':[],
            'native_pixel_audit':{'title_raster':[1,60,18,100,42],'icon_rasters':{'20':[10,42]}},
            'source_sha256':{},'compiled_dependency_sha256':sources,'capture_sha256':captures})
    write(root/'contracts/qualification.json',{'system_git_pin':recipe.PINS['system'],
        'stable_source_verified':True,'hardware_tested':False,'physical_radio':False,
        'cases':[{'sanitized':mode,'passed':True,'output':' '.join(prepare.PROFILES)} for mode in (False,True)],
        'parity':[{'touchpad':pad,'paper':paper,'identical':True} for pad in (0,1) for paper in (False,True)],
        'source_hashes':{},'compiled_dependency_sha256':{'utilities:Apps/ble_buttons_native_layout.h':closure['utilities:Apps/ble_buttons_native_layout.h']},
        'log_sha256':{name:put(root/'contracts'/name) for name in ('normal.log','sanitized.log')},
        'command_sha256':{'normal-command.json':put(root/'contracts/normal-command.json',b'[]')}})
    runs=[]
    for name,script in [('models','scripts/test_hid_apps.py'),('legacy-renderer','scripts/test_hid_renderer.py'),('paper-renderer','scripts/test_hid_renderer.py')]:
        sha=put(root/(name+'.log'));staged('utilities:'+script)
        runs.append({'id':name,'script':script,'returncode':0,'log':name+'.log','log_sha256':sha})
    write(root/'existing-controls.json',{'legacy_system_commit':prepare.LEGACY_SYSTEM,'runs':runs,'source_files':{},
          'external_sources':{'legacy-system:lib/PortableApps/include/PortableApps.h':staged('legacy-system:lib/PortableApps/include/PortableApps.h')}})
    app = root/prepare.PACKAGE; app.mkdir()
    shutil.copyfile(root/'target/native/ble_buttons.elf',app/'ble_buttons.elf')
    shutil.copyfile(root/'target/native/command.json',app/'command.json')
    shutil.copyfile(root/'source/utilities'/prepare.MANIFEST,app/'ble_buttons.json')
    native = modules[-1]
    layout = {'sha256':native['sha256'],'file_size_bytes':native['size_bytes'],
        'sections':[{'type':8,'flags':3,'size_bytes':8}], 'shf_alloc_size_sum_bytes':24,
        'section_loader':{'executable_allocation_request_bytes':16,'data_allocation_request_bytes':11,
            'resident_section_allocation_request_bytes':27,'data_allocation_components':[{'alignment':4}]}}
    write(app/'elf-layout.json',layout)
    path = 'src/runtime/packages/ElfAdmission.h'
    runtime_sha = put(root/'admission/runtime'/path); staged('runtime:'+path)
    reporter_sha = put(root/'admission/report_native_settings_memory.py'); staged('system:scripts/report_native_settings_memory.py')
    harness_sha = put(root/'admission/admit.cpp')
    result = {'ordinary_application_admitted':True,'missing_import_rejected':True,'unsafe_state_rejected':True,
        'driver_role_rejected':True,'data_allocation_request_bytes':11,'alignment_placements_checked':16}
    write(app/'elf-memory-admission.json',{'runtime_commit':recipe.PINS['runtime'],'elf_sha256':native['sha256'],
        'layout_sha256':prepare.digest(app/'elf-layout.json'),'manifest_sha256':manifest_sha,
        'budgets':prepare.memory_budgets(layout),'target_instructions_executed':False,'linked_device_memory_fit_qualified':False,
        'runs':[{'sanitized':mode,'returncode':0,'result':result} for mode in (False,True)],
        'runtime_source_sha256':{path:runtime_sha},'shared_inspector_source':'system:scripts/report_native_settings_memory.py',
        'shared_inspector_sha256':reporter_sha,'admission_harness_sha256':harness_sha,'unqualified_requirements':['device fit']})
    write(root/'source-provenance.json',{'source_commit':source,'source_tree':'c'*40,'working_tree_dirty':False,'source_files':files})
    write(app/prepare.RECORD,prepare.qualified_record(root,source))
    return source


class BleButtonsReleaseTests(unittest.TestCase):
    def setUp(self):
        self.original_recipe_sha=recipe.RECIPE_SHA256
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        self.source = fixture(self.root)

    def tearDown(self):
        recipe.RECIPE_SHA256=self.original_recipe_sha
        self.temp.cleanup()

    def change(self,name,fn):
        path = self.root/name; data = prepare.read(path); fn(data); prepare.write(path,data)

    def test_complete_synthetic_package(self):
        record = publish.validate(self.root,self.source)
        self.assertEqual(record['version'],'0.1.30-dev1')
        self.assertEqual(len(record['required_capabilities']),16)
        self.assertGreaterEqual(len(prepare.SCENARIOS)*len(prepare.PROFILES)*4,1000)
        self.assertNotEqual(record['target_original_source_commit'],record['source_commit'])

    def test_historical_negative_witnesses_are_required_but_not_passing_cases(self):
        record=publish.validate(self.root,self.source)
        proof=record['historical_negative_witnesses']
        self.assertFalse(proof['counts_toward_passing_matrices'])
        self.assertFalse(proof['first_alarm_source_receipt_completed'])
        self.assertEqual(len(proof['files_sha256']),24)
        path=self.root/proof['source_prefix']/'back-and-quick/receipt.json'
        outcomes={row['scene']:row['exit_code'] for row in prepare.read(path)['results']}
        self.assertEqual(outcomes,{'alarm':0,'quick-controls':-6})

    def test_missing_or_changed_historical_negative_witness_rejected(self):
        folder=self.root/'source/utilities'/prepare.NEGATIVE_WITNESSES
        files=['selection.json','baseline.json','alarm-no-visible-dismiss-320x180.ppm',
               'back-and-quick/receipt.json','back-and-quick/320x180/quick-controls/run.log',
               'back-and-quick/320x180/alarm/frame-004.ppm.gz','source-identities.json','README.md']
        for name in files:
            with self.subTest(name=name):
                path=folder/name;original=path.read_bytes();path.unlink()
                with self.assertRaisesRegex(ValueError,'Missing historical negative'):
                    publish.validate(self.root,self.source)
                path.write_bytes(original)
        path=folder/'back-and-quick/receipt.json';original=path.read_bytes();path.write_bytes(original+b' ')
        with self.assertRaisesRegex(ValueError,'Changed historical negative witness'):
            publish.validate(self.root,self.source)

    def test_untracked_historical_negative_witness_rejected(self):
        name='source/utilities/'+prepare.NEGATIVE_WITNESSES+'/baseline.json'
        self.change('source-provenance.json',lambda p:p['source_files'].pop(name))
        with self.assertRaisesRegex(ValueError,'Missing tracked historical negative witness'):
            publish.validate(self.root,self.source)

    def test_native_manifest_unchanged_and_distinct(self):
        self.assertEqual(prepare.read(ROOT/prepare.MANIFEST),prepare.expected_manifest())
        self.assertNotEqual(prepare.read(ROOT/'Apps/ble_buttons.json')['version'],prepare.VERSION)
        self.assertNotIn('minimum_firmware_version',prepare.expected_manifest())

    def test_wrong_source_rejected(self):
        with self.assertRaisesRegex(ValueError,'source identity'): publish.validate(self.root,'d'*40)

    def test_dirty_source_rejected(self):
        self.change('source-provenance.json',lambda d:d.update(working_tree_dirty=True))
        with self.assertRaisesRegex(ValueError,'source identity'): publish.validate(self.root,self.source)

    def test_each_direct_tamper_rejected(self):
        for name in publish.DIRECT:
            path = self.root/('target' if name=='qualification.json' else prepare.PACKAGE)/name
            content=path.read_bytes();path.write_bytes(content+b' ')
            with self.assertRaises(ValueError,msg=name): publish.validate(self.root,self.source)
            path.write_bytes(content)

    def test_failed_missing_duplicate_or_unsanitized_host_rejected(self):
        path='host/sanitized-async/receipt.json';original=(self.root/path).read_bytes()
        for mutate in (lambda d:d['results'][0].update(exit_code=1),lambda d:d['results'].pop(),
                       lambda d:d['results'].__setitem__(0,d['results'][1]),lambda d:d.update(sanitized=False)):
            self.change(path,mutate)
            with self.assertRaises(ValueError):publish.validate(self.root,self.source)
            (self.root/path).write_bytes(original)

    def test_missing_temporal_profile_or_worktree_evidence_rejected(self):
        path='temporal/sanitized/receipt.json'; original=(self.root/path).read_bytes()
        for mutate in (lambda d:d['results'].pop(),lambda d:d.update(system_working_tree_allowed=True),
                       lambda d:d.update(full_feature_profile=False),lambda d:d.update(temporal=False)):
            self.change(path,mutate)
            with self.assertRaises(ValueError):publish.validate(self.root,self.source)
            (self.root/path).write_bytes(original)

    def test_changed_host_source_rejected(self):
        put(self.root/'source/system/lib/PortableApps/src/adapter.c',b'changed')
        with self.assertRaisesRegex(ValueError,'source evidence'):publish.validate(self.root,self.source)

    def test_missing_compiled_source_rejected(self):
        (self.root/'source/utilities/Apps/ble_buttons.c').unlink()
        with self.assertRaises(FileNotFoundError):publish.validate(self.root,self.source)

    def test_changed_pixels_rejected(self):
        r=prepare.read(self.root/'host/normal-sync/receipt.json')
        capture=next(iter(r['capture_sha256']))
        put(self.root/'host/normal-sync'/capture,b'changed')
        with self.assertRaisesRegex(ValueError,'Changed host capture'):publish.validate(self.root,self.source)

    def test_counterexamples_and_self_declared_reduced_scenarios_rejected(self):
        path='host/normal-sync/receipt.json';original=(self.root/path).read_bytes()
        for mutate in (lambda d:d.update(counterexamples=[{'scene':'previsible-tap'}]),
                       lambda d:d.update(required_scenarios=['idle'],results=[x for x in d['results'] if x['scene']=='idle'])):
            self.change(path,mutate)
            with self.assertRaises(ValueError):publish.validate(self.root,self.source)
            (self.root/path).write_bytes(original)

    def test_missing_or_failed_contract_control_rejected(self):
        self.change('contracts/qualification.json',lambda d:d['parity'][0].update(identical=False))
        with self.assertRaisesRegex(ValueError,'token parity'):publish.validate(self.root,self.source)

    def test_generated_fixture_provenance_rejected(self):
        self.change('source/system/replay-fixtures.json',lambda d:d['empty_catalog.c'].update(text='changed'))
        with self.assertRaises(ValueError):publish.validate(self.root,self.source)

    def test_legacy_elf_difference_rejected(self):
        put(self.root/'target/legacy/ble_buttons.elf',b'changed')
        with self.assertRaises(ValueError):publish.validate(self.root,self.source)

    def test_failed_models_or_changed_log_rejected(self):
        self.change('existing-controls.json',lambda d:d['runs'][0].update(returncode=1))
        with self.assertRaisesRegex(ValueError,'controls failed'):publish.validate(self.root,self.source)

    def test_memory_limits_and_alignment_fail_closed(self):
        original=prepare.read(self.root/'app/elf-layout.json')
        for key in ('executable_allocation_request_bytes','data_allocation_request_bytes','resident_section_allocation_request_bytes'):
            layout=json.loads(json.dumps(original));layout['section_loader'][key]=8*1024*1024+1
            with self.assertRaisesRegex(ValueError,'budget exceeded'):prepare.memory_budgets(layout)
        layout=json.loads(json.dumps(original));layout['sections'][0]['size_bytes']=8*1024*1024+1
        with self.assertRaisesRegex(ValueError,'budget exceeded'):prepare.memory_budgets(layout)
        layout=json.loads(json.dumps(original));layout['section_loader']['data_allocation_components'][0]['alignment']=32
        with self.assertRaisesRegex(ValueError,'alignment residue'):prepare.memory_budgets(layout)

    def test_wrong_runtime_or_hardware_claim_rejected(self):
        path='app/elf-memory-admission.json';original=(self.root/path).read_bytes()
        for mutate in (lambda d:d.update(runtime_commit='d'*40),lambda d:d.update(linked_device_memory_fit_qualified=True),
                       lambda d:d['runs'][1]['result'].update(unsafe_state_rejected=False)):
            self.change(path,mutate)
            with self.assertRaises(ValueError):publish.validate(self.root,self.source)
            (self.root/path).write_bytes(original)

    def test_immutable_tag_and_hashes(self):
        record=publish.validate(self.root,self.source); published=publish.publication_record(record)
        publish.verify_existing(published,self.source,record)
        with self.assertRaises(ValueError):publish.verify_existing(published,'d'*40,record)
        published['elf_sha256']='0'*64
        with self.assertRaises(ValueError):publish.verify_existing(published,self.source,record)

    def assets(self,out,extra=None):
        record=publish.validate(self.root,self.source)
        for name in publish.DIRECT:
            shutil.copyfile(self.root/('target' if name=='qualification.json' else 'app')/name,out/name)
        prepare.write(out/'publication.json',publish.publication_record(record))
        with zipfile.ZipFile(out/(prepare.TAG+'.zip'),'w') as archive:
            for path in self.root.rglob('*'):
                if path.is_file():archive.write(path,path.relative_to(self.root))
            archive.write(out/'publication.json','publication.json')
            if extra:extra(archive)
        (out/'SHA256SUMS').write_text(''.join(prepare.digest(p)+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file()))
        return record

    def test_local_stager_and_nested_output_rejection(self):
        record=publish.validate(self.root,self.source)
        with self.assertRaisesRegex(ValueError,'outside'):publish.stage_assets(self.root,self.root/'assets',record)
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory);publish.stage_assets(self.root,out,record)
            self.assertEqual({p.name for p in out.iterdir()},publish.REQUIRED)

    def test_owned_header_symlinks_are_flattened_and_external_links_rejected(self):
        record=publish.validate(self.root,self.source)
        target=self.root/'source/utilities/Apps/ble_buttons.c'
        link=self.root/'target/owned-header.h';link.symlink_to(target)
        # Restamp immutable evidence after adding this synthetic internal link.
        prepare.write(self.root/'app'/prepare.RECORD,prepare.qualified_record(self.root,self.source))
        record=publish.validate(self.root,self.source)
        with tempfile.TemporaryDirectory() as directory:
            publish.stage_assets(self.root,Path(directory),record)
        link.unlink();link.symlink_to('/etc/passwd')
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError,'escapes evidence'):
                publish.stage_assets(self.root,Path(directory),record)

    def test_ten_asset_roundtrip_and_tamper(self):
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory);record=self.assets(out)
            self.assertEqual(len(publish.REQUIRED),10);publish.verify_release_assets(out,record)
            put(out/'ble_buttons.elf',b'changed')
            with self.assertRaisesRegex(ValueError,'Published asset changed'):publish.verify_release_assets(out,record)

    def test_incomplete_checksums_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory);record=self.assets(out)
            path=out/'SHA256SUMS';path.write_text('\n'.join(path.read_text().splitlines()[1:])+'\n')
            with self.assertRaisesRegex(ValueError,'Incomplete release checksums'):publish.verify_release_assets(out,record)

    def test_archive_traversal_and_duplicate_rejected(self):
        for name in ('../escape','/absolute','app/ble_buttons.elf'):
            with tempfile.TemporaryDirectory() as directory:
                out=Path(directory);record=self.assets(out,lambda archive:archive.writestr(name,b'bad'))
                with self.assertRaises(ValueError):publish.verify_release_assets(out,record)

    def test_archive_symlink_rejected(self):
        def symlink(archive):
            info=zipfile.ZipInfo('link');info.create_system=3;info.external_attr=0o120777<<16;archive.writestr(info,'target')
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory);record=self.assets(out,symlink)
            with self.assertRaisesRegex(ValueError,'symlink'):publish.verify_release_assets(out,record)

    def test_archived_publication_must_match_direct_asset(self):
        for content in (None,b'{}\n'):
            with tempfile.TemporaryDirectory() as directory:
                out=Path(directory);record=self.assets(out)
                archive_path=out/(prepare.TAG+'.zip')
                with zipfile.ZipFile(archive_path) as archive:
                    members=[(item,archive.read(item)) for item in archive.infolist() if item.filename!='publication.json']
                with zipfile.ZipFile(archive_path,'w') as archive:
                    for item,data in members:archive.writestr(item,data)
                    if content is not None:archive.writestr('publication.json',content)
                (out/'SHA256SUMS').write_text(''.join(prepare.digest(p)+'  '+p.name+'\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS'))
                with self.assertRaises((ValueError,FileNotFoundError)):publish.verify_release_assets(out,record)

    def test_workflow_explicit_publication_gate(self):
        workflow=(ROOT/'.github/workflows/native-ble-buttons.yml').read_text();qualify,publisher=workflow.split('  publish-development:',1)
        self.assertNotIn('contents: write',qualify)
        for value in ("github.event_name == 'workflow_dispatch'","github.ref == 'refs/heads/main'",'inputs.publish_development',
                      'needs: qualify','GH_TOKEN: ${{ github.token }}','--publish'):self.assertIn(value,publisher)
        self.assertNotIn("github.event_name == 'push'",publisher)
        for value in ('--run-controls','--validate-only','include-hidden-files: true','native-ble-buttons-downloaded',
                      'name: native-ble-buttons-${{ github.event.pull_request.head.sha || github.sha }}'):self.assertIn(value,qualify)
        self.assertNotIn('--publish',qualify)
        self.assertNotIn('publish_native_countdown.py',workflow)
        for pin in recipe.PINS.values():self.assertIn(pin,workflow)

    def test_workflow_preserves_push_and_pull_request_qualification(self):
        workflow=(ROOT/'.github/workflows/native-ble-buttons.yml').read_text()
        triggers=workflow.split('\npermissions:',1)[0]
        paths="['Apps/ble_*', 'Apps/native/ble_buttons.json', 'scripts/*ble_buttons*', 'scripts/*hid*', 'test/native_apps/*ble_buttons*', 'test/native_apps/hid*', 'tests/*ble_buttons*', 'tests/hid*', 'docs/ble-buttons-native/**', '.github/workflows/native-ble-buttons.yml']"
        self.assertIn('  push:\n    branches: [main, work/ble-buttons-native-20261019]\n    paths: '+paths,triggers)
        self.assertIn('  pull_request:\n    paths: '+paths,triggers)
        self.assertIn('  workflow_dispatch:\n    inputs:\n      publish_development:',triggers)
        self.assertIn('        default: false\n        type: boolean',triggers)

    def test_workflow_explicit_dispatch_event_matrix(self):
        publisher=(ROOT/'.github/workflows/native-ble-buttons.yml').read_text().split('  publish-development:',1)[1]
        condition=next(line.strip()[4:] for line in publisher.splitlines() if line.startswith('    if: '))
        expected="github.ref == 'refs/heads/main' && github.event_name == 'workflow_dispatch' && inputs.publish_development"
        self.assertEqual(condition,expected)
        self.assertIn('\n    needs: qualify\n',publisher)
        self.assertNotIn('always()',publisher)
        self.assertNotIn('continue-on-error:',publisher)
        # Evaluate only the exact, asserted expression above. Successful needs
        # is GitHub's default prerequisite when no status override is present.
        expression=condition.replace('&&','and').replace('||','or')
        for ref in ('refs/heads/main','refs/heads/work/ble-buttons-native-20261019','refs/pull/77/merge'):
            for event in ('push','workflow_dispatch','pull_request','pull_request_target','release'):
                for requested in (False,True):
                    for qualification in ('success','failure','cancelled','skipped'):
                        with self.subTest(ref=ref,event=event,requested=requested,qualification=qualification):
                            allowed=qualification=='success' and eval(expression,{'__builtins__':{}},
                                {'github':SimpleNamespace(ref=ref,event_name=event),
                                 'inputs':SimpleNamespace(publish_development=requested)})
                            expected_allowed=(qualification=='success' and ref=='refs/heads/main' and
                                              event=='workflow_dispatch' and requested)
                            self.assertEqual(allowed,expected_allowed)

    def test_workflow_publisher_binds_qualified_source_and_artifact(self):
        workflow=(ROOT/'.github/workflows/native-ble-buttons.yml').read_text()
        qualify,publisher=workflow.split('  publish-development:',1)
        for value in ('ref: ${{ github.sha }}','name: native-ble-buttons-${{ github.sha }}',
                      '--source-sha "${{ github.sha }}"','--repository "${{ github.repository }}"',
                      'publish_native_ble_buttons.py --artifact dist/native-ble-buttons',
                      'Publish immutable standalone BLE Buttons with asset readback'):
            self.assertIn(value,publisher)
        self.assertIn('--source-sha "${{ github.event.pull_request.head.sha || github.sha }}"',qualify)
        self.assertIn('Validate transported BLE Buttons source and evidence bundle',qualify)
        self.assertIn('default: false',qualify)
        self.assertNotIn('continue-on-error:',workflow)
        self.assertEqual(workflow.count(' --publish'),1)

    def test_existing_published_release_with_different_qualification_stops_before_remote_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            existing_assets=Path(directory)
            existing=self.assets(existing_assets)
            publish.verify_release_assets(existing_assets,existing)
            # Both bundles are valid and keep the same ELF, manifest and source,
            # but new qualification evidence cannot replace the fixed release.
            self.change('target/qualification.json',lambda d:d.update(utilities_commit='d'*40))
            target_sha=prepare.digest(self.root/'target/qualification.json')
            for mode,*_ in prepare.CONTROL_RUNS:
                self.change(mode+'/receipt.json',lambda d:d.update(target_qualification_sha256=target_sha))
            prepare.write(self.root/'app'/prepare.RECORD,prepare.qualified_record(self.root,self.source))
            current=publish.validate(self.root,self.source)
            for key in ('elf_sha256','manifest_sha256','source_commit'):
                self.assertEqual(existing[key],current[key])
            self.assertNotEqual(existing['target_receipt_sha256'],current['target_receipt_sha256'])
            repo='owner/RiscRTE-Utilities';calls=[]
            def remote(*args,check=True):
                calls.append(args)
                if args == ('api','repos/'+repo+'/commits/'+self.source,'--jq','.sha'):
                    stdout=self.source
                elif args == ('release','view',prepare.TAG,'--repo',repo,'--json','tagName,isDraft,isPrerelease,assets'):
                    stdout=json.dumps({'tagName':prepare.TAG,'isDraft':False,'isPrerelease':True,
                                       'assets':[{'name':name} for name in sorted(publish.REQUIRED)]})
                elif args[:6] == ('release','download',prepare.TAG,'--repo',repo,'--dir') and len(args)==7:
                    shutil.copytree(existing_assets,Path(args[6]),dirs_exist_ok=True);stdout=''
                else:
                    self.fail('Unexpected remote call before immutable rejection: '+repr(args))
                return subprocess.CompletedProcess(args,0,stdout,'')
            argv=['publish','--artifact',str(self.root),'--source-sha',self.source,'--repository',repo,'--publish']
            with mock.patch.object(sys,'argv',argv),mock.patch.object(publish,'gh',side_effect=remote), \
                 mock.patch.object(publish,'stage_assets',wraps=publish.stage_assets) as stage:
                with self.assertRaisesRegex(ValueError,'Published qualified artifact differs: qualification.json'):
                    publish.main()
                stage.assert_not_called()
            self.assertEqual([call[:2] for call in calls],[('api','repos/'+repo+'/commits/'+self.source),
                                                         ('release','view'),('release','download')])
            publish.verify_release_assets(existing_assets,existing)

    def publish_with_fake_remote(self, initial_tag=None, create_failure=False, raced_tag=False, asset_failure=False):
        """A draft does not create its Git tag; exercise real CLI orchestration."""
        state = {'tag':initial_tag,'draft':False,'published':False,'calls':[]}
        self.remote_state=state
        repo='owner/RiscRTE-Utilities'
        def remote(*args,check=True):
            state['calls'].append(args)
            ok=True; stdout=''; stderr=''
            if args[:2] == ('api','repos/'+repo+'/commits/'+self.source): stdout=self.source
            elif args[:3] == ('release','view',prepare.TAG):
                if state['published']:
                    stdout=json.dumps({'tagName':prepare.TAG,'isDraft':False,'isPrerelease':True,
                                       'assets':[{'name':name} for name in publish.REQUIRED]})
                else: ok=False;stderr='release not found'
            elif args[:2] == ('api','repos/'+repo+'/git/ref/tags/'+prepare.TAG):
                ok=state['tag'] is not None;stderr='' if ok else 'HTTP 404'
            elif args[:2] == ('api','repos/'+repo+'/git/refs'):
                self.assertIn('ref=refs/tags/'+prepare.TAG,args)
                self.assertIn('sha='+self.source,args)
                self.assertIsNone(state['tag'])
                if create_failure: ok=False;stderr='HTTP 422 ref exists'
                else: state['tag']='d'*40 if raced_tag else self.source
            elif args[:2] == ('api','repos/'+repo+'/commits/'+prepare.TAG):
                ok=state['tag'] is not None;stdout=state['tag'] or '';stderr='' if ok else 'HTTP 404'
            elif args[:3] == ('release','create',prepare.TAG):
                self.assertIn('--verify-tag',args);state['draft']=True
            elif args[:2] in (('release','upload'),('release','download')): self.assertTrue(state['draft'])
            elif args[:3] == ('release','edit',prepare.TAG):
                self.assertTrue(state['draft']);self.assertEqual(state['tag'],self.source);state['published']=True
            else: self.fail('Unexpected remote call: '+repr(args))
            result=subprocess.CompletedProcess(args,0 if ok else 1,stdout,stderr)
            if check and not ok: raise subprocess.CalledProcessError(result.returncode,args,stdout,stderr)
            return result
        argv=['publish','--artifact',str(self.root),'--source-sha',self.source,'--repository',repo,'--publish']
        with mock.patch.object(sys,'argv',argv),mock.patch.object(publish,'gh',side_effect=remote), \
             mock.patch.object(publish,'stage_assets'),mock.patch.object(publish,'verify_release_assets',
                 side_effect=ValueError('asset readback failed') if asset_failure else None):
            publish.main()
        return state

    def test_first_publication_creates_tag_before_draft(self):
        state=self.publish_with_fake_remote()
        self.assertTrue(state['published'])
        calls=state['calls'];create=next(i for i,c in enumerate(calls) if c[:2]==('api','repos/owner/RiscRTE-Utilities/git/refs'))
        draft=next(i for i,c in enumerate(calls) if c[:2]==('release','create'))
        self.assertLess(create,draft)

    def test_existing_matching_tag_is_not_recreated(self):
        state=self.publish_with_fake_remote(self.source)
        self.assertTrue(state['published'])
        self.assertFalse(any(c[:2]==('api','repos/owner/RiscRTE-Utilities/git/refs') for c in state['calls']))

    def test_existing_mismatched_tag_stops_publication(self):
        with self.assertRaisesRegex(ValueError,'another source'):
            self.publish_with_fake_remote('d'*40)
        self.assertFalse(self.remote_state['draft'])

    def test_nested_checkout_uses_most_specific_source_owner(self):
        pins={'utilities':'/work/utilities', 'system':'/work/utilities/.dependencies/system',
              'runtime':'/work/utilities/.dependencies/runtime'}
        self.assertEqual(prepare.source_identity('/work/utilities/.dependencies/system/lib/adapter.c',pins),
                         'system:lib/adapter.c')
        self.assertEqual(prepare.source_identity('/work/utilities/.dependencies/runtime/sdk/api.h',pins),
                         'runtime:sdk/api.h')
        self.assertEqual(prepare.source_identity('/work/utilities/Apps/ble_buttons.c',pins),
                         'utilities:Apps/ble_buttons.c')
        with self.assertRaisesRegex(ValueError,'Unbound'):
            prepare.source_identity('/work/utilities-sibling/Apps/ble_buttons.c',pins)

    def test_nested_ci_relative_bridge_input_uses_pinned_owner(self):
        checkout='/home/runner/work/RiscRTE-Utilities/RiscRTE-Utilities'
        relative='dist/native-ble-buttons/target/inputs/bridges/apps/clock/PortableSleepPolicy.h'
        roots={'utilities':checkout,'bridges':checkout+'/dist/native-ble-buttons/target/inputs/bridges'}
        identity=prepare.source_identity(relative,roots)
        self.assertEqual(identity,'bridges:apps/clock/PortableSleepPolicy.h')
        sha=put(self.root/'source/bridges/apps/clock/PortableSleepPolicy.h',b'pinned bridge bytes')
        receipt_path='host/normal-sync/receipt.json'
        self.change(receipt_path,lambda r:r.update(source_roots=roots,source_sha256={relative:sha}))
        prepare.validate_controls(self.root,prepare.read(self.root/'target/qualification.json'))
        put(self.root/'source/bridges/apps/clock/PortableSleepPolicy.h',b'tampered bridge bytes')
        with self.assertRaisesRegex(ValueError,'Changed source evidence: bridges:'):
            prepare.validate_controls(self.root,prepare.read(self.root/'target/qualification.json'))

    def test_nested_ci_runner_receipt_owns_sdk_and_staged_inputs(self):
        host=prepare.host_control
        checkout=self.root/'checkout';target=checkout/'dist/native-ble-buttons/target'
        system=checkout/'.dependencies/system'
        roots={owner:target/'inputs'/owner for owner in ('utilities','system','runtime','bridges','generated')}
        expected={
            checkout/'Apps/ble_buttons.c':'utilities:Apps/ble_buttons.c',
            roots['bridges']/'apps/clock/PortableSleepPolicy.h':'bridges:apps/clock/PortableSleepPolicy.h',
            roots['system']/'lib/PortableApps/src/adapter.c':'system:lib/PortableApps/src/adapter.c',
            roots['runtime']/'sdk/app/RiscRuntimeV1.h':'runtime:sdk/app/RiscRuntimeV1.h',
            roots['generated']/'watch-alarm-profile.h':'generated:watch-alarm-profile.h',
            roots['utilities']/'lib/Alarm/include/AlarmServiceV1.h':'utilities:lib/Alarm/include/AlarmServiceV1.h',
            system/'Apps/NativeUi.h':'system:Apps/NativeUi.h',
        }
        before={p:put(p,str(p).encode()) for p in expected}
        aggregate=target/'sdk/include/RiscRuntimeV1.h'
        before[aggregate]=put(aggregate,(roots['runtime']/'sdk/app/RiscRuntimeV1.h').read_bytes())
        expected[aggregate]='runtime:sdk/app/RiscRuntimeV1.h'
        identity=lambda p:host.dependency_identity(p,roots=roots,utilities=checkout,system=system,
                                                  catalog=checkout/'dist/host/catalog.c',target=target)
        for path,name in expected.items():self.assertEqual(identity(path),name)
        with mock.patch.object(host,'ROOT',checkout):sources,dependencies=host.receipt_source_hashes(before,identity)
        self.assertEqual(sources,{name.split(':',1)[1]:before[path] for path,name in expected.items() if name.startswith('utilities:')})
        self.assertEqual(dependencies,{name:before[path] for path,name in expected.items()})
        self.assertFalse(any(name.startswith(('dist/','.dependencies/')) for name in sources))
        # The aggregate and pinned Runtime alias have the same hash and collapse
        # to one canonical input, while different bytes must never be matched.
        self.assertEqual(len(dependencies),len(before)-1)
        put(aggregate,b'changed aggregate header')
        with self.assertRaisesRegex(ValueError,'Unowned aggregate header'):
            identity(aggregate)
        with self.assertRaisesRegex(ValueError,'Unowned dependency'):
            identity(self.root/'unowned.h')

    def test_conflicting_staged_and_checkout_source_bytes_are_rejected(self):
        host=prepare.host_control
        paths={self.root/'Apps/a.h':'a'*64,self.root/'dist/inputs/utilities/Apps/a.h':'b'*64}
        with self.assertRaisesRegex(ValueError,'Conflicting consumed source identity: utilities:Apps/a.h'):
            host.receipt_source_hashes(paths,lambda p:'utilities:Apps/a.h')

    def test_actual_producer_module_shape_is_supported(self):
        path='target/qualification.json'
        def producer_shape(target):
            for module in target['modules']:
                module.pop('build_defines');module.pop('native_ui')
        self.change(path,producer_shape)
        prepare.validate_target(self.root)

    def test_failed_or_raced_tag_creation_stops_before_draft(self):
        with self.assertRaises(subprocess.CalledProcessError):self.publish_with_fake_remote(create_failure=True)
        self.assertFalse(self.remote_state['draft'])
        with self.assertRaisesRegex(ValueError,'another source'):self.publish_with_fake_remote(raced_tag=True)
        self.assertFalse(self.remote_state['draft'])

    def test_failed_asset_readback_does_not_publish_draft(self):
        with self.assertRaisesRegex(ValueError,'asset readback failed'):self.publish_with_fake_remote(asset_failure=True)
        self.assertTrue(self.remote_state['draft']);self.assertFalse(self.remote_state['published'])


if __name__ == '__main__':unittest.main()
