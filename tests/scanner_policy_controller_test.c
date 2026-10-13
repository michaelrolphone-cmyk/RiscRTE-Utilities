/* Actual Scanner controller and shared client; deterministic provider/bridge boundaries.
 * Production adapter/provider integration is qualified separately, not mocked here as hardware. */
#define main legacy_policy_controller_main
#include "ble_app_test.c"
#undef main
static void name_editor(void){reset();scan.count=1;scan.devices[0].address_type=2;memcpy(scan.devices[0].address,"123456",6);detail=true;begin_name();assert(naming&&name_preference.edit.revision==1&&grants==2);}
int main(int argc,char **argv){
 assert(argc==2);const char *t=argv[1];name_editor();
 assert(policy_enables==1&&name_checkpoint.enabled);
 if(!strcmp(t,"accept")){text_accept("Kitchen");assert(!naming&&uat.commits==1&&!grants&&!name_home_return);}
 else if(!strcmp(t,"cancel")){text_result.state=RISC_TEXT_ENTRY_CANCELLED;name_step();assert(!naming&&!uat.submits&&!grants);}
 else if(!strcmp(t,"detached")||!strcmp(t,"continuity")){
  bridge_prepare=1;bridge_continuity=!strcmp(t,"continuity");name_step();assert(policy_pauses==1&&policy_resumes==1&&bridge_applies==1&&policy_focus!=bridge_continuity&&naming&&!uat.submits);
  assert(policy_resume_flags==(RISC_TEXT_ENTRY_RESUME_REDRAW|(bridge_continuity?0:RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)));text_accept("Kept");assert(uat.commits==1&&!grants);
 }else if(!strcmp(t,"presenting")||!strcmp(t,"input-busy")){
  bridge_prepare=1;policy_snapshot_flags=!strcmp(t,"presenting")?RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING:RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY;name_step();assert(!policy_pauses&&!bridge_applies&&naming);
 }else if(!strcmp(t,"pending")||!strcmp(t,"pending-exit")||!strcmp(t,"busy-after-pending")){
  bridge_prepare=1;policy_pending=1;name_step();assert(name_checkpoint.pausing&&!bridge_applies&&naming);
  if(!strcmp(t,"pending-exit")){bridge_exit=1;name_step();assert(!naming&&name_home_return&&!uat.submits&&!grants);}
  else if(!strcmp(t,"busy-after-pending")){policy_pause_result=RISC_TEXT_ENTRY_BUSY;name_step();assert(name_client.retained&&!bridge_applies);}
  else{name_step();assert(bridge_applies==1&&policy_resumes==1&&naming);}
 }else if(!strcmp(t,"busy")){bridge_prepare=1;policy_pause_result=RISC_TEXT_ENTRY_BUSY;name_step();assert(naming&&!name_client.retained&&!bridge_applies);}
 else if(!strcmp(t,"exit")||!strcmp(t,"delayed-exit")||!strcmp(t,"retained-close")||!strcmp(t,"exit-after-poll")||!strcmp(t,"runtime-exit")){
  text_result.state=RISC_TEXT_ENTRY_ACCEPTED;strcpy(text_result.text,"Discard");
  if(!strcmp(t,"exit-after-poll"))bridge_exit_on_second_read=true;
  else if(!strcmp(t,"runtime-exit"))uat.exit_pending=true;
  else bridge_exit=1;
  if(!strcmp(t,"delayed-exit"))text_close_again=1;
  if(!strcmp(t,"retained-close"))text_close_bad=true;
  name_step();assert(!uat.submits&&!uat.commits);
  if(text_close_bad){assert(name_client.retained&&naming&&!name_home_return&&grants==2);}
  else{if(naming){assert(name_closing&&!name_home_return&&grants==2);name_step();}assert(name_home_return&&!naming&&!grants);}
 }else if(!strcmp(t,"exit-apply")){
  bridge_prepare=1;bridge_apply=1;name_step();assert(name_closing&&naming&&!name_home_return&&name_policy_exit);name_step();assert(name_home_return&&!grants&&!uat.submits);
 }else if(!strcmp(t,"retained-pause")||!strcmp(t,"retained-resume")||!strcmp(t,"again-resume")||!strcmp(t,"retained-capture")||!strcmp(t,"retained-prepare")||!strcmp(t,"retained-apply")||!strcmp(t,"malformed-snapshot")){
  bridge_prepare=1;
  if(!strcmp(t,"retained-pause"))policy_pause_result=RISC_TEXT_ENTRY_RETAINED;
  if(!strcmp(t,"retained-resume"))policy_resume_result=RISC_TEXT_ENTRY_RETAINED;
  if(!strcmp(t,"again-resume"))policy_resume_result=RISC_TEXT_ENTRY_AGAIN;
  if(!strcmp(t,"retained-capture"))bridge_capture=-1;
  if(!strcmp(t,"retained-prepare"))bridge_prepare=-1;
  if(!strcmp(t,"retained-apply"))bridge_apply=-1;
  if(!strcmp(t,"malformed-snapshot"))policy_snapshot_flags=0x80000000u;
  name_step();assert(name_client.retained&&!uat.submits&&grants==2);unsigned before=text_closes;name_step();assert(text_closes==before);
 }else assert(0);
 printf("Scanner policy controller %s PASS\n",t);return 0;
}
