#!/usr/bin/env python3
"""Build/validate only bounded scene-provider ELFs; no firmware/store mutation."""
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','system-apps','telemetry-font','output'):
 p.add_argument('--'+name,type=Path,required=True)
p.add_argument('--legacy-system',type=Path,help='Optional materialized 1aaf scene and its four original changed headers')
p.add_argument('--compactor',type=Path,help='Approved loader-metadata-v3 helper; needs pyelftools')
a=p.parse_args();system=a.system_apps.resolve();output=a.output.resolve()
if output.exists():raise FileExistsError('Choose a new output directory')
output.mkdir(parents=True);sys.path.insert(0,str(system/'scripts'))
from scene_build import build,compiler_path,json_write
from scene_sdk import stage_sdk
font_hash=hashlib.sha256(a.telemetry_font.read_bytes()).hexdigest()
assert font_hash=='2d7501e42d6223ebbc191817715431182c435fea944a1a02166a0748a577eaf1'
compiler=compiler_path();receipts={}
if a.compactor:
 spec=importlib.util.spec_from_file_location('pair_compactor',a.compactor);compact=importlib.util.module_from_spec(spec);spec.loader.exec_module(compact)
 assert compact.PROFILE=='loader-metadata-v3'
with tempfile.TemporaryDirectory(prefix='provider-target-',dir=output) as td:
 t=Path(td);validator=t/'validate'
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(system/'test/native_apps/stubs'),'-I'+str(system/'lib/elf_loader/include'),str(system/'lib/elf_loader/src/esp_elf_validate.c'),str(system/'test/native_apps/validate_test.c'),'-o',str(validator)],check=True)
 for name,source in [('cumulative',system)]+([('legacy',a.legacy_system)] if a.legacy_system else []):
  inc=stage_sdk(a.runtime,system,t/name);(inc/'fonts').mkdir();shutil.copyfile(a.telemetry_font,inc/'fonts/telemetry.inc')
  if name=='legacy':
   for path in ('sdk/app/RiscSceneLifecycleV1.h','sdk/app/RiscSceneComponentsV1.h','sdk/app/RiscSceneCheckpointV1.h','Services/text_input/SceneKeyboardV1.h'):
    shutil.copyfile(source/path,inc/Path(path).name)
  manifest=json.loads((source/'Services/scene_host/manifest.json').read_text())
  manifest['requires']=[r for r in manifest['requires'] if r['capability']!='input.navigation']
  receipt=build(compiler,inc,output/name,manifest,[source/'Services/scene_host/host.c'],[])
  receipt['profile']='full; no explicit -D; optional navigation omitted from manifest'
  receipt['font_inputs']={'telemetry.inc':font_hash};receipt['manifest']=manifest
  subprocess.run([str(validator),str(output/name/'driver.elf')],check=True)
  receipt['raw_structural_validation']='passed'
  if a.compactor:
   candidate=output/name/'driver.compact.elf';shutil.copyfile(output/name/'driver.elf',candidate)
   receipt['compaction']=compact.compact(candidate,compiler,output/name/'driver.debug.elf')
   assert compact.evidence((output/name/'driver.elf').read_bytes())==compact.evidence(candidate.read_bytes())
   subprocess.run([str(validator),str(candidate)],check=True);receipt['compact_structural_validation']='passed'
  json_write(output/name/'build.json',receipt);receipts[name]=receipt
json_write(output/'qualification.json',{'scope':'target provider only; no Xtensa instruction execution or firmware/store admission','providers':receipts})
for name,r in receipts.items():print(name,r['size_bytes'],r['sha256'],json.dumps(r.get('compaction',{})))
