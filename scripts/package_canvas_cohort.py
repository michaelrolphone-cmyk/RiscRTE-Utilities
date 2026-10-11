#!/usr/bin/env python3
"""Admit and package a source-pinned full-initial canvas product candidate.

No device I/O, release, OTA qualification or implicit data migration.
"""
import argparse,hashlib,io,json,os,subprocess,sys
from pathlib import Path

def sha(raw):return hashlib.sha256(raw).hexdigest()
def encoded(v):return (json.dumps(v,indent=2,sort_keys=True)+'\n').encode()
def inventory(files):return {k:{'sha256':sha(v),'bytes':len(v)} for k,v in sorted(files.items())}
def readfiles(root):return {str(p.relative_to(root)):p.read_bytes() for p in root.rglob('*') if p.is_file()}
def git(root,x):return subprocess.check_output(['git','-C',str(root),'rev-parse',x],text=True).strip()
def artifact(row):
 b=Path(row['path']).read_bytes();assert sha(b)==row['sha256'] and len(b)==row['bytes'],row['path'];return b
p=argparse.ArgumentParser(description=__doc__)
for n in ('spec','output'):p.add_argument('--'+n,type=Path,required=True)
a=p.parse_args();spec=json.loads(a.spec.read_text());assert not a.output.exists();a.output.mkdir(parents=True)
for label,row in spec['sources'].items():
 root=Path(row['path']);assert git(root,'HEAD')==row['commit'] and git(root,'HEAD^{tree}')==row['tree'],label
 assert not subprocess.check_output(['git','-C',str(root),'status','--porcelain','--untracked-files=no']),label
for name,digest in spec['compiled_inputs'].items():assert sha(Path(name).read_bytes())==digest,name
for row in spec['qualifications']:artifact(row)
runtime=Path(spec['runtime']);watch=Path(spec['watch_tools']);recovery=Path(spec['recovery_tools'])
sys.path[:0]=[str(watch/'scripts'),str(runtime/'scripts'),str(recovery),str(recovery.parent/'scripts')]
from current_bootfs import build as pack
from read_only_spiffs import read_image
from paired_bank_images import initial_bank_state,initial_otadata,parse_record
from paired_candidate import native_proof,partitions,APP_DATA_EXPECTED
from radio_iq_proof import prove as iq_proof
from native_binary_exports import exports as bin_exports,reference_tables
from elftools.elf.elffile import ELFFile
import verify_update_elf as verify
import recovery_store_admission as admission
admission.ROOT=watch
raw=artifact(spec['baseline']);assert len(raw)==0x1000000
baseline=read_image(raw[0x2f0000:0x800000],0x510000)
old=json.loads(baseline['cohort.json']);assert old['version']==spec['baseline_version']
files=readfiles(Path(spec['stage']));stage=json.loads(artifact(spec['stage_receipt']))
assert {n:sha(b) for n,b in files.items()}==stage['stage_files']
firmware=artifact(spec['firmware']);native_elf=artifact(spec['native_elf']);assert len(firmware)<=0x260000
assert b'RISC_APP_POLICY_ROWS:24\0' in firmware
assert ('RISC_RUNTIME_VERSION:'+spec['runtime_version']).encode()+b'\0' in firmware
assert ('RTE_SOURCE='+git(runtime,'HEAD')).encode()+b'\0' in firmware
bootloader=artifact(spec['bootloader']);table=artifact(spec['partitions']);partitions(table,APP_DATA_EXPECTED)
assert raw[:len(bootloader)]==bootloader and raw[0x8000:0x8000+len(table)]==table
native_checks=native_proof(native_elf);native_checks['iq']=iq_proof(native_elf)
exports,export_proof=bin_exports(firmware,reference_tables(native_elf));assert exports==verify.public_exports(ELFFile(io.BytesIO(native_elf)))
cohort={**old,'version':spec['version'],'runtime_version':spec['runtime_version'],'firmware_sha256':sha(firmware),'firmware_size':len(firmware),'source_revision':spec['product_revision']}
files['cohort.json']=encoded(cohort);boot=json.loads(files['boot.json'])
assert len(boot['app_capabilities'])==spec['app_count'] and len(boot['drivers'])==27
old_source=verify.admission_source;old_header=verify.cohort_admission_header
header='#define RISC_APP_REQUIREMENT_ROWS 24\n#define RISC_APP_POLICY_ROWS 24\n#include "'+str(runtime/'src/runtime/drivers/NativeProviderPolicyValidationV1.h')+'"\n'
def source(text):
 b,r,d=old_source(text);return header+b,r,d
