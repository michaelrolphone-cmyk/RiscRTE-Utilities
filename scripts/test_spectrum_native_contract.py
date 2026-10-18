#!/usr/bin/env python3
"""Native geometry/subpages and exact selector-off token parity."""
import argparse, hashlib, json, os, re, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
for key in ('legacy-system','sdk-include','baseline','output'):p.add_argument('--'+key,type=Path,required=True)
a=p.parse_args();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
report={'sources':{},'tests':[],'parity':[],'not_claimed':['physical display usability','physical audio','leak sanitizer']}
def run(command,label,**kw):
 r=subprocess.run(list(map(str,command)),capture_output=True,text=True,timeout=180,**kw)
 (out/(label+'.log')).write_text(r.stdout+r.stderr)
 (out/(label+'-command.json')).write_text(json.dumps(list(map(str,command)),indent=2)+'\n')
 r.check_returncode();return r.stdout
for source in ('tests/spectrum_native_layout_test.c','test/native_apps/spectrum_native_subpages_test.c'):
 report['sources'][source]=digest(ROOT/source)
 for sanitized in (False,True):
  label=Path(source).stem+('-sanitized' if sanitized else '-normal');exe=out/label
  flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitized else []
  cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,'-I'+str(a.legacy_system.resolve()/'lib/PortableApps/include'),'-I'+str(a.legacy_system.resolve()/'lib/NativeApps/include'),ROOT/source,'-lm','-o',exe]
  run(cmd,label+'-build');output=run([exe],label,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
  report['tests'].append({'source':source,'sanitized':sanitized,'returncode':0,'output':output});print(label,'PASS',flush=True)
for defines in ([],['PORTABLE_NOVA_UI','PORTABLE_ALARM_CLIENT'],['PORTABLE_SPECTRUM_SHARED_TEXT_INPUT']):
 values=[]
 for kind,tree in [('baseline',a.baseline.resolve()),('current',ROOT)]:
  cmd=[os.environ.get('CC','cc'),'-E','-P','-std=c11',*['-D'+d for d in defines],'-I'+str(a.sdk_include.resolve()),'-I'+str(a.legacy_system.resolve()/'lib/NativeApps/include'),'Apps/audio_spectrum.c']
  raw=run(cmd,'preprocess-'+kind+'-'+str(len(report['parity'])),cwd=tree)
  values.append('\n'.join(re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]',raw)).encode())
 assert values[0]==values[1],('selector-off token mismatch',defines)
 report['parity'].append({'defines':defines,'identical':True,'token_sha256':hashlib.sha256(values[1]).hexdigest()});print('Selector-off parity',defines or 'plain','PASS',flush=True)
for source in ('Apps/audio_spectrum.c','Apps/spectrum_controls.inc','Apps/spectrum_native_layout.h','Apps/spectrum_native_view.inc','Apps/spectrum_native_subpages.inc'):
 report['sources'][source]=digest(ROOT/source)
(out/'qualification.json').write_text(json.dumps(report,indent=2)+'\n')
