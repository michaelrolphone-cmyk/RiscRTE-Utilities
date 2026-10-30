"""Full-TU dependency receipts; scanner primitives reused from qualified resident-worker runner."""
import hashlib,json,os,shlex,shutil,subprocess
from functools import lru_cache
from pathlib import Path
def check_compiler_environment():
 for name in ('CPATH','CPLUS_INCLUDE_PATH','C_INCLUDE_PATH','OBJC_INCLUDE_PATH','GCC_EXEC_PREFIX','COMPILER_PATH','DEPENDENCIES_OUTPUT','SUNPRO_DEPENDENCIES'):
  if name in os.environ:raise ValueError('Unqualified compiler environment override: '+name)
def compiler_system_roots(prefix):
 check_compiler_environment()
 return compiler_standard_paths(prefix[0],'c' if '-std=c11' in prefix else 'c++')
@lru_cache(maxsize=None)
def compiler_standard_paths(compiler,language):
 # Resolve symlinked standard headers too; GCC emits their canonical targets.
 query=subprocess.run([compiler,'-E','-x',language,'-v','-'],input='',text=True,capture_output=True,check=True)
 text=query.stderr.split('#include <...> search starts here:',1)[1].split('End of search list.',1)[0]
 roots=[Path(line.strip()).resolve() for line in text.splitlines() if line.strip()]
 if not roots or any(not p.is_dir() for p in roots):raise ValueError('Cannot identify compiler standard include directories')
 aliases={};extra=[]
 for root in roots:
  if any(root!=other and root.is_relative_to(other) for other in roots):continue
  for path in root.rglob('*'):
   if not path.is_symlink():continue
   target=path.resolve()
   if path.is_dir():extra.append(target)
   elif path.is_file():aliases[str(target)]=str(path)
 return roots+extra,aliases
def compiler_dependencies(prefix,source):
 check_compiler_environment()
 raw=subprocess.check_output([*prefix,'-M','-MT','dependencies',str(source)],text=True)
 return [Path(value).resolve() for value in shlex.split(raw.split(':',1)[1].replace('\\\n',' '))]

class Closure:
 def __init__(self, roots, output):
  self.roots=[(name,Path(path).resolve()) for name,path in roots]
  self.output=Path(output);self.inputs={};self.system_inputs={};self.compilers={};self.units=[];self.generated_origins={}
 def record(self,command):
  command=list(map(str,command));sources=[v for v in command[1:] if Path(v).suffix in ('.c','.cpp','.cc','.cxx')]
  if not sources:return
  prefix=[];skip=False
  for index,value in enumerate(command):
   if skip:skip=False;continue
   if value=='-o':skip=True;continue
   if value in sources or value.endswith('.o') or value.startswith('-Wl,') or value.startswith('-l'):continue
   prefix.append(value)
  roots,aliases=compiler_system_roots(prefix)
  compiler=Path(shutil.which(prefix[0]) or prefix[0]).resolve()
  self.compilers[str(compiler)]={'sha256':hashlib.sha256(compiler.read_bytes()).hexdigest(),'version':subprocess.check_output([str(compiler),'--version'],text=True),'standard_roots':list(map(str,roots))}
  for source in sources:
   keys=[]
   for path in compiler_dependencies(prefix,source):
    key=None
    for name,root in self.roots:
     if path.is_relative_to(root):key=name+'/'+str(path.relative_to(root));break
    digest=hashlib.sha256(path.read_bytes()).hexdigest()
    if key is not None:
     if key in self.inputs and self.inputs[key]!=digest:raise ValueError('Input changed during build: '+key)
     self.inputs[key]=digest;keys.append(key)
     if key.startswith('generated-sdk/') and key not in self.generated_origins:
      origins=[]
      for name,root in self.roots:
       if name=='generated-sdk':continue
       for original in root.rglob(path.name):
        if original.is_file() and hashlib.sha256(original.read_bytes()).hexdigest()==digest:
         origins.append(name+'/'+str(original.relative_to(root)))
      if not origins:raise ValueError('Generated SDK dependency has no exact declared source: '+key)
      self.generated_origins[key]=sorted(origins)
    elif str(path) in aliases or any(path.is_relative_to(p) for p in roots):self.system_inputs[str(path)]=digest
    else:raise ValueError('Undeclared external dependency: '+str(path))
   self.units.append({'source':source,'compile_prefix':prefix,'dependencies':sorted(keys)})
  self.save()
 def save(self):
  self.output.write_text(json.dumps({'inputs':dict(sorted(self.inputs.items())),'system_inputs':dict(sorted(self.system_inputs.items())),'compiler_provenance':self.compilers,'translation_units':self.units,'generated_origins':self.generated_origins,'scanner_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n')
