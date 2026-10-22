"""Negative admission, evidence completeness and strict Audio section checks."""
import copy
import hashlib
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('rf_candidate', ROOT / 'scripts/qualify_rf_labeled_candidate.py')
q = importlib.util.module_from_spec(spec)
spec.loader.exec_module(q)


class CandidateAdmission(unittest.TestCase):
    def facts(self):
        return dict(candidate='a' * 40, expected_candidate='a' * 40, base_ancestor=True,
            predecessor=q.PREDECESSOR, frozen=q.FROZEN, cohort='historical-host', system=q.SYSTEMS['historical-host'],
            dirty={k: '' for k in ('candidate', 'predecessor', 'frozen', 'system')},
            delta={p: 'A' for p in q.ADDITIONS}, production_delta=q.GUARD_FILES)

    def reject(self, key, value):
        facts = self.facts()
        facts[key] = value
        with self.assertRaises(ValueError): q.validate_admission(q.CONTRACT, facts)

    def test_clean_exact_admission(self): q.validate_admission(q.CONTRACT, self.facts())
    def test_wrong_candidate(self): self.reject('candidate', 'b' * 40)
    def test_short_candidate(self): self.reject('candidate', 'abcd')
    def test_wrong_predecessor(self): self.reject('predecessor', q.FROZEN)
    def test_wrong_frozen(self): self.reject('frozen', q.PREDECESSOR)
    def test_wrong_system(self): self.reject('system', q.SYSTEMS['ci-composition'])
    def test_wrong_cohort(self): self.reject('cohort', 'ci-composition')
    def test_missing_base(self): self.reject('base_ancestor', False)
    def test_dirty_source(self): self.reject('dirty', {**self.facts()['dirty'], 'candidate': ' M Apps/waterfall.c'})
    def test_dirty_dependency(self): self.reject('dirty', {**self.facts()['dirty'], 'system': '?? extra.h'})
    def test_missing_tree_status(self): self.reject('dirty', {'candidate': ''})
    def test_production_change(self): self.reject('delta', {**self.facts()['delta'], 'Apps/audio_spectrum.c': 'M'})
    def test_frozen_runner_change(self): self.reject('delta', {**self.facts()['delta'], 'scripts/test_rf_shared_input_safety.py': 'M'})
    def test_missing_file(self): self.reject('delta', {p: 'A' for p in q.ADDITIONS[:-1]})
    def test_existing_file_modified(self): self.reject('delta', {p: 'M' for p in q.ADDITIONS})
    def test_unaccounted_guard(self): self.reject('production_delta', q.GUARD_FILES + ['Apps/audio_spectrum.c'])
    def test_bad_evidence_path(self): self.reject('delta', {**self.facts()['delta'], 'docs/other.tar.gz': 'A'})
    def test_evidence_replaces_file(self): self.reject('delta', {**self.facts()['delta'], q.EVIDENCE_ARCHIVE: 'M'})

    def test_additive_evidence(self):
        facts = self.facts()
        facts['delta'][q.EVIDENCE_ARCHIVE] = 'A'
        q.validate_admission(q.CONTRACT, facts)

    def test_contract_changes(self):
        for key, value in [('default_identity_hash', 0), ('frozen', q.PREDECESSOR), ('distributable', True)]:
            with self.subTest(key=key):
                profile = copy.deepcopy(q.CONTRACT)
                profile[key] = value
                with self.assertRaises(ValueError): q.validate_admission(profile, self.facts())

    def result(self):
        runs, pairs, tokens = q.expected_names()
        h = hashlib.sha256(b'fixture output').hexdigest()
        return dict(distributable=False, hardware_verified=False, target_elf_qualified=False,
            source_hashes_unchanged=True, inputs={'source': h}, default_identity_hash=2931434081,
            historical_nondefault_cfp_provenance='unresolved', counts=dict(q.COUNTS),
            stages={s: {'status': 'passed'} for s in list(q.COUNTS) + ['manifest-tests']},
            runs=[{'name': n, 'status': 'passed', 'dependencies': {'source': h}, 'binary_sha256': h,
                   'stdout': 'fixture output', 'normalized_sha256': h,
                   'selector_verified_off': n.startswith('rf-optout/')} for n in sorted(runs)],
            audio_pairs=[{'name': n, 'before': h, 'after': h, 'allocated_sections_identical': True,
                          'relocations_identical': True, 'raw_object_status': 'failed'} for n in sorted(pairs)],
            token_comparisons=[{'name': n, 'before': '0' * 64 if n.startswith('frozen-to-predecessor') else h, 'after': h,
                                'status': 'failed' if n.startswith('frozen-to-predecessor') else 'passed'} for n in sorted(tokens)],
            frozen_contract={'status': 'failed', 'waived': False, 'target_elf_gate': 'not-run',
                'unchanged_storage_checks': {p: 'passed' for p in ('Apps/rf_app_data_scope.h', 'Apps/rf_temporal_app.inc',
                    'Apps/context_fingerprint_app.inc', 'test/native_apps/rf_startup_dirty_checkpoint_test.c', 'test/native_apps/rf_startup_contexts_test.c')},
                'token_checks': [{'name': n.replace('frozen-to-predecessor', 'frozen-to-candidate'), 'status': 'failed'}
                                 for n in sorted(tokens) if n.startswith('frozen-to-predecessor')]})

    def bad_result(self, change):
        record = self.result()
        change(record)
        with self.assertRaises(ValueError): q.validate_result(record)

    def test_complete_result(self): q.validate_result(self.result())
    def test_failed_audio_behavior(self): self.bad_result(lambda r: r['stages']['audio-behavior'].update(status='failed'))
    def test_audio_hash_difference(self): self.bad_result(lambda r: r['audio_pairs'][0].update(after='0' * 64))
    def test_audio_code_difference(self): self.bad_result(lambda r: r['audio_pairs'][-1].update(allocated_sections_identical=False))
    def test_audio_relocation_difference(self): self.bad_result(lambda r: r['audio_pairs'][-1].update(relocations_identical=False))
    def test_missing_audio_pair(self): self.bad_result(lambda r: r['audio_pairs'].pop())
    def test_detached_audio_pair(self): self.bad_result(lambda r: r['runs'][0].update(stdout='different'))
    def test_frozen_waiver(self): self.bad_result(lambda r: r['frozen_contract'].update(waived=True))
    def test_false_frozen_pass(self): self.bad_result(lambda r: r['frozen_contract'].update(status='passed'))
    def test_missing_final_token_check(self): self.bad_result(lambda r: r['frozen_contract']['token_checks'].pop())
    def test_detached_frozen_row(self): self.bad_result(lambda r: r['frozen_contract']['token_checks'][0].update(status='passed'))
    def test_missing_frozen_byte_check(self): self.bad_result(lambda r: r['frozen_contract']['unchanged_storage_checks'].pop('Apps/rf_app_data_scope.h'))
    def test_missing_token_row(self): self.bad_result(lambda r: r['token_comparisons'].pop())
    def test_invalid_token_digest(self): self.bad_result(lambda r: r['token_comparisons'][0].update(before='h'))
    def test_false_token_status(self): self.bad_result(lambda r: r['token_comparisons'][0].update(status='passed'))
    def test_missing_execution(self): self.bad_result(lambda r: r['runs'].pop())
    def test_substituted_execution(self): self.bad_result(lambda r: r['runs'][0].update(name='unexpected'))
    def test_duplicate_execution(self): self.bad_result(lambda r: r['runs'][0].update(name=r['runs'][1]['name']))
    def test_missing_matrix_count(self): self.bad_result(lambda r: r['counts'].update({'rf-optout': 0}))
    def test_manifest_not_run(self): self.bad_result(lambda r: r['stages']['manifest-tests'].update(status='not-run'))
    def test_missing_optout_proof(self): self.bad_result(lambda r: next(x for x in r['runs'] if x['name'].startswith('rf-optout/')).update(selector_verified_off=False))
    def test_misplaced_optout_proof(self): self.bad_result(lambda r: r['runs'][0].update(selector_verified_off=True))
    def test_input_changed(self): self.bad_result(lambda r: r.update(source_hashes_unchanged=False))
    def test_missing_input_fingerprints(self): self.bad_result(lambda r: r.update(inputs={}))
    def test_changed_hash(self): self.bad_result(lambda r: r.update(default_identity_hash=0))
    def test_false_hardware(self): self.bad_result(lambda r: r.update(hardware_verified=True))
    def test_false_distribution(self): self.bad_result(lambda r: r.update(distributable=True))
    def test_false_target_acceptance(self): self.bad_result(lambda r: r.update(target_elf_qualified=True))
    def test_false_historical_provenance(self): self.bad_result(lambda r: r.update(historical_nondefault_cfp_provenance='repaired'))


