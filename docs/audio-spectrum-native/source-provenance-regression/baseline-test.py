"""Synthetic fail-closed packaging controls; real target admission is separate."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import build_native_spectrum_qualification as recipe
import prepare_native_spectrum_release as prepare
import publish_native_spectrum as publish


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
    closure = {name:staged(name) for name in recipe.REQUIRED}
    for name in recipe.REQUIRED:
        if name.startswith('utilities:'):staged(name.replace('utilities:','utilities-baseline:',1))
    for name in prepare.EXTRA_SOURCE: staged('utilities:'+name,(ROOT/name).read_bytes())
    manifest_sha = prepare.digest(root/'source/utilities'/prepare.MANIFEST)
    legacy_sha = staged('utilities:Apps/audio_spectrum.json',b'legacy manifest\n')
    staged('utilities-baseline:Apps/audio_spectrum.json',b'legacy manifest\n')
    script_sha = staged('utilities:scripts/build_native_spectrum_qualification.py')
    recipe_sha = staged('wave:'+recipe.RECIPE)
    shared_sha = staged('utilities:scripts/test_spectrum_shared_input.py')
    modules = []
    for mode in ('baseline','legacy','native'):
        elf = root/'target'/mode/'audio_spectrum.elf'
        sha = put(elf,(b'native ELF' if mode == 'native' else b'baseline ELF')*8)
        command_sha = put(elf.parent/'command.json',b'[]\n')
        modules.append({'mode':mode,'sha256':sha,'size_bytes':elf.stat().st_size,'source_sha256':closure,
            'command_sha256':command_sha,'structural_loader_admission':'passed','target_instructions_executed':False,
            'native_ui':mode=='native','build_defines':recipe.NATIVE if mode=='native' else [],
            'exports':sorted(recipe.EXPORTS),'imports':['risc_runtime_get_api']})
    target = {'modules':modules,'utilities_base':recipe.BASE,'utilities_commit':'b'*40,
        'exact_external_git_pins':recipe.PINS,'native_opt_in':True,'native_selectors':recipe.NATIVE,
        'selector_off_exact_baseline_bytes':True,'legacy_manifest_exact_base':True,'native_manifest':manifest,
        'native_manifest_sha256':manifest_sha,'capability_contract':manifest['requires'],
        'legacy_manifest_sha256':legacy_sha,'hardware_qualified':False,'firmware_assembled':False,
        'retained_flags_removed':['-Wl,--undefined=__divsf3'],'script_sha256':script_sha,'retained_recipe_sha256':recipe_sha}
    write(root/'target/qualification.json',target)
    for profile in prepare.PROFILES:
        folder = root/'host'/profile; sources = {}
        for identity in prepare.HOST_INPUTS:
            owner,name=identity.split(':',1);sources['/old/'+owner+'/'+name] = staged(identity)
        results = []
        for mode in sorted(prepare.SCENARIOS):
            for sanitized in (False,True):
                sub = folder/('sanitized' if sanitized else 'normal')/mode
                put(sub/'run.log'); put(sub/'app.png',b'synthetic pixels')
                results.append({'mode':mode,'sanitized':sanitized,'returncode':0,'log':'/old/host/'+profile+'/'+sub.parent.name+'/'+mode+'/run.log'})
        write(folder/'qualification.json',{'results':results,'added_safety_defines':recipe.NATIVE+['-DSPECTRUM_TEST_WIDTH='+profile.split('x')[0],'-DSPECTRUM_TEST_HEIGHT='+profile.split('x')[1]],
            'source_pins':{'legacy_utilities':{'path':'/old/utilities','commit':'b'*40,'status':['old dirty tree']},
                'system':{'path':'/old/system','commit':recipe.PINS['system'],'status':[]},
                'runtime':{'path':'/old/runtime','commit':recipe.PINS['runtime'],'status':[]}},
            'sources':sources,'test_script_sha256':shared_sha,
            'normal_sanitized_pixel_identical':[name+'/app.png' for name in sorted(prepare.SCENARIOS)],
            'non_keyboard_views':[str(i) for i in range(18)]})
    tests = ('tests/spectrum_native_layout_test.c','test/native_apps/spectrum_native_subpages_test.c')
    write(root/'contracts/qualification.json',{'tests':[{'source':s,'sanitized':m,'returncode':0} for s in tests for m in (False,True)],
        'parity':[{'defines':d,'identical':True} for d in ([],['PORTABLE_NOVA_UI','PORTABLE_ALARM_CLIENT'],['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT'])],
        'sources':{s:staged('utilities:'+s) for s in tests}})
    runs = []
    for name,script in [('models','scripts/test_audio_spectrum.py'),('storage','scripts/test_spectrum_storage_lifecycle.py')]:
        sha = put(root/(name+'.log')); staged('utilities:'+script)
        runs.append({'script':script,'returncode':0,'log':name+'.log','log_sha256':sha})
    write(root/'existing-controls.json',{'system_commit':recipe.PINS['system'],'legacy_system_commit':prepare.LEGACY_SYSTEM,
        'runs':runs,'source_files':{},'external_sources':{owner+':lib/PortableApps/include/PortableApps.h':
            staged(owner+':lib/PortableApps/include/PortableApps.h') for owner in ('system','legacy-system')}})
    app = root/prepare.PACKAGE; app.mkdir()
    shutil.copyfile(root/'target/native/audio_spectrum.elf',app/'audio_spectrum.elf')
    shutil.copyfile(root/'target/native/command.json',app/'command.json')
    shutil.copyfile(root/'source/utilities'/prepare.MANIFEST,app/'audio_spectrum.json')
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


class SpectrumReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.root = Path(self.temp.name)
        self.source = fixture(self.root)

    def tearDown(self): self.temp.cleanup()

    def change(self,name,fn):
        path = self.root/name; data = prepare.read(path); fn(data); prepare.write(path,data)

    def test_complete_synthetic_package(self):
        record = publish.validate(self.root,self.source)
        self.assertEqual(record['version'],'0.5.11-dev1')
        self.assertEqual(len(record['required_capabilities']),21)
        self.assertEqual(len(prepare.SCENARIOS)*len(prepare.PROFILES)*2,610)
        self.assertNotEqual(record['target_original_source_commit'],record['source_commit'])

    def test_native_manifest_unchanged_and_distinct(self):
        self.assertEqual(prepare.read(ROOT/prepare.MANIFEST),prepare.expected_manifest())
        self.assertNotEqual(prepare.read(ROOT/'Apps/audio_spectrum.json')['version'],prepare.VERSION)
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
        path='host/410x502/qualification.json'; original=(self.root/path).read_bytes()
        for mutate in (lambda d:d['results'][0].update(returncode=1),lambda d:d['results'].pop(),
                       lambda d:d['results'].__setitem__(0,d['results'][1]),
                       lambda d:[x.update(sanitized=False) for x in d['results']]):
            self.change(path,mutate)
            with self.assertRaisesRegex(ValueError,'host matrix'):publish.validate(self.root,self.source)
            (self.root/path).write_bytes(original)

    def test_changed_host_source_rejected(self):
        put(self.root/'source/system/Services/scene_host/host.c',b'changed')
        with self.assertRaisesRegex(ValueError,'source evidence'):publish.validate(self.root,self.source)

    def test_missing_compiled_source_rejected(self):
        (self.root/'source/utilities/Apps/audio_spectrum.c').unlink()
        with self.assertRaises(FileNotFoundError):publish.validate(self.root,self.source)

    def test_changed_pixels_rejected(self):
        put(self.root/'host/240x240/normal/native-pages/app.png',b'changed')
        with self.assertRaisesRegex(ValueError,'pixels differ'):publish.validate(self.root,self.source)

    def test_default_parity_failure_rejected(self):
        self.change('contracts/qualification.json',lambda d:d['parity'][0].update(identical=False))
        with self.assertRaisesRegex(ValueError,'parity failed'):publish.validate(self.root,self.source)

    def test_legacy_elf_difference_rejected(self):
        put(self.root/'target/legacy/audio_spectrum.elf',b'changed')
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

    def test_ten_asset_roundtrip_and_tamper(self):
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory);record=self.assets(out)
            self.assertEqual(len(publish.REQUIRED),10);publish.verify_release_assets(out,record)
            put(out/'audio_spectrum.elf',b'changed')
            with self.assertRaisesRegex(ValueError,'Published asset changed'):publish.verify_release_assets(out,record)

    def test_incomplete_checksums_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            out=Path(directory);record=self.assets(out)
            path=out/'SHA256SUMS';path.write_text('\n'.join(path.read_text().splitlines()[1:])+'\n')
            with self.assertRaisesRegex(ValueError,'Incomplete release checksums'):publish.verify_release_assets(out,record)

    def test_archive_traversal_and_duplicate_rejected(self):
        for name in ('../escape','/absolute','app/audio_spectrum.elf'):
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
        workflow=(ROOT/'.github/workflows/native-spectrum.yml').read_text();qualify,publisher=workflow.split('  publish-development:',1)
        self.assertNotIn('contents: write',qualify)
        for value in ("github.event_name == 'workflow_dispatch'","github.event_name == 'push'","github.ref == 'refs/heads/main'",'inputs.publish_development',
                      'needs: qualify','GH_TOKEN: ${{ github.token }}','--publish'):self.assertIn(value,publisher)
        for value in ('--run-controls','--validate-only','include-hidden-files: true','native-spectrum-downloaded',
                      'name: native-spectrum-${{ github.event.pull_request.head.sha || github.sha }}'):self.assertIn(value,qualify)
        self.assertNotIn('publish_native_countdown.py',workflow)
        for pin in recipe.PINS.values():self.assertIn(pin,workflow)

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
        pins={'legacy_utilities':{'path':'/work/utilities'},
              'system':{'path':'/work/utilities/.dependencies/system'},
              'runtime':{'path':'/work/utilities/.dependencies/runtime'}}
        self.assertEqual(prepare.source_identity('/work/utilities/.dependencies/system/lib/adapter.c',pins),
                         'system:lib/adapter.c')
        self.assertEqual(prepare.source_identity('/work/utilities/.dependencies/runtime/sdk/api.h',pins),
                         'runtime:sdk/api.h')
        self.assertEqual(prepare.source_identity('/work/utilities/Apps/audio_spectrum.c',pins),
                         'utilities:Apps/audio_spectrum.c')
        with self.assertRaisesRegex(ValueError,'Unbound'):
            prepare.source_identity('/work/utilities-sibling/Apps/audio_spectrum.c',pins)

    def test_failed_or_raced_tag_creation_stops_before_draft(self):
        with self.assertRaises(subprocess.CalledProcessError):self.publish_with_fake_remote(create_failure=True)
        self.assertFalse(self.remote_state['draft'])
        with self.assertRaisesRegex(ValueError,'another source'):self.publish_with_fake_remote(raced_tag=True)
        self.assertFalse(self.remote_state['draft'])

    def test_failed_asset_readback_does_not_publish_draft(self):
        with self.assertRaisesRegex(ValueError,'asset readback failed'):self.publish_with_fake_remote(asset_failure=True)
        self.assertTrue(self.remote_state['draft']);self.assertFalse(self.remote_state['published'])


if __name__ == '__main__':unittest.main()
