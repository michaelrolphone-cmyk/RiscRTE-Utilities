/* Contexts owns its controller; the shared host alone owns text/key state.
 * Never accept a draft before close, capability release and scene resumption. */
#include "PortableTextInputResidentCheckpoint.h"
static portable_text_client cs_text;
static portable_text_checkpoint_client cs_text_checkpoint;
static bool cs_text_unavailable,cs_text_policy_continuity;
static risc_scene_navigation_v1 cs_text_navigation;
static char cs_text_accepted[33];
static unsigned cs_text_outcome;

static bool cs_text_busy(void){
 return cs_text.active||cs_text.acquired||cs_text.suspended;
}
static void cs_text_reset(void){
 cs_text=(portable_text_client){0};
 cs_text_checkpoint=(portable_text_checkpoint_client){0};cs_text_unavailable=false;cs_text_policy_continuity=false;
 cs_text_navigation=(risc_scene_navigation_v1){.struct_size=sizeof(cs_text_navigation)};
 memset(cs_text_accepted,0,sizeof(cs_text_accepted));cs_text_outcome=0;cs_text_retry=false;
}
bool portable_text_adapter_retained(void){return cs_terminal;}
void portable_text_adapter_retain(void){cs_retain();}
int portable_text_adapter_attention(void){
 if(!cs_alive())return -1;
 bool exiting=cs_host_exit();
 return cs_alive()?(exiting?1:0):-1;
}
int portable_text_adapter_suspend(void){
 if(!cs_alive())return RISC_TEXT_ENTRY_RETAINED;
 if(!cs_session){cs_retain();return RISC_TEXT_ENTRY_RETAINED;}
 cs_text_navigation=(risc_scene_navigation_v1){.struct_size=sizeof(cs_text_navigation)};
 uint32_t flags=0;
 int rc=cs_scene->snapshot(cs_scene->context,cs_session,&cs_text_navigation,&flags);
 if(!cs_alive())return RISC_TEXT_ENTRY_RETAINED;
 if(rc){cs_retain();return RISC_TEXT_ENTRY_RETAINED;}
 if(flags&RISC_SCENE_ACTIVITY)cs_resident.activity=true;
 if(!cs_close_scene())return RISC_TEXT_ENTRY_RETAINED;
 return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_resume(void){
 if(!cs_alive())return RISC_TEXT_ENTRY_RETAINED;
 /* Native or shared-host Home must not resurrect the cancelled editor. */
 bool exiting=ctx_exit||cs_host_exit();
 if(!cs_alive())return RISC_TEXT_ENTRY_RETAINED;
 if(exiting)return RISC_TEXT_ENTRY_OK;
 cs_declare();
 if(!cs_open_scene(&cs_text_navigation)){cs_retain();return RISC_TEXT_ENTRY_RETAINED;}
 ctx_dirty=true;return RISC_TEXT_ENTRY_OK;
}
/* The reason-bound POLL promise alone permits passive input continuity.
 * Possible sleep releases all input custody before native power admission. */
static bool cs_text_policy(void){
 if(!cs_alive()||cs_host_exit())return false;
 int rc=portable_text_checkpoint_pause_mode(&cs_text,&cs_text_checkpoint,cs_text_policy_continuity);
 if(rc==RISC_TEXT_ENTRY_BUSY){
  /* New activity invalidates the old policy request. Only a fresh resident
   * POLL carrying that activity may request another policy handoff. */
  cs_resident.policy=false;cs_text_policy_continuity=false;return true;
 }
 if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
 if(rc!=RISC_TEXT_ENTRY_OK||!cs_alive()||cs_host_exit())return false;
 if(!cs_catalog_close())return false;
 bool training=cs_client.foreground_learning;
 if(!portable_contexts_pause(&cs_client)||!portable_contexts_close(&cs_client)){cs_retain();return false;}
 ctx_service=NULL;
 if(!cs_alive()||cs_host_exit())return false;
 uint32_t resume_flags=0;
 rc=portable_text_resident_policy(&cs_resident,cs_text_policy_continuity,&resume_flags);
 if(!cs_alive())return false;
 if(rc==RISC_RESIDENT_RETAINED){cs_retain();return false;}
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY)return false;
 if(cs_host_exit()||!cs_alive())return false;
 if(!cs_backend_open())return false;
 cs_client.foreground_learning=training;ctx_dirty=true;
 if(!cs_alive()||cs_host_exit())return false;
 /* Repaint as before. Possible sleep uses predecessor wake neutralization;
  * the exact no-sleep POLL promise instead preserves queued input. */
 rc=portable_text_checkpoint_resume(&cs_text,&cs_text_checkpoint,resume_flags);
 if(rc!=RISC_TEXT_ENTRY_OK||!cs_alive()||cs_host_exit())return false;
 return true;
}
static bool cs_text_resident_step(void){
 if(!cs_resident.enabled||!cs_text.active||cs_text.closing||!cs_text_checkpoint.enabled)return true;
 if(cs_text_checkpoint.pausing)return cs_text_policy();
 risc_text_entry_checkpoint_state_v1 state={.struct_size=sizeof(state)};
 int rc=portable_text_checkpoint_snapshot(&cs_text,&cs_text_checkpoint,&state);
 if(rc==RISC_TEXT_ENTRY_BUSY)return true;
 if(rc!=RISC_TEXT_ENTRY_OK||!cs_alive()||cs_host_exit())return false;
 uint32_t flags=0;
 if(state.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING)flags|=RISC_SCENE_PRESENTING;
 if(state.flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)flags|=RISC_SCENE_INPUT_BUSY;
 if(state.flags&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY)flags|=RISC_SCENE_ACTIVITY;
 bool working=cs_client.foreground_learning||((cu_page==CU_LEARN||(cu_page==CU_CANCEL&&cu_return==CU_LEARN))&&(cu_learning.state==CONTEXTS_LEARN_READY||cu_learning_saved));
 uint32_t work=working?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0;
 rc=portable_text_resident_poll(&cs_resident,flags,work,&cs_text_policy_continuity);
 if(!cs_alive())return false;
 if(rc==RISC_RESIDENT_RETAINED){cs_retain();return false;}
 if(rc==RISC_RESIDENT_EXIT||cs_host_exit())return false;
 if(cs_resident.policy&&!working&&!(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY)))return cs_text_policy();
 return cs_alive();
}
static bool cs_text_step(void){
 if(!cs_alive())return false;
 if(!cs_text_busy()){
  if(cu_page!=CU_NAME||cs_text_retry)return true;
  memset(cs_text_accepted,0,sizeof(cs_text_accepted));cs_text_outcome=0;
  cs_text_checkpoint=(portable_text_checkpoint_client){0};cs_text_unavailable=false;cs_text_policy_continuity=false;
  int rc=portable_text_client_begin(&cs_text,ctx_runtime,
      cu_name_reason==4?"MESSAGE":"NAME",cu_name,cu_name_reason==4?33u:17u);
  if(rc==RISC_TEXT_ENTRY_RETAINED||!cs_alive())return false;
  if(rc==RISC_TEXT_ENTRY_OK&&cs_resident.enabled){
   rc=portable_text_checkpoint_enable(&cs_text,&cs_text_checkpoint);
   if(rc==RISC_TEXT_ENTRY_RETAINED||!cs_alive())return false;
   if(rc!=RISC_TEXT_ENTRY_OK){cs_text_unavailable=true;cs_text.closing=true;}
  }
  if(rc!=RISC_TEXT_ENTRY_OK){cs_text_retry=true;cu_note("TEXT INPUT UNAVAILABLE / RETRY");}
  if(cs_host_exit()||!cs_alive())return false;
  return true;
 }
 if(cs_text_checkpoint.pausing&&!cs_text.closing)return cs_text_policy();
 if(!cs_text.closing){
  risc_text_entry_state_v1 state={.struct_size=sizeof(state)};
  int rc=portable_text_client_poll(&cs_text,&state);
  if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
  if(rc!=RISC_TEXT_ENTRY_OK||!cs_alive())return false;
  if(state.state==RISC_TEXT_ENTRY_EDITING)return true;
  cs_resident.activity=true;
  cs_text_outcome=state.state;
  if(state.flags&RISC_TEXT_ENTRY_HOME_CANCEL){ctx_exit=true;cs_launch[0]=0;}
  if(state.state==RISC_TEXT_ENTRY_ACCEPTED&&!ctx_exit)
   memcpy(cs_text_accepted,state.text,strlen(state.text)+1);
  /* Avoid retaining a second copy of the host's draft on our stack. */
  (void)portable_text_client_state_result(&state,0);
 }
 int rc=portable_text_client_close(&cs_text);
 if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
 if(rc!=RISC_TEXT_ENTRY_OK||!cs_alive())return false;
 bool exiting=ctx_exit||cs_host_exit();
 if(!cs_alive())return false;
 if(!exiting&&!cs_text_unavailable){
  if(cs_text_outcome==RISC_TEXT_ENTRY_ACCEPTED){
   memcpy(cu_name,cs_text_accepted,sizeof(cu_name));cu_name_done();
   /* Empty/invalid text and persistence errors retain the existing retry path. */
   cs_text_retry=cu_page==CU_NAME;
  }else{cu_back();cs_text_retry=false;}
 }
 memset(cs_text_accepted,0,sizeof(cs_text_accepted));cs_text_outcome=0;
 cs_text_checkpoint=(portable_text_checkpoint_client){0};cs_text_unavailable=false;cs_text_policy_continuity=false;
 return cs_alive();
}
static bool cs_text_cleanup(void){
 /* Every ordinary exit cancels, including a native Home between acceptance
  * and close completion. Do not run controller persistence from cleanup. */
 ctx_exit=true;cs_launch[0]=0;
 memset(cs_text_accepted,0,sizeof(cs_text_accepted));cs_text_outcome=0;
 for(unsigned i=0;cs_text_busy()&&i<1000;i++){
  int rc=portable_text_client_close(&cs_text);
  if(rc==RISC_TEXT_ENTRY_OK)return true;
  if(rc!=RISC_TEXT_ENTRY_AGAIN||!cs_alive())return false;
  ctx_runtime->yield_ms(5);
 }
 if(cs_text_busy()){cs_retain();return false;}
 return cs_alive();
}