class AudioSections(unittest.TestCase):
    def sections(self):
        return {'.text': dict(type=1, flags=6, size=1, bytes=b'X'),
                '.data': dict(type=1, flags=3, size=1, bytes=b'Y'),
                '.rela.text': dict(type=4, flags=0, size=1, bytes=b'R'),
                '.strtab': dict(type=3, flags=0, size=10, bytes=b'CSWTCH.1\0')}

    def test_identical(self): self.assertEqual(q.compare_sections(self.sections(), self.sections()), [])
    def test_compiler_switch_number_only(self):
        b = self.sections()
        b['.strtab']['bytes'] = b'CSWTCH.2\0'
        self.assertEqual(q.compare_sections(self.sections(), b)[0]['section'], '.strtab')

    def test_differences_rejected(self):
        for section, field, value in [('.text', 'bytes', b'Z'), ('.data', 'bytes', b'Z'),
            ('.rela.text', 'bytes', b'Z'), ('.strtab', 'bytes', b'other\0'), ('.text', 'size', 2), ('.data', 'flags', 2)]:
            with self.subTest(section=section, field=field):
                b = self.sections()
                b[section][field] = value
                with self.assertRaises(ValueError): q.compare_sections(self.sections(), b)

    def test_bad_elf(self):
        for data in (b'', b'\x7fELF', bytes(64)):
            with self.assertRaises(ValueError): q.elf_sections(data)


