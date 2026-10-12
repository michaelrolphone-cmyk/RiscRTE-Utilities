#define main legacy_ble_controller_fixture
#include "ble_app_test.c"
#undef main
static void radio_record(uint8_t flags){uint8_t b[]={0x51,1,flags,(uint8_t)(flags^0xa5u)};uat_store("quick_radio",b,4);}
static void name_editor(void){reset();scan.count=1;scan.devices[0].address_type=2;memcpy(scan.devices[0].address,"123456",6);detail=true;begin_name();assert(naming&&name_preference.edit.revision==1&&grants==2);}
int main(int argc,char **argv){
 assert(argc==2);const char *test=argv[1];
 if(!strncmp(test,"name-",5)){
  name_editor();
  if(!strcmp(test,"name-cancel")){text_result.state=RISC_TEXT_ENTRY_CANCELLED;name_step();assert(!uat.submits&&!grants);}
  else if(!strcmp(test,"name-stale")){uat.revision++;text_accept("Draft");assert(!uat.commits&&name_save_failed&&!grants);}
  else if(!strcmp(test,"name-busy")){uat.submit_error=RISC_ATOMIC_BUSY;text_accept("Draft");assert(uat.submits==1&&!uat.commits&&!grants&&name_save_failed);}
  else if(!strcmp(test,"name-copy")){text_accept("  Kitchen  ");assert(uat.commits==1&&!strcmp(aliases[0],"Kitchen")&&!grants);memset(editing,'x',24);char name[25];int i=uat_find(name_preference.edit.key);assert(i>=0&&ble_alias_decode(2,(const uint8_t*)"123456",uat.records[i].value,40,name)&&!strcmp(name,"Kitchen"));uat_assert_user(name_preference.edit.key,uat.records[i].value,40,1);}
  else if(!strcmp(test,"name-tombstone")){text_accept("");assert(uat.commits==1&&!grants);int i=uat_find(name_preference.edit.key);assert(i>=0&&uat.records[i].size==40&&!uat.records[i].value[10]);uat_assert_user(name_preference.edit.key,uat.records[i].value,40,1);}
  else if(!strcmp(test,"name-delayed")){uat.pending_polls=3;text_accept("Wait");assert(uat.commits==1&&uat.yields==3&&uat.collects==4&&!grants);}
  else if(!strcmp(test,"name-collect-error")){uat.collect_error=RISC_ATOMIC_IO;text_accept("Retain");assert(uncertain&&name_preference.edit.token&&name_preference.grant.api&&grants==1&&!uat.commits&&!portable_app_before_launch("x"));}
  else if(!strcmp(test,"name-dirty-submit")){uat.submit_error=RISC_ATOMIC_IO;uat.dirty_submit=true;text_accept("Retain");assert(uncertain&&name_preference.edit.token&&grants==1&&!uat.collects);}
  else if(!strcmp(test,"name-release-error")){uat.release_bad=true;text_accept("Retain");assert(uncertain&&!name_preference.edit.token&&name_preference.grant.api&&grants==1&&uat.commits==1);}
  else if(!strcmp(test,"name-bad-receipt")){uat.bad_receipt=true;text_accept("Retain");assert(uncertain&&name_preference.edit.token&&grants==1);}
  else if(!strcmp(test,"name-exit")){uat.exit_pending=true;text_accept("Discard");assert(!uat.submits&&!uat.commits&&!grants);}
  else if(!strcmp(test,"name-same")){text_accept("Same");begin_name();text_accept("Same");assert(uat.commits==2&&uat.user_intents==2&&!grants);}
  else assert(0);
 }else{
  reset();radio_record(1);paper_enable_needed=true;
  if(!strcmp(test,"radio-success")){paper_enable();assert(enabled&&controls==1&&uat.user_intents==1&&!uat.rollback_intents&&!grants&&!paper_enable_needed);uint8_t b[]={0x51,1,3,0xa6};uat_assert_user("quick_radio",b,4,1);}
  else if(!strcmp(test,"radio-airplane")){radio_record(4);paper_enable();assert(!controls&&!uat.submits&&!grants);}
  else if(!strcmp(test,"radio-rollback")){enable_bad=true;paper_enable();assert(!enabled&&controls==2&&uat.user_intents==1&&uat.rollback_intents==1&&uat.commits==2&&!grants);assert(uat.intent_size==192&&!memcmp(uat.intent,"PFR0001",7)&&uat.intent[8]==3&&uat.intent[172]==1&&uat.intent[180]==2&&uat.records[uat_find("quick_radio")].value[2]==1);}
  else if(!strcmp(test,"radio-missing-rollback")){uat.count=0;enable_bad=true;paper_enable();assert(uat.rollback_intents==1&&uat.intent[9]==RISC_ATOMIC_DELETE&&uat_find("quick_radio")<0&&!grants);}
  else if(!strcmp(test,"radio-submit-fail")){uat.submit_error=RISC_ATOMIC_IO;paper_enable();assert(!controls&&!uat.commits&&!grants);}
  else if(!strcmp(test,"radio-uncertain")){uat.receipt_error=RISC_ATOMIC_UNCERTAIN;paper_enable();assert(uncertain&&!controls&&grants==2&&!uat.rollback_intents);}
  else if(!strcmp(test,"radio-restore-fail")){enable_bad=rollback_bad=true;paper_enable();assert(uncertain&&uat.rollback_intents==1&&grants==2);}
  else assert(0);
 }
 printf("Actual BLE controller %s PASS\n",test);return 0;
}
