#!/usr/bin/env python3
"""Current T039 Buttons geometry and unchanged Touchpad selector-off parity."""
import argparse,hashlib,json,os,re,shlex,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
p.add_argument('--baseline-source',type=Path)
a=p.parse_args();
import qualify_t039_ble_candidate as candidate
assert candidate.digest(root/'Apps/ble_hid_app.inc') == candidate.CONTROLLER, 'Unreviewed controller change'
system=a.system_apps.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=True)
report={'schema':1,'hardware_tested':False,'physical_radio':False,'cases':[],'parity':[],'source_hashes':{}}
report['system_git_pin']=subprocess.check_output(['git','-C',str(system),'rev-parse','HEAD'],text=True).strip()
report['utilities_git_pin']=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
dependencies={}
includes=[system/'Apps',root/'Apps',root/'lib/Bluetooth/include',system/'lib/PortableApps/include',system/'lib/NativeApps/include',system/'lib/PortableApps/src']
base=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*['-I'+str(p) for p in includes]]
def run(cmd,label,**kw):
    r=subprocess.run(list(map(str,cmd)),capture_output=True,text=True,**kw)
    (out/(label+'-command.json')).write_text(json.dumps(list(map(str,cmd)),indent=2)+'\n')
    (out/(label+'.log')).write_text(r.stdout+r.stderr);r.check_returncode();return r.stdout
def identity(path):
    baseline_path=globals().get('baseline')
    if baseline_path is not None and path==baseline_path.resolve():
        return 'utilities-baseline:Apps/ble_hid_app.inc'
    for owner,base_path in sorted([('utilities',root),('system',system),('fixture',out)],key=lambda x:len(x[1].parts),reverse=True):
        try:return owner+':'+str(path.relative_to(base_path))
        except ValueError:pass
    raise ValueError('Unexpected dependency outside source roots: '+str(path))
def capture_dependencies(cmd,label):
    text=run(cmd,label).replace('\\\n',' ')
    for item in shlex.split(text.split(':',1)[1]):
        path=Path(item).resolve();key=identity(path)
        digest=hashlib.sha256(path.read_bytes()).hexdigest()
        if key in dependencies:assert dependencies[key][1]==digest,('Source changed during qualification',key)
        dependencies[key]=(path,digest)
capture_dependencies([*base,'-MM',root/'tests/hid_buttons_native_test.c'],'controller-dependencies')
script=root/'scripts/test_t039_touchpad_contract.py'
dependencies[identity(script)]=(script,hashlib.sha256(script.read_bytes()).hexdigest())
for sanitize in (False,True):
    label='sanitized' if sanitize else 'normal';exe=out/label
    flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if sanitize else []
    run([*base,*flags,root/'tests/hid_buttons_native_test.c','-o',exe],label+'-build')
    text=run([exe],label,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
    report['cases'].append({'sanitized':sanitize,'output':text,'passed':True});print(text,end='')
baseline=a.baseline_source
if baseline is None:
    baseline=out/'baseline-ble_hid_app.inc';baseline.write_bytes(subprocess.check_output(['git','-C',str(root),'show','1f246972e8177e9e0c7275e6b651a2b8c1829d60:Apps/ble_hid_app.inc']))
assert baseline.read_bytes()==subprocess.check_output(['git','-C',str(root),'show','1f246972e8177e9e0c7275e6b651a2b8c1829d60:Apps/ble_hid_app.inc']),'Baseline source differs from pinned Git object'
for pad,paper in [(0,False),(1,False),(0,True),(1,True)]:
    flags=['-DPORTABLE_APP_OWNS_TOUCH_CHROME','-DPORTABLE_RETURN_APP="springboard.elf"','-DPORTABLE_ALARM_CLIENT','-DPORTABLE_NOVA_UI',f'-DHID_TOUCHPAD={pad}']+(['-DPORTABLE_PAPER_HID'] if paper else [])
    cmd=[os.environ.get('CC','cc'),'-E','-P','-std=c11','-x','c',*flags,*['-I'+str(p) for p in includes]]
    values=[]
    for kind,path in [('baseline',baseline),('current',root/'Apps/ble_hid_app.inc')]:
        capture_dependencies([*cmd,'-MM',path],f'dependencies-{pad}-{paper}-{kind}')
        text=run([*cmd,path],f'preprocess-{pad}-{paper}-{kind}')
        values.append('\n'.join(re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|\w+|[^\s]',text)))
    expected_identical = bool(pad)
    assert (values[0]==values[1]) is expected_identical,('unexpected T039 Buttons/Touchpad delta',pad,paper)
    report['parity'].append({'touchpad':pad,'paper':paper,'identical':expected_identical,
        'historical_token_sha256':hashlib.sha256(values[0].encode()).hexdigest(),
        'candidate_token_sha256':hashlib.sha256(values[1].encode()).hexdigest()})
for name in ['Apps/ble_hid_app.inc','Apps/ble_buttons_native_layout.h','Apps/ble_buttons_native_view.inc','tests/hid_buttons_native_test.c','scripts/test_t039_touchpad_contract.py']:
    report['source_hashes'][name]=hashlib.sha256((root/name).read_bytes()).hexdigest()
for key,(path,digest) in dependencies.items():
    assert hashlib.sha256(path.read_bytes()).hexdigest()==digest,('Source changed during qualification',key)
report['compiled_dependency_sha256']={key:value[1] for key,value in sorted(dependencies.items())}
report['stable_source_verified']=True
report['log_sha256']={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.glob('*.log'))}
report['command_sha256']={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.glob('*-command.json'))}
(out/'qualification.json').write_text(json.dumps(report,indent=2)+'\n')
print('Seven current controller profiles; two unchanged Touchpad and two intentional Buttons token controls PASS')