if __name__ == '__main__': unittest.main()


class CompositionAdmission(unittest.TestCase):
    facts = CandidateAdmission.facts
    reject = CandidateAdmission.reject
    result = CandidateAdmission.result
    def test_composition_binds_exact_head(self):
        facts=self.facts()
        facts.update(base_ancestor=False,delta={'Apps/audio_spectrum.c':'M'},
            composition={'source':facts['candidate'],'partitions':['signal-capture']})
        q.validate_admission(q.CONTRACT,facts)
        facts['composition']['source']='b'*40
        with self.assertRaisesRegex(ValueError,'exact signal'):q.validate_admission(q.CONTRACT,facts)

    def test_old_contexts_incomplete_cohort_is_not_current(self):
        self.reject('system','fd4b1e52fe2a49c3c4c22f9ccd4c98a91f88afa0')
        self.assertEqual(q.SYSTEMS['ci-composition'],'f3731ab02127dea86a308472e3780068c5a0a1cf')

    def test_composed_audio_overlay_is_exact(self):
        result=self.result()
        result['admission']={'composition':{'source':'a'*40,'partitions':['signal-capture']}}
        with self.assertRaisesRegex(ValueError,'overlay'):q.validate_result(result)
        result['audio_comparison_baseline']={'base':q.PREDECESSOR,'overlay':copy.deepcopy(q.AUDIO_OVERLAY)}
        q.validate_result(result)
        result['audio_comparison_baseline']['overlay']['Apps/audio_spectrum.c']['sha256']='0'*64
        with self.assertRaisesRegex(ValueError,'overlay'):q.validate_result(result)
