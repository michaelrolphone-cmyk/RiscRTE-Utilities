#!/usr/bin/env python3
"""Bind actual shared OBD candidate bytes to owning Runtime graph/ELF admission.
Reuses Files native fixtures, with X4 requirement capacity closed against its new
full native profile. No target instruction or physical memory-fit claim.
Serial boot/private file preparation is excluded from this no-I/O fixture;
the separately built full X4 firmware retains those enabled namespaces.
"""
import argparse,pathlib,json,subprocess,hashlib,shlex,os,re
WATCH=pathlib.Path(__file__).resolve().parents[1]
ROOT=pathlib.Path(os.environ['OBD_SYSTEM']).resolve()
p=argparse.ArgumentParser(description=__doc__)
for name in ('runtime','x4-runtime','assembly','output'):p.add_argument('--'+name,type=pathlib.Path,required=True)
for name in ('x4-product','watch-product','wave-product'):p.add_argument('--'+name,type=pathlib.Path,required=True)
p.add_argument('--bringup',action='store_true');p.add_argument('--prepare-only',action='store_true');p.add_argument('--lane',choices=['watch','wave','x4']);p.add_argument('--source-pins',type=pathlib.Path);p.add_argument('--sanitize',action='store_true');args=p.parse_args();args.assembly=args.assembly.resolve();args.output.mkdir(parents=True,exist_ok=False)
fixtures=ROOT/'test/files_products/native';templates=json.loads((fixtures/'commands.json').read_text());results={}
sha=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

def source_head(root):
 result=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
 subprocess.run(['git','-C',str(root),'diff','--exit-code','HEAD','--'],check=True,capture_output=True)
 return result
source_commit=source_head(ROOT)
runtime_heads={role:source_head(root) for role,root in (('runtime',args.runtime),('x4_runtime',args.x4_runtime))}
if runtime_heads!={'runtime':'712ecc3f894dba99d9c9638941778a82f5856b97','x4_runtime':'0052d1fc42e9cfc82d626c5a247d460919c87935'}:raise ValueError('Exact selected shared Home Runtime required')
profile=args.x4_product/'profiles/native-shared-home.cmake'
if sha(profile)!='a08af0c1dac13b3727f87c8223583bb2a88fcad6802dab33ac98dd1d4779e3c8':raise ValueError('Qualified X4 profile differs')
profile_text=profile.read_text()
profile_values={}
for key in ('RISC_APP_POLICY_ROWS','RISC_APP_REQUIREMENT_ROWS','RISC_PROVIDER_CAPACITY','RISC_PLATFORM_CAPACITY','RISC_NATIVE_SERIAL_BOOT_FILES','RISC_NATIVE_SERIAL_APPDATA_FILES','RISC_NATIVE_SERIAL_SD'):
 matches=re.findall(r'\b'+key+r'=(\d+)\b',profile_text)
 if len(matches)!=1:raise ValueError('Ambiguous native profile definition '+key)
 profile_values[key]=int(matches[0])
if profile_values!={'RISC_APP_POLICY_ROWS':32,'RISC_APP_REQUIREMENT_ROWS':32,'RISC_PROVIDER_CAPACITY':40,'RISC_PLATFORM_CAPACITY':40,'RISC_NATIVE_SERIAL_BOOT_FILES':1,'RISC_NATIVE_SERIAL_APPDATA_FILES':1,'RISC_NATIVE_SERIAL_SD':1}:raise ValueError('Native profile capacities/features differ')
if templates['x4'].count('-DRISC_APP_REQUIREMENT_ROWS=24')!=1:raise ValueError('Original fixture capacity changed')
templates['x4']=[('-DRISC_APP_REQUIREMENT_ROWS=32' if x=='-DRISC_APP_REQUIREMENT_ROWS=24' else x) for x in templates['x4']]
native_profile={'source':source_head(args.x4_product),'path':'profiles/native-shared-home.cmake','sha256':sha(profile),'definitions':profile_values,
 'fixture_exclusions':{'RISC_NATIVE_SERIAL_BOOT_FILES':0,'RISC_NATIVE_SERIAL_APPDATA_FILES':0},
 'scope':'Owning no-I/O admission models retained SD graph only; boot/private serial preparation performs storage provisioning and is covered by full native firmware source/build, not this fixture.'}

