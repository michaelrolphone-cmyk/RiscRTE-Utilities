#define UTILITY_CONTEXTS_DIRECT_TEST
#include "app_test.c"
int main(int argc,char **argv){
 assert(argc==2);const char *test=argv[1];uat_reset();native_alive=true;ctx_runtime=&app_runtime;ctx_service=&service.base;cs_client=(portable_contexts_client){.api=&service.base,.runtime=&app_runtime};
 ctx_fp_valid=true;ctx_fp=(contexts_fingerprint_status_v1){.struct_size=sizeof(ctx_fp),.sources=3};cs_source_mask=3;
 if(!strcmp(test,"enable")){cu_action(CU_ROW_AUTO);assert(ctx_enabled&&uat.commits==1&&!kv_writes);const uint8_t b[]={'C',1,1,0xa4};uat_assert_user("contexts_on",b,4,1);cu_action(CU_ROW_AUTO);assert(!ctx_enabled&&uat.commits==2);}
 else if(!strcmp(test,"sources")){cu_action(CU_ROW_SOURCE+1);assert(uat.commits==1&&ctx_fp.sources==3);uint8_t b=1;uat_assert_user("context_sources",&b,1,1);}
 else if(!strcmp(test,"timing")){cu_action(CU_ROW_SOURCE+2);assert(uat.commits==1);uint8_t b=1;uat_assert_user("context_timing",&b,1,1);}
 else if(!strcmp(test,"busy")){uat.submit_error=RISC_ATOMIC_BUSY;cu_action(CU_ROW_AUTO);assert(!ctx_enabled&&!uat.commits&&uat.submits==1);}
 else if(!strcmp(test,"delayed")){uat.pending_polls=3;cu_action(CU_ROW_AUTO);assert(ctx_enabled&&uat.commits==1&&uat.yields==3);}
 else if(!strcmp(test,"exit")){native_exit_pending=true;cu_action(CU_ROW_AUTO);assert(!ctx_enabled&&!uat.commits&&!uat.submits);}
 else if(!strcmp(test,"retained")){uat.collect_error=RISC_ATOMIC_IO;cu_action(CU_ROW_AUTO);assert(cs_terminal&&ctx_preference.edit.token&&ctx_preference.grant.api&&app_acquires>app_releases);}
 else if(!strncmp(test,"automatic-",10)){
  cs_client.rules_available=true;cs_client.settings_valid=ctx_enabled=true;
  for(unsigned f=0;f<CR_SETTINGS;f++){automatic_result.settings[f]=CR_KEEP;automatic_result.winners[f]=-1;}
  if(!strcmp(test,"automatic-noop")){assert(cs_rules_tick()&&automatic_evaluations==1&&!uat.submits);}
  else {
   automatic_result.changed=0x274u;automatic_result.settings[CR_SLEEP]=30;automatic_result.settings[CR_WIFI]=0;automatic_result.settings[CR_BLUETOOTH]=1;automatic_result.settings[CR_BRIGHTNESS]=7;automatic_result.settings[CR_DND]=1;
   for(unsigned f=0;f<CR_SETTINGS;f++)if(automatic_result.changed&(1u<<f))automatic_result.winners[f]=0;
   if(!strcmp(test,"automatic-low"))cs_client.low_battery=true;
   else if(!strcmp(test,"automatic-partial"))uat.fail_on_submit=3;
   else if(!strcmp(test,"automatic-retained"))uat.collect_error=RISC_ATOMIC_IO;
   else if(!strcmp(test,"automatic-mask")){automatic_result.changed|=1u<<CR_VOLUME;automatic_result.settings[CR_VOLUME]=8;automatic_result.winners[CR_VOLUME]=0;}
   else if(!strcmp(test,"automatic-rgb")){
    automatic_result.changed|=(1u<<CR_POWER)|(1u<<CR_DEEP)|(1u<<CR_VOLUME)|(1u<<CR_NOTIFY);
    automatic_result.settings[CR_POWER]=1;automatic_result.settings[CR_DEEP]=600;automatic_result.settings[CR_VOLUME]=8;automatic_result.settings[CR_NOTIFY]=1;
    for(unsigned f=0;f<CR_SETTINGS;f++)if(automatic_result.changed&(1u<<f))automatic_result.winners[f]=0;
   }
   else assert(!strcmp(test,"automatic-order"));
   bool ok=cs_rules_tick();
   if(!strcmp(test,"automatic-retained")){assert(!ok&&cs_terminal&&ctx_automatic.operation.token&&ctx_automatic_grant.api&&!automatic_reports);}
   else {assert(ok&&!kv_writes&&!uat.user_intents&&!uat.rollback_intents&&automatic_reports==1);
    if(!strcmp(test,"automatic-low")){assert(uat.commits==3&&!strcmp(uat.keys[0],"quick_radio")&&!strcmp(uat.keys[2],"alert_dnd")&&automatic_report_status[0]==CR_PARTIAL);}
    else if(!strcmp(test,"automatic-rgb")){
     assert(uat.commits==10&&automatic_report_status[0]==CR_APPLIED);
     for(unsigned i=0;i<uat.commits;i++)assert(uat.boundaries[i]==PORTABLE_AUTOMATIC_CONTEXTS_EDITOR);
     assert(!strcmp(uat.keys[0],"sleep_mode")&&!strcmp(uat.keys[2],"sleep_deep")&&!strcmp(uat.keys[6],"alarm_volume"));
    }
    else {assert(uat.submits==5&&!strcmp(uat.keys[0],"sleep_idle")&&!strcmp(uat.keys[1],"quick_radio")&&!strcmp(uat.keys[2],"quick_radio")&&!strcmp(uat.keys[3],"brightness")&&!strcmp(uat.keys[4],"alert_dnd"));for(unsigned i=0;i<5;i++)assert(uat.boundaries[i]==PORTABLE_AUTOMATIC_CONTEXTS_EDITOR);
     if(!strcmp(test,"automatic-partial"))assert(uat.commits==4&&automatic_report_status[0]==CR_PARTIAL);
     else if(!strcmp(test,"automatic-mask"))assert(uat.commits==5&&automatic_report_status[0]==CR_PARTIAL);
     else assert(uat.commits==5&&automatic_report_status[0]==CR_APPLIED);
    }
   }
  }
 }
 else assert(0);
 if(!cs_terminal)assert(app_acquires==app_releases&&!ctx_preference.grant.api&&!ctx_preference.edit.token);
 printf("Actual Contexts controller %s PASS\n",test);return 0;
}
