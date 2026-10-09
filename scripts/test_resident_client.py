#!/usr/bin/env python3
"""Actual Contexts paper controller with selected renderer-free System adapter.

Reuse the existing deterministic provider fixture, removing only its old
Quick-only test hooks. Production sources are included unchanged.
"""
import argparse,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--build',type=Path,required=True);p.add_argument('--utilities',type=Path,required=True);p.add_argument('--sanitize',action='store_true');a=p.parse_args()
r=json.loads((a.build/'contexts/x4-native-app.json').read_text());cmd=r['compile_command'];system=next(Path(s).parents[3] for s in cmd if s.endswith('/lib/PortableApps/src/adapter.c'))
out=ROOT/'build/resident-client';out.mkdir(parents=True,exist_ok=True)
fixture=(system/'test/native_apps/portable_native_toolbar_test.c').read_text().split('static void no_legacy(void)')[0]
fixture=fixture.replace('#define PORTABLE_QUICK_ACTIONS','').replace('#define PORTABLE_ALARM_CLIENT','').replace('if(close_quick_on_yield&&hook_calls)pqa_close(&quick.ui);','(void)close_quick_on_yield;')
fixture=fixture.replace('#include "../../lib/PortableApps/src/adapter.c"','#include "'+str(system/'lib/PortableApps/src/adapter.c')+'"')
fixture_path=out/'toolbar-providers.c';fixture_path.write_text(fixture)
s=(ROOT/'test/native_apps/contexts_paper_test.c').read_text()
s=s.replace('#include "../../Apps/contexts.c"','#include "'+str(ROOT/'Apps/contexts.c')+'"\n#include "resident_test_bridge.h"')
s=s.replace('static int cp_unread=-1;', 'static int cp_unread=-1;static const char *cp_bad_pref;static bool cp_pref_release_fail;')
s=s.replace('int i=cp_key(key);if(i<0){', 'int i=cp_key(key);if(i<0){if(cp_bad_pref&&!strcmp(key,cp_bad_pref)){assert(cap);*(uint8_t*)out=255;*size=1;return RISC_KEY_VALUE_OK;}')
s=s.replace('static unsigned cp_sleep_calls;', 'static bool cp_release_fail;\nstatic bool cp_release(risc_runtime_capability_v1 *g){if((cp_release_fail&&g->api==&cp_service)||(cp_pref_release_fail&&g->api==&cp_kv))return false;return fx_release(g); }\nstatic unsigned cp_sleep_calls;')
s=s.replace('fx_runtime.request_launch=cp_launch;', 'fx_runtime.request_launch=cp_launch;fx_runtime.resident_shell=resident_test_get;fx_runtime.release=cp_release;')
s=s.replace('assert(app_module_init()==0);ctx_app=', """assert(app_module_init()==0);
 if(!strcmp(argv[2],"preferences")) {
  assert(contexts_controls_load());assert(portable_contexts_action_mask()==(PORTABLE_CONTEXT_IDLE|PORTABLE_CONTEXT_DND));
  info.flags|=RISC_DISPLAY_INFO_BRIGHTNESS;cp_alarm.output_modes=ALARM_MODE_BOTH;
  assert(contexts_controls_load());uint16_t all=PORTABLE_CONTEXT_IDLE|PORTABLE_CONTEXT_DND|PORTABLE_CONTEXT_BRIGHTNESS|PORTABLE_CONTEXT_VOLUME|PORTABLE_CONTEXT_ALERT;
  assert(portable_contexts_action_mask()==all);
  const char *keys[]={PQA_DND_KEY,PQA_BRIGHTNESS_KEY,PQA_VOLUME_KEY};const uint16_t bits[]={PORTABLE_CONTEXT_DND,PORTABLE_CONTEXT_BRIGHTNESS,PORTABLE_CONTEXT_VOLUME};
  for(unsigned i=0;i<3;i++){cp_bad_pref=keys[i];assert(contexts_controls_load());assert(portable_contexts_action_mask()==(all&~bits[i]));}
  cp_bad_pref=NULL;assert(contexts_controls_load()&&portable_contexts_action_mask()==all);
  cp_pref_release_fail=true;assert(!contexts_controls_load()&&portable_adapter_retained());unsigned last=calls;
  assert(!contexts_controls_load()&&!portable_contexts_action_mask());app_module_fini();assert(calls==last);
  puts("Contexts missing/default and malformed preferences, visual/sound output, terminal preference release PASS");return 0;
 }
 ctx_app=""",1)