native_profiles={'x4':native_profile}
for lane,root,expected,old in (
 ('watch',args.watch_product,'c69d7914935fccdc04749f7bfa429c74219bf88a','native-provisioning.cmake'),
 ('wave',args.wave_product,'97ca6157aa9c3bba7c7fee3858aa7d0ff896d13b','serial-transfer-native.cmake')):
 if source_head(root)!=expected:raise ValueError('Exact owning capacity successor source required '+lane)
 root=root.resolve()
 expected_text='# Preserve the complete selected provisioning profile; increase provider rows only.\ninclude("${CMAKE_CURRENT_LIST_DIR}/'+old+'")\nlist(REMOVE_ITEM RISC_NATIVE_DEFINITIONS RISC_PROVIDER_CAPACITY=40)\nlist(APPEND RISC_NATIVE_DEFINITIONS RISC_PROVIDER_CAPACITY=48)\n'
 profile=root/'profiles/shared-home-capacity.cmake'
 if profile.read_text()!=expected_text:raise ValueError('Capacity-only derivative differs '+lane)
 expanded=args.output/(lane+'-native-profile.txt');probe=args.output/(lane+'-native-profile.cmake')
 probe.write_text('set(RISC_RUNTIME_ROOT "'+str(args.runtime.resolve())+'")\ninclude("'+str(profile)+'")\nfile(WRITE "'+str(expanded)+'" "${RISC_NATIVE_DEFINITIONS}")\n')
 subprocess.run(['cmake','-P',str(probe)],check=True)
 definitions=expanded.read_text().split(';')
 limits={}
 for key in ('RISC_APP_POLICY_ROWS','RISC_APP_REQUIREMENT_ROWS','RISC_PROVIDER_CAPACITY','RISC_PLATFORM_CAPACITY'):
  values=[int(x.split('=',1)[1]) for x in definitions if x.startswith(key+'=')]
  if len(values)!=1:raise ValueError('Ambiguous expanded native capacity '+lane+'/'+key)
  limits[key]=values[0]
 if limits!={'RISC_APP_POLICY_ROWS':32,'RISC_APP_REQUIREMENT_ROWS':32,'RISC_PROVIDER_CAPACITY':48,'RISC_PLATFORM_CAPACITY':32}:raise ValueError('Owning native capacities differ '+lane)
 if templates[lane].count('-DRISC_PROVIDER_CAPACITY=40')!=1:raise ValueError('Original fixture capacity changed '+lane)
 templates[lane]=['-DRISC_PROVIDER_CAPACITY=48' if x=='-DRISC_PROVIDER_CAPACITY=40' else x for x in templates[lane]]
 native_profiles[lane]={'source':expected,'path':'profiles/shared-home-capacity.cmake','sha256':sha(profile),'definitions':definitions,'capacities':limits,
  'scope':'CMake-expanded owning profile; separate full native firmware compilation and linked array proof required.'}

def tree_pins(root):
 if root.name=='after-sd' and not root.exists():return {}
 if not root.is_dir():raise ValueError('Missing candidate inventory '+str(root))
 result={}
 for path in sorted(root.rglob('*')):
  if path.is_symlink():raise ValueError('Candidate inventory contains symlink')
  if path.is_file():result[path.relative_to(root).as_posix()]=sha(path)
 if not result and root.name!='after-sd':raise ValueError('Empty candidate inventory')
 return result
def canonical_digest(value):
 return hashlib.sha256((json.dumps(value,sort_keys=True,separators=(',',':'))+'\n').encode()).hexdigest()
