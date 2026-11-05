#!/usr/bin/env python3
"""Actual Spectrum source against an exclusive app/shared-data Runtime model."""
import argparse,os,pathlib,subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--system-apps',type=pathlib.Path,required=True)
p.add_argument('--expect-conflict',action='store_true')
a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[1];system=a.system_apps.resolve()
out=root/'build/spectrum-storage-lifecycle';out.mkdir(parents=True,exist_ok=True)
for san in (False,True):
 target=out/('spectrum-storage'+('-san' if san else ''))
 flags=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if san else []
 cmd=[os.environ.get('CC','cc'),'-std=c11','-O1','-g','-Wall','-Wextra','-Werror',*flags,
      '-I'+str(root/'lib/Contexts/include'),'-I'+str(system/'lib/PortableApps/include'),'-I'+str(system/'lib/NativeApps/include'),
      str(root/'test/native_apps/spectrum_storage_lifecycle_test.c'),'-lm','-o',str(target)]
 subprocess.run(cmd,check=True,timeout=120)
 run=subprocess.run([str(target)]+(['baseline'] if a.expect_conflict else []),capture_output=True,text=True,timeout=30,
  env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0','UBSAN_OPTIONS':'halt_on_error=1'})
 print(run.stdout+run.stderr,flush=True)
 if a.expect_conflict:
  assert run.returncode!=0 and 'single-slot storage refusal' in run.stderr,'Shipped-source conflict was not reproduced'
  print('CONFIRMED: shipped startup reaches the checked single-slot refusal',flush=True)
 else:run.check_returncode()