# Keep all model, action-mask, storage, scrolling, frame and recovery assertions.
begin=s.index(' /* Quick Controls cancels')
end=s.index(' ctx_draft_dirty=false;',begin)
s=s[:begin]+''' /* Host controls refuse unresolved drafts; host policy preserves their stack. */
 ctx_open_preset(0);ctx_field=1;ctx_change(1);cp_draw(argv[1],"23-draft-controls");old=ctx_draft.idle_ms;
 assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)==RISC_RESIDENT_BUSY);
 assert(!resident_test_controls&&ctx_draft_dirty&&ctx_draft.idle_ms==old);
 assert(portable_paper_frame_drain());contexts_client.loaded=true;contexts_client.policy.enabled=true;contexts_client.policy.radio_allowed=true;
 unsigned polls_before=resident_test_polls;assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_BUSY&&resident_test_polls==polls_before);
 assert(contexts_suspend());resident_policy_pending=true;
 assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY)==RISC_RESIDENT_OK);
 assert(resident_test_policies==1&&!contexts_client.api&&ctx_draft.idle_ms==old&&ctx_draft_dirty);
 ctx_read_status();assert(contexts_client.api);cp_draw(argv[1],"24-policy-draft");
 ctx_draft_dirty=false;ctx_page=CT_HOME;cp_draw(argv[1],"25-clean-controls");
 unsigned pauses=cp_pause_count;
 assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)==RISC_RESIDENT_OK);
 assert(resident_test_controls==1&&cp_pause_count>pauses&&!contexts_client.api);
 ctx_read_status();assert(contexts_client.api);
''' +s[end:]
# Clean physical Home returns to the resident host without a native launch.
s=s.replace('assert(cp_launches==saved_launches+1&&!strcmp(cp_destination,"default.elf"));', 'assert(cp_launches==saved_launches+(pass?1u:0u));')
# The callback refusal test is terminal and therefore runs last.
s=s.replace(' printf("Contexts paper', ''' assert(app_module_init()==0);ctx_runtime=portable_app_custody_runtime();ctx_service=portable_contexts_service();assert(ctx_service);
 if(!strcmp(argv[2],"release-fail"))cp_release_fail=true;else cp_pause_fail=true;assert(contexts_suspend()==false&&portable_adapter_retained());
 unsigned final_calls=calls;ctx_read_status();ctx_draw();app_module_fini();assert(calls==final_calls);
 printf("Contexts paper''')
source=out/'contexts-resident-test.c';source.write_text(s)
flags=[s for s in cmd if s.startswith(('-D','-I'))]
flags+=['-DTEST_NATIVE_TOOLBAR_QUICK','-DCONTEXTS_TOOLBAR_FIXTURE="'+str(fixture_path)+'"','-I'+str(system/'test/native_apps'),'-I'+str(a.utilities/'test/native_apps')]
extra=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-no-pie'] if a.sanitize else []
exe=out/('sanitized' if a.sanitize else 'normal')
command=['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',*extra,*flags,source,'-Wl,--wrap=free','-o',exe]
subprocess.run(list(map(str,command)),check=True)
for case in ('normal','flip','release-fail','preferences'):
 dest=out/(case+'-'+str(int(a.sanitize)));dest.mkdir(exist_ok=True)
 subprocess.run([str(exe),str(dest),case],check=True,timeout=60,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
(out/('evidence-san.json' if a.sanitize else 'evidence.json')).write_text(json.dumps(dict(production_receipt=r['elf_sha256'],cases=['normal','flip','release-fail','preferences'],sanitized=a.sanitize,compile_command=list(map(str,command))),indent=2)+'\n')
