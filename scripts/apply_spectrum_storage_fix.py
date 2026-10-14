from pathlib import Path
import subprocess
R=Path.cwd()
def edit(name, old, new):
 p=R/name;s=p.read_text();n=s.count(old)
 if n!=1:raise RuntimeError(f'{name}: expected one match, found {n}: {old[:80]!r}')
 p.write_text(s.replace(old,new))
for name,sha in {'Apps/audio_spectrum.c':'8ba32d8b1e22fbf69009fad0086771ff23e79ed6','Apps/spectrum_temporal_app.inc':'1983519e065da117184cd62bbcbd5ef6e936cc52'}.items():
 assert subprocess.check_output(['git','hash-object',name],text=True).strip()==sha,(name,'source drift')
p='Apps/spectrum_temporal_app.inc'
edit(p,'static risc_runtime_capability_v1 event_data_grant;\nstatic bool event_data_acquired,event_initialized', '#include "rf_app_data_scope.h"\n/* Reuse the RF app/shared-data transaction fence for Audio namespace 2. */\nstatic rf_app_data_scope event_data_scope;\nstatic bool event_initialized')
edit(p,'''static bool event_acquire(void){
 if(event_files.api)return true;
 if(event_data_acquired)return false;
 event_data_grant=(risc_runtime_capability_v1){.struct_size=sizeof(event_data_grant)};
 if(!runtime->acquire("storage.app-data",1,2,&event_data_grant))return false;
 event_data_acquired=true;
 const risc_app_data_v1 *api=event_data_grant.api;if(!api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->stat||!api->read||!api->replace)return false;event_files.api=api;return true;
}''','''static bool event_acquire(void){
 if(event_files.api)return true;
 /* This stable app-owned table holds no native lease between callbacks.
  * Contexts may checkpoint shared models/rules before or after any file call. */
 event_files.api=rf_data_bind(&event_data_scope,runtime,2u);return true;
}''')
edit(p,'event_data_grant=(risc_runtime_capability_v1){0};event_data_acquired=false;event_initialized=true;', 'event_data_scope=(rf_app_data_scope){0};event_initialized=true;')
edit(p,'static bool temporal_retained(void){return event_initialized&&event_files.retained;}', 'static bool temporal_retained(void){return event_initialized&&(event_files.retained||event_data_scope.retained);}')
p='Apps/audio_spectrum.c'
edit(p,'restore();signature_restore();event_restore();cfa_restore();if(temporal_retained())return;', 'restore();signature_restore();event_restore();if(temporal_retained())return;cfa_restore();if(temporal_retained())return;')
edit(p,'stop();if(event_data_acquired&&!runtime->release(&event_data_grant))retain();event_data_acquired=false;event_files.api=NULL;', 'stop();if(temporal_retained())return;event_files.api=NULL;')
# Fixture accounting now follows each native operation, not one app lifetime.
for p in (R/'test/native_apps').glob('*spectrum*.c'):
 s=p.read_text();t=s.replace('&event_data_grant','&event_data_scope.grant').replace('file_releases==1','file_releases==file_calls')
 t=t.replace('!file_releases&&!store_releases','file_releases+1==file_calls&&!store_releases')
 if s!=t:p.write_text(t)
assert not any('event_data_grant' in (R/p).read_text() or 'event_data_acquired' in (R/p).read_text() for p in ['Apps/audio_spectrum.c','Apps/spectrum_temporal_app.inc'])
subprocess.run(['git','diff','--check'],check=True)
subprocess.run(['git','diff','--stat'],check=True)
