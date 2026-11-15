#define main resident_regressions
#include "alarms_resident_test.c"
#undef main
static unsigned submits,collects,refreshes,pending_count,legacy_applies;static int submitted_rc,receipt_rc;
static bool bad_token,lose_owner,plan_invalid,not_applicable,race_after_commit;static uint8_t current,desired;
static int32_t pref_state(void*c,risc_key_value_atomic_state_v1*s){(void)c;*s=(risc_key_value_atomic_state_v1){.struct_size=sizeof(*s),.flags=RISC_PREFERENCE_STORE_COPIED_INPUT,.namespace_id=1,.max_mutations=4,.max_value_bytes=64,.max_group_bytes=512,.max_intent_bytes=176,.revision=7};return 0;}
static int32_t pref_get(void*c,uint64_t r,const char*k,void*b,uint32_t z,uint32_t*n){(void)c;assert(r==7&&!strcmp(k,"alarm_volume")&&z>=1);*(uint8_t*)b=current;*n=1;return 0;}
static int32_t pref_submit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*m,uint32_t n,const void*i,uint32_t size,uint64_t*t){(void)c;assert(r==7&&n==1&&m[0].kind==RISC_ATOMIC_PUT&&!strcmp(m[0].key,"alarm_volume")&&m[0].size==1&&size==176&&!memcmp(i,"PFI0001",7));assert(state.pending&&present&&writes>=1);desired=*(const uint8_t*)m[0].value;submits++;*t=submitted_rc==0||bad_token?99:0;return submitted_rc;}
static int32_t pref_collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*q){(void)c;assert(t==99);collects++;if(lose_owner){replaced=true;return RISC_ATOMIC_PENDING;}if(pending_count){pending_count--;return RISC_ATOMIC_PENDING;}*q=(risc_key_value_atomic_receipt_v1){sizeof(*q),receipt_rc,8,receipt_rc==0?44:0};if(receipt_rc==0)current=race_after_commit?50:desired;return 0;}
static int32_t pref_cancel(void*c,uint64_t t){(void)c;(void)t;assert(0);return -1;}
static int32_t copied_plan(void*c,const alarm_control_command_v1*cmd,alarm_control_preference_plan_v1*p){(void)c;assert(cmd==&state.command&&p->struct_size==sizeof(*p));if(not_applicable)return ALARM_CONTROL_NOT_APPLICABLE;*p=(alarm_control_preference_plan_v1){.struct_size=sizeof(*p),.namespace_id=plan_invalid?9:1,.before_size=1,.after_size=1,.key="alarm_volume",.before={50},.after={60}};return 0;}
static int32_t reconcile(void*c,const alarm_control_command_v1*cmd){(void)c;assert(cmd==&state.command);if(current!=60&&!not_applicable)return ALARM_CONTROL_STALE;refreshes++;return 0;}
static int32_t legacy_apply(void*c,const alarm_control_command_v1*cmd){legacy_applies++;assert(not_applicable&&"semantic preference path must never call legacy apply");return reconcile(c,cmd);}
static risc_preference_store_v1 prefs={1,sizeof(prefs),NULL,pref_state,pref_get,pref_submit,pref_collect,pref_cancel};
static alarm_control_preferences_v1 extended;
static void ready(void){setup();preferences=&prefs;submits=collects=refreshes=pending_count=legacy_applies=0;submitted_rc=receipt_rc=0;bad_token=lose_owner=plan_invalid=not_applicable=race_after_commit=false;current=50;desired=0;extended=(alarm_control_preferences_v1){.base=service,.tag=ALARM_CONTROL_PREFERENCES_TAG,.version=1,.preference_plan=copied_plan,.reconcile_preferences=reconcile};extended.base.struct_size=sizeof(extended);extended.base.apply=legacy_apply;control=&extended.base;state.pending=1;state.command.struct_size=sizeof(state.command);memset(state.command.bytes,0x42,sizeof(state.command.bytes));}
int main(void){
 resident_regressions();
 ready();pending_count=2;assert(apply_pending()&&!state.pending&&submits==1&&collects==3&&refreshes==1&&current==60&&writes==2&&!retains);
 ready();receipt_rc=RISC_ATOMIC_UNCERTAIN;assert(apply_pending()&&state.pending&&state.pending_result==ALARM_CONTROL_UNCERTAIN&&submits==1&&!refreshes&&current==50&&writes==1);assert(!memcmp(state.command.bytes,bytes+RISC_SCENE_STATE_HEADER+36,96));
 ready();receipt_rc=RISC_ATOMIC_CONFLICT;assert(apply_pending()&&state.pending&&state.pending_result==ALARM_CONTROL_STALE&&!refreshes);
 ready();current=51;assert(apply_pending()&&state.pending&&state.pending_result==ALARM_CONTROL_STALE&&!submits&&!collects);
 ready();current=60;assert(apply_pending()&&!state.pending&&!submits&&refreshes==1); /* restart after committed write */
 ready();submitted_rc=RISC_ATOMIC_BUSY;assert(apply_pending()&&state.pending&&state.pending_result==ALARM_CONTROL_PENDING&&submits==1&&!collects&&!refreshes);
 ready();submitted_rc=RISC_ATOMIC_IO;bad_token=true;assert(!apply_pending()&&terminal&&retains==1&&state.pending&&!refreshes);
 ready();lose_owner=true;assert(!apply_pending()&&terminal&&state.pending&&submits==1&&collects==1&&!releases&&!refreshes);
 ready();write_fail=true;assert(!apply_pending()&&state.pending&&!submits&&!collects&&!refreshes);
 ready();plan_invalid=true;assert(apply_pending()&&state.pending&&state.pending_result==ALARM_CONTROL_INVALID&&!submits);
 ready();not_applicable=true;assert(apply_pending()&&!state.pending&&!submits&&refreshes==1&&legacy_applies==1);
 ready();receipt_rc=RISC_ATOMIC_IO;assert(apply_pending()&&state.pending&&state.pending_result==ALARM_CONTROL_UNCERTAIN&&!refreshes&&!retains);
 ready();receipt_rc=RISC_ATOMIC_UNCERTAIN;assert(apply_pending()&&state.pending&&submits==1);current=60;receipt_rc=0;assert(apply_pending()&&!state.pending&&submits==1&&refreshes==1); /* committed uncertainty reconciles without resubmit */
 ready();receipt_rc=RISC_ATOMIC_UNCERTAIN;assert(apply_pending()&&state.pending&&submits==1);uint8_t command_copy[96];memcpy(command_copy,state.command.bytes,96);fresh_model();storage_known=false;assert(load_checkpoint()&&state.pending&&!memcmp(command_copy,state.command.bytes,96));current=60;assert(apply_pending()&&!state.pending&&submits==1); /* checkpoint reload */
 ready();race_after_commit=true;assert(apply_pending()&&state.pending&&state.pending_result==ALARM_CONTROL_STALE&&submits==1&&collects==1&&!refreshes&&current==50);
 puts("Alarms semantic preferences: 15 transaction/journal/conflict/restart/fence scenarios passed");
}
