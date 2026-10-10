#!/usr/bin/env python3
"""Product overlay regression tests with real target package inputs."""
import argparse
import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
from compose_alarms_scene import compose,encoded,files_at


def run(runtime,system_packages,alarm_packages,watch_profile,paper_profile):
    tests=0
    with tempfile.TemporaryDirectory(prefix='scene-composition-') as temporary:
        root=Path(temporary)
        marker=root/'capacity.c';marker.write_text('const unsigned int risc_runtime_provider_capacity[4]={0x31504352,1,28,44};int main(void){return 0;}\n')
        subprocess.run(['cc',str(marker),'-Wl,-u,risc_runtime_provider_capacity','-o',str(root/'capacity.elf')],check=True)
        spec=importlib.util.spec_from_file_location('runtime_capacity',runtime/'scripts/runtime_capacity.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        witness=(root/'capacity.elf').read_bytes();assert module.read_capacity(witness)['providers']==28;tests+=1
        for bad in (b'',witness[:63],witness.replace(bytes.fromhex('52435031010000001c0000002c000000'),bytes(16))):
            try:module.read_capacity(bad)
            except ValueError:tests+=1
            else:raise AssertionError('Corrupt capacity evidence accepted')
        apps=[]
        for label,profile_path in (('watch',watch_profile),('paper',paper_profile)):
            native=label=='paper';base=root/label;base.mkdir()
            config_key='alarm_utc_cfg' if native else 'alarm_cfg';preference='time_zone' if native else 'alarm_volume'
            drivers=[{'manifest':'alarm-service/manifest.json','key_value':[
                {'key':config_key,'namespace':3,'access':'read'},
                {'key':preference,'namespace':1,'access':'read'}]}]
            entries={'board.json':b'{"unchanged":"hardware"}\n','alarms.elf':b'old elf',
                     'alarms.json':encoded({'id':'alarms','version':'0.2.14','file_name':'alarms.elf'}),
                     'alarm-service/driver.elf':b'unchanged scheduler',
                     'alarm-service/manifest.json':encoded({'id':'alarm-service','version':'0.4.8',
                       'provides':[{'capability':'alarm.service','api':2 if native else 1}]}),
                     'default.elf':b'unchanged product shell','cohort.json':b'old native binding'}
            for i in range(23):
                name=f'existing-{i}.json';drivers.append({'manifest':name})
                entries[name]=encoded({'id':f'existing-{i}','version':'1.0.0','provides':[{'capability':f'fixture.{i}','api':1}]})
            boot={'board':'board.json','default_app':'default.elf','provider_activation':'demand-retained',
                  'drivers':drivers,'app_capabilities':[{'manifest':'alarms.json','grants':[]}]}
            entries['boot.json']=encoded(boot)
            for name,blob in entries.items():
                path=base/name;path.parent.mkdir(exist_ok=True,parents=True);path.write_bytes(blob)
            before=files_at(base)
            output=root/(label+'-stage')
            record=compose(base,system_packages,alarm_packages,profile_path,output,runtime,root/'capacity.elf')
            assert files_at(base)==before and record['provider_count']==27
            assert not record['flashable'] and record['product_native_cohort_binding_required']
            assert record['native_capacity']['providers']==28 and record['removed_files']==['cohort.json']
            after=files_at(output/'store');assert 'cohort.json' not in after
            for name,blob in before.items():
                if name not in ('alarms.elf','alarms.json','boot.json','cohort.json'):assert after[name]==blob
            new_boot=json.loads(after['boot.json'])
            assert new_boot['drivers'][:24]==boot['drivers'] and new_boot['default_app']==boot['default_app']
            assert new_boot['provider_activation']=='demand-retained'
            assert json.loads(after['alarms.json'])['requires']==[{'capability':c,'api':1} for c in ('ui.scene','alarm.control','storage.app-data')]
            apps.append(hashlib.sha256(after['alarms.elf']).hexdigest());tests+=1
            for failure in ('capacity','namespace','clock-key','service-api','downgrade'):
                trial=copy.deepcopy(boot);p=json.loads(Path(profile_path).read_bytes());files=copy.deepcopy(before)
                if failure=='capacity':p['provider_capacity']=26
                elif failure=='namespace':trial['app_capabilities'].append({'manifest':'other.json','grants':[{'capability':'storage.app-data','api':1,'instance_id':61}]})
                elif failure=='clock-key':trial['drivers'][0]['key_value'][0]['key']='different_clock_basis'
                elif failure=='service-api':files['alarm-service/manifest.json']=encoded({'id':'alarm-service','version':'0.4.8','provides':[{'capability':'alarm.service','api':99}]})
                else:files['alarms.json']=encoded({'id':'alarms','version':'9.0.0'})
                files['boot.json']=encoded(trial)
                for name,blob in files.items():(base/name).write_bytes(blob)
                pp=root/(label+'-'+failure+'.json');pp.write_bytes(encoded(p))
                try:compose(base,system_packages,alarm_packages,pp,root/(label+'-'+failure))
                except ValueError:tests+=1
                else:raise AssertionError(f'{label} accepted {failure}')
                assert not (root/(label+'-'+failure)).exists()
                for name,blob in before.items():(base/name).write_bytes(blob)
        assert apps[0]==apps[1];tests+=1
    print(f'{tests} capacity-evidence/product-overlay checks passed; same Alarms ELF on both profiles; no flash image claimed.')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('runtime','system-packages','alarm-packages','watch-profile','paper-profile'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();run(a.runtime,a.system_packages,a.alarm_packages,a.watch_profile,a.paper_profile)
