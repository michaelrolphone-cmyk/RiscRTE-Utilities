#!/usr/bin/env python3
import copy,json
from pathlib import Path
from lora_product_authority import project_authority,LEGACY_GRANTS
r=Path(__file__).resolve().parents[1]
canonical=json.loads((r/'Apps/lora_scene.json').read_text());original=copy.deepcopy(canonical)
base=[{'capability':'radio.lora','api':2,'instance_id':0},{'capability':'storage.key-value','api':1,'instance_id':9}]
cases=[]
fixtures=json.loads((r/'test/lora_projection/original_radio_grants.json').read_text())
assert fixtures['watch']['radio_grants']==[{'api':2,'capability':'radio.lora','instance_id':11}]
assert fixtures['wave']['radio_grants']==[]
assembler=(r/'scripts/assemble_lora_watch_wave.py').read_text()
for lane in ('watch','wave'):
    assert fixtures[lane]['artifact_sha256'] in assembler
    grants=copy.deepcopy(fixtures[lane]['radio_grants'])+base[1:]+copy.deepcopy(list(LEGACY_GRANTS));before=copy.deepcopy(grants)
    manifest,retained,added=project_authority(canonical,grants,set())
    assert retained==grants==before and manifest['requires']==canonical['requires']
    assert [g for g in retained if g['capability']=='radio.lora']==fixtures[lane]['radio_grants']
    assert added==[{'capability':g['capability'],'api':1} for g in LEGACY_GRANTS]
    assert manifest['optional']==canonical['optional']+added
    cases.append(lane+'-actual-pinned-radio-preservation')
manifest,retained,added=project_authority(canonical,base,set())
assert manifest==canonical and retained==base and not added;cases.append('absent-no-new-authority')
for i,expected in enumerate(LEGACY_GRANTS):
    for key,value in [('api',2),('instance_id',99),('api',True),('instance_id',False),('file','unexpected')]:
        bad=dict(expected);bad[key]=value
        try:project_authority(canonical,base+[bad],set())
        except ValueError:cases.append(str(i)+'-reject-'+key+'-'+str(value))
        else:raise AssertionError('Accepted altered legacy grant')
    try:project_authority(canonical,base+[expected,expected],set())
    except ValueError:cases.append(str(i)+'-duplicate')
    else:raise AssertionError('Accepted duplicate')
try:project_authority(canonical,base+[{'capability':'unknown.authority','api':1,'instance_id':0}],set())
except ValueError:cases.append('unknown-still-rejected')
else:raise AssertionError('Unknown authority accepted')
without_radio=[g for g in base if g['capability']!='radio.lora']
manifest,retained,added=project_authority(canonical,without_radio,set())
assert retained==without_radio and manifest==canonical and not added
cases.append('radio-absent-preserved')
manifest,retained,added=project_authority(canonical,base,set())
assert retained==base;cases.append('radio-present-exact')
for bad in ([{'capability':'radio.lora','api':1,'instance_id':0}],
            [{'capability':'radio.lora','api':2,'instance_id':-1}],
            [{'capability':'radio.lora','api':2,'instance_id':2147483648}],
            [{'capability':'radio.lora','api':2,'instance_id':11.0}],
            [{'capability':'radio.lora','api':True,'instance_id':11}],
            [base[0],{'capability':'radio.lora','api':2,'instance_id':11}],
            [{'capability':'radio.lora','api':2,'instance_id':False}],
            [{'capability':'radio.lora','api':2,'instance_id':0,'file':'unknown'}],
            [base[0],base[0]]):
    try:project_authority(canonical,without_radio+bad,set())
    except ValueError:cases.append('radio-malformed-'+str(len(cases)))
    else:raise AssertionError('Malformed radio authority accepted')
# Schema is product-independent; no replacement guessed ID. Only the pinned
# artifact supplies authority. Projection must never rewrite any valid ID.
for instance in (0,1,11,42,2147483647):
    original_radio={'capability':'radio.lora','api':2,'instance_id':instance}
    _,retained,_=project_authority(canonical,without_radio+[original_radio],set())
    assert retained==without_radio+[original_radio]
    cases.append('schema-preserves-instance-'+str(instance))
assert canonical==original
print(json.dumps({'passed':True,'cases':cases,'count':len(cases),'scope':'manifest projection only; no product or hardware execution'},indent=2))
