#!/usr/bin/env python3
import hashlib,json,tempfile,zipfile,warnings
from pathlib import Path
from unittest.mock import patch
from lora_product_archives import file_sha256,selected_archive
cases=[]
with tempfile.TemporaryDirectory() as tmp:
    root=Path(tmp);p=root/'artifact.zip'
    with zipfile.ZipFile(p,'w',compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('candidate/store.bin',b'store');z.writestr('candidate/sd-apps.zip',b'sd');z.writestr('unneeded/large.bin',b'x'*(16*1024*1024))
    assert file_sha256(p)==hashlib.sha256(p.read_bytes()).hexdigest();cases.append('complete-streamed-hash')
    reads=[];original=zipfile.ZipFile.read
    def observed(self,name,*args,**kwargs):reads.append(name);return original(self,name,*args,**kwargs)
    with patch.object(zipfile.ZipFile,'read',observed):result=selected_archive(p,{'candidate/store.bin'},{'candidate/sd-apps.zip'})
    assert result=={'candidate/store.bin':b'store','candidate/sd-apps.zip':b'sd'} and reads==sorted(result);cases.append('only-required-and-present-optional-expanded')
    assert selected_archive(p,{'candidate/store.bin'},{'absent'})=={'candidate/store.bin':b'store'};cases.append('missing-optional-allowed')
    try:selected_archive(p,{'absent'})
    except ValueError:cases.append('missing-required-rejected')
    else:raise AssertionError('Missing required accepted')
    for name in ('../unsafe','/unsafe','bad\\path'):
        q=root/'unsafe.zip'
        with zipfile.ZipFile(q,'w') as z:z.writestr('candidate/store.bin',b'ok');z.writestr(name,b'ignored')
        try:selected_archive(q,{'candidate/store.bin'})
        except ValueError:cases.append('unselected-unsafe-rejected:'+name)
        else:raise AssertionError('Unsafe ignored member accepted')
    q=root/'duplicate.zip'
    with warnings.catch_warnings():
        warnings.simplefilter('ignore')
        with zipfile.ZipFile(q,'w') as z:z.writestr('candidate/store.bin',b'ok');z.writestr('ignored',b'a');z.writestr('ignored',b'b')
    try:selected_archive(q,{'candidate/store.bin'})
    except ValueError:cases.append('unselected-duplicate-rejected')
    else:raise AssertionError('Duplicate ignored member accepted')
print(json.dumps({'passed':True,'cases':cases,'count':len(cases),'native_memory_peak':'not measured; native SIGKILL cause remains unproven'},indent=2))
