#!/usr/bin/env python3
"""Preservation regressions; synthetic layout, actual shared manifest."""
import copy,json,unittest
from pathlib import Path
from assemble_shared_sdr_products import assemble
from qualify_shared_sdr_products import preservation
from shared_sdr_products import encode,sha,ADDITIONS
MANIFEST=(Path(__file__).resolve().parents[1]/'Apps/shared_sdr/waterfall.json').read_bytes()
class Products(unittest.TestCase):
 def fixture(self,style):
  old=b'\x7fELF'+b'old'*30;new=b'\x7fELF'+b'new'*30
  m=json.loads(MANIFEST);m['version']='0.3.5'
  row={'manifest':'waterfall.json','grants':[{'capability':'storage.key-value','api':1,'instance_id':3}]}
  sd={}
  if style:
   path='Apps/waterfall/'+(('image-'+sha(old)+'/') if style=='addressed' else '')+'waterfall.elf'
   row['image_source']={'path':path,'volume_instance':9};sd[path]=old;sd[path.rsplit('/',1)[0]+'/manifest.json']=encode(m)
   if style=='addressed':row['catalog_authority']=True
  boot={'app_capabilities':[row],'drivers':[],'default_app':'home.elf'}
  before={'waterfall.json':encode(m),'peer.elf':b'peer','peer.json':b'{}'}
  if not style:before['waterfall.elf']=old
  for index,(cap,api) in enumerate(ADDITIONS):
   name=f'provider{index}.json';before[name]=encode({'provides':[{'capability':cap,'api':api}]});boot['drivers'].append({'manifest':name})
  before['boot.json']=encode(boot);sd['peer.bin']=b'untouched'
  return before,sd,new
 def test_layouts_and_peer_rejection(self):
  for style in ('','stable','addressed'):
   with self.subTest(style=style):
    before,sd,elf=self.fixture(style);after,new_sd,*_=assemble(before,sd,elf,MANIFEST)
    preservation(before,sd,after,new_sd,elf,MANIFEST)
    bad=dict(after);bad['peer.elf']=b'changed'
    with self.assertRaises(ValueError):preservation(before,sd,bad,new_sd,elf,MANIFEST)
    bad_sd=dict(new_sd);bad_sd['peer.bin']=b'changed'
    with self.assertRaises(ValueError):preservation(before,sd,after,bad_sd,elf,MANIFEST)
    bad=dict(after);boot=json.loads(bad['boot.json']);boot['app_capabilities'][0]['grants'].pop(0);bad['boot.json']=encode(boot)
    with self.assertRaises(ValueError):preservation(before,sd,bad,new_sd,elf,MANIFEST)
 def test_catalog_manifest_required(self):
  before,sd,elf=self.fixture('addressed');after,new_sd,*_=assemble(before,sd,elf,MANIFEST)
  path=json.loads(after['boot.json'])['app_capabilities'][0]['image_source']['path']
  new_sd.pop(path.rsplit('/',1)[0]+'/manifest.json')
  with self.assertRaises(ValueError):preservation(before,sd,after,new_sd,elf,MANIFEST)
 def test_ambiguous_provider_rejected(self):
  before,sd,elf=self.fixture('stable');boot=json.loads(before['boot.json']);boot['drivers'].append(copy.deepcopy(boot['drivers'][0]));before['boot.json']=encode(boot)
  with self.assertRaises(ValueError):assemble(before,sd,elf,MANIFEST)
if __name__=='__main__':unittest.main()