binding=None;product_pins={}
if not args.prepare_only:
 assembly_path=args.assembly/'assembly.json'
 assembly=json.loads(assembly_path.read_text())
 binding={'source_commit':source_commit,'obd_source_commit':source_head(WATCH),'assembly_sha256':sha(assembly_path),'products':{}}
 for lane in templates:
  if args.lane and lane!=args.lane:continue
  product_pins[lane]={which:tree_pins(args.assembly/lane/which) for which in ('before','after','stored','after-sd')}
  binding['products'][lane]={'inventory_sha256':assembly['products'][lane]['inventory_sha256'],
    **{which.replace('-','_')+'_tree_sha256':canonical_digest(pins) for which,pins in product_pins[lane].items()}}

for lane,template in templates.items():
 if args.lane and lane!=args.lane:continue
 cmd=[x.format(runtime=args.runtime,x4_runtime=args.x4_runtime,fixtures=fixtures,output=args.output) for x in template]
 if args.bringup and lane=='wave':cmd=[str(fixtures/'usb_wave.cpp') if x==str(fixtures/'wave.cpp') else x for x in cmd]
 if lane=='x4':cmd=[str(WATCH/'test/shared_obd_x4.cpp') if x==str(fixtures/'x4.cpp') else x for x in cmd]
 cmd.append('-I'+str(fixtures))
 cmd[1:1]=['-DSTORE_ADMISSION_COHORT','-Wno-misleading-indentation']
 if lane!='x4':cmd[1:1]=['-DSTORE_ADMISSION_ENCODED_IMAGES']
 if args.bringup and lane=='wave':cmd[1:1]=['-DSTORE_ADMISSION_USB_SD']
 cmd+=['-I'+str((args.x4_runtime if lane=='x4' else args.runtime)/'lib/elf_loader/include'),'-I'+str((args.x4_runtime if lane=='x4' else args.runtime)/'test/native_bank_stubs')]
 if args.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie']
 selected_runtime=args.x4_runtime if lane=='x4' else args.runtime
 validator=args.output/(lane+'-validate.o');vsrc=selected_runtime/'lib/elf_loader/src/esp_elf_validate.c'
 vcmd=['cc','-std=c11',*(['-fsanitize=address,undefined','-fno-sanitize-recover=all'] if args.sanitize else []),'-I'+str(selected_runtime/'lib/elf_loader/include'),'-I'+str(selected_runtime/'test/native_bank_stubs'),'-c',str(vsrc),'-o',str(validator)]
 subprocess.run(vcmd,check=True);cmd.append(str(validator))
 vdeps=subprocess.check_output(vcmd[:vcmd.index('-c')]+['-MM',str(vsrc)],text=True);validator_dependencies=shlex.split(vdeps.replace('\\\n',' ').split(':',1)[1])
 sources=[x for x in cmd if x.endswith(('.c','.cpp'))];base=[];skip=False
 for x in cmd[1:]:
  if skip:skip=False;continue
  if x=='-o':skip=True;continue
  if x in sources or x in ['-ldl','-rdynamic'] or x.endswith('.o'):continue
  base.append(x)
 files=set(validator_dependencies);per={str(vsrc):validator_dependencies}
 for src in sources:
  q=subprocess.run([cmd[0],*base,'-MM','-MT','source',src],capture_output=True,text=True,check=True);deps=shlex.split(q.stdout.replace('\\\n',' ').split(':',1)[1]);per[src]=deps;files.update(deps)
 before={f:sha(f) for f in sorted(files)}
 before[str(vsrc)]=sha(vsrc)
 portable={}
 for f,h in before.items():
  path=pathlib.Path(f).resolve()
  for role,root in [('fixtures',fixtures),('runtime',selected_runtime),('obd',WATCH)]:
   try:portable[role+'/'+str(path.relative_to(root.resolve()))]=h;break
   except ValueError:pass
  else:raise ValueError('Unmapped compiler dependency: '+f)
 if args.source_pins:assert portable==json.loads(args.source_pins.read_text())[lane],lane+' source pin mismatch'
 (args.output/(lane+'-source-pins.json')).write_text(json.dumps(portable,indent=2)+'\n')
 q=subprocess.run(cmd,capture_output=True);(args.output/(lane+'-compile.log')).write_bytes(q.stdout+q.stderr)
 if q.returncode:
  print((q.stdout+q.stderr).decode(errors='replace'),flush=True);raise RuntimeError(lane+' compile failed; see retained log')
 rows=[]
 for which in (() if args.prepare_only else ('after',)):
  argv=[str(args.output/lane),str(args.assembly/lane/'before'),str(args.assembly/lane/'stored')]
  if args.bringup and lane=='wave':argv.append(str(args.assembly/lane/'after-sd/Apps/watch-bringup/bringup.elf'))
  if lane=='x4':argv.append(str(args.assembly/lane/'after-sd/Apps/obd-dashboard/obd_dashboard.elf'))
  q=subprocess.run(argv,capture_output=True,text=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
  (args.output/(lane+'-'+which+'.log')).write_text(q.stdout+q.stderr)
  if q.returncode:
   print(q.stdout+q.stderr,flush=True);raise RuntimeError(lane+' '+which+' preparation failed, exit='+str(q.returncode))
  result=json.loads(q.stdout)
  if lane=='x4':
   if 'OBD_SD_ELF_ADMITTED' not in q.stderr:raise RuntimeError('Actual X4 OBD SD ELF not admitted')
   result['obd_sd_elf_validated']=True
  if args.bringup and lane=='wave' and not result.get('sd_elf_validated'):raise RuntimeError('Actual Wave SD application ELF not admitted')
  encoding=assembly['products'][lane].get('image_storage')
  expected_encoded=len(encoding['images']) if encoding else 0
  if result.get('encoded_image_count',0)!=expected_encoded:raise RuntimeError('Actual stored envelope read count differs '+lane)
  if encoding:
   if product_pins[lane]['after']!=encoding['canonical_files'] or product_pins[lane]['stored']!=encoding['stored_files']:raise RuntimeError('Encoding maps differ from exact candidate trees')
  elif product_pins[lane]['after']!=product_pins[lane]['stored']:raise RuntimeError('Unrecorded storage representation change')
  print(json.dumps({'lane':lane,'input':which,'admission':result}),flush=True)
  if not result['prepared'] or not result['cohort_validated'] or not result['elf_count'] or result['hardware_calls'] or result['storage_calls']:raise RuntimeError(lane+' invalid preparation')
  if lane=='x4' and not result.get('retained_sd_serial_graph_bound'):raise RuntimeError('X4 retained SD graph missing')
  rows.append({'input':which,'result':result})
 if {f:sha(f) for f in before}!=before:raise RuntimeError('Compiler input mutated')
 if binding:
  if sha(args.assembly/'assembly.json')!=binding['assembly_sha256']:raise RuntimeError('Assembly receipt mutated')
  if {which:tree_pins(args.assembly/lane/which) for which in ('before','after','stored','after-sd')}!=product_pins[lane]:raise RuntimeError('Candidate inventory mutated')
 results[lane]={'command':cmd,'inputs':before,'per_tu_dependencies':per,'results':rows,'binary_sha256':sha(args.output/lane)}
 (args.output/'receipt.json').write_text(json.dumps({'schema':1,'sanitized':args.sanitize,'scope':__doc__,'products':results,'runtime_sources':runtime_heads,'native_profile':native_profile,'native_profiles':native_profiles,'product_binding':binding,'candidate_input_files':product_pins,'prepare_only':args.prepare_only},indent=2)+'\n');print(lane,'PASS',flush=True)