os.environ['CPLUS_INCLUDE_PATH']=str(runtime/'src')
strict=[];verify.admission_source=source
try:
 for name in sorted(n for n in files if n.endswith('.elf')):
  r=verify.verify(runtime,native_elf,files[name],provider='/' in name);r['path']=name;strict.append(r)
finally:verify.admission_source=old_source
verify.cohort_admission_header=lambda rt,elf:header+old_header(rt,elf)
admissions=[]
for sanitize in ['0','1']:
 os.environ['SANITIZE']=sanitize;os.environ['ASAN_OPTIONS']='detect_leaks=0'
 r=admission.admit_cohort(runtime,native_elf,files,files,app_policy_rows=24);r['sanitized']=sanitize=='1';admissions.append(r)
# Multiple provider instances can share one module (the Watch has two I2C
# buses). Require the exact manifest-selected module set, not one ELF per
# instance, while still rejecting missing or orphan executable files.
selected=set()
for row in boot['app_capabilities']+boot['drivers']:
 manifest=Path(row['manifest']);metadata=json.loads(files[str(manifest)])
 selected.add(str(manifest.parent/metadata['file_name']))
assert {r['path'] for r in strict}==selected
assert all(r['cohort_validated'] and r['elf_count']==len(selected) and r['hardware_calls']==r['storage_calls']==0 for r in admissions)
fs,geometry=pack(files);assert pack(files)[0]==fs and read_image(fs,0x510000)==files
assert admission.unpack_image(fs,Path(spec['mkspiffs']),0x510000,expected_tool_sha256=spec['mkspiffs_sha256'])==files
state=initial_bank_state(firmware,fs,True);pair=parse_record(state[:96],True)
assert pair[6]==hashlib.sha256(firmware).digest() and pair[7]==hashlib.sha256(fs).digest()
image=bytearray(raw);image[0x10000:0x270000]=firmware+b'\xff'*(0x260000-len(firmware));image[0x2f0000:0x800000]=fs;image[0xff0000:0xff2000]=initial_otadata();image[0xff2000:0xff4000]=state
regions=[(0,0x10000),(0x270000,0x2f0000),(0x800000,0xff0000),(0xff4000,0x1000000)]
assert all(image[x:y]==raw[x:y] for x,y in regions)
changed=sorted(k for k in set(files)|set(baseline) if files.get(k)!=baseline.get(k));assert set(changed)==set(spec['changed_paths']),changed
for name,blob in files.items():
 dest=a.output/'store'/name;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(blob)
name=spec['target']+'-'+spec['version']+'-Canvas-Apps-FULL-INITIAL-ERASES-DATA.bin'
(a.output/name).write_bytes(image);(a.output/'bootfs.bin').write_bytes(fs)
report={'schema':'riscrte.canvas-cohort','version':spec['version'],'inputs':spec,'cohort':cohort,'image':{'name':name,'bytes':len(image),'sha256':sha(image),'flash_offset':0},'native':native_checks,'exports':export_proof,'strict_admission':strict,'cohort_admission':admissions,'store_generator':geometry,'store_files':inventory(files),'changed_store_paths':changed,'paired_sha_crc_verified':True,'preserved_regions':[{'offset':x,'bytes':y-x,'sha256':sha(raw[x:y])} for x,y in regions],'hardware_tested':False,'data_preserving_update_qualified':False,'warning':'Full 16 MiB image at 0x0 overwrites internal settings and AppData. Back up first. SD is not included.'}
(a.output/'build-custody.json').write_bytes(encoded(report));print(json.dumps(report['image']))
