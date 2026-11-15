"""Use the selected System canonical SDK verifier; preserve historical exact pins."""
import hashlib,importlib.util,subprocess
from pathlib import Path

def git(repo,*args):
    return subprocess.check_output(['git','-C',str(repo),*args],stderr=subprocess.PIPE)
def verify(system,runtime,canonical):
    source=Path(system)/'scripts/runtime_sdk_identity.py'
    if source.exists():
        if source.read_bytes()!=git(system,'show','HEAD:scripts/runtime_sdk_identity.py'):
            raise ValueError('Selected System SDK verifier differs from committed source')
        spec=importlib.util.spec_from_file_location('selected_system_sdk_identity',source)
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        receipt=module.verify_runtime_sdk(runtime,canonical)
        receipt['verifier_sha256']=hashlib.sha256(source.read_bytes()).hexdigest()
        receipt['compatibility_loader_sha256']=hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        return receipt
    # Old System profiles lack the additive verifier. Their exact-only rule stays.
    if git(runtime,'rev-parse','HEAD').decode().strip()!=canonical or git(runtime,'status','--porcelain').strip():
        raise ValueError('Historical System requires exact clean canonical Runtime '+canonical)
    return {'canonical_commit':canonical,'checkout_commit':canonical,'checkout_clean':True,
            'historical_exact_only':True}
def header(runtime,canonical,name):
    return git(runtime,'show',canonical+':sdk/app/'+name)
