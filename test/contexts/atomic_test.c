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
 else if(!strcmp(test,"automatic-gate")){cs_client.rules_available=true;cs_client.settings_valid=ctx_enabled=true;assert(cs_rules_tick()&&!uat.submits&&!kv_writes);}
 else assert(0);
 if(!cs_terminal)assert(app_acquires==app_releases&&!ctx_preference.grant.api&&!ctx_preference.edit.token);
 printf("Actual Contexts controller %s PASS\n",test);return 0;
}
