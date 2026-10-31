/* Shared semantic Audio Spectrum. Capture/DSP and storage are independent of
 * presentation cadence; the optional providers own physical resources. */
#include "RiscSceneComponentsV1.h"
#include "RiscSceneResidentV1.h"
#include "RiscResidentAttentionV1.h"
#include "RiscSceneCanvasV1.h"
#include "RiscTextEntryV1.h"
#include "PortableTextInputResidentCheckpoint.h"
#include "spectrum_scene_background.h"
#define PORTABLE_CONTEXTS_CLIENT 1
#define portable_contexts_service spectrum_scene_background_service
#define portable_contexts_models_save spectrum_scene_background_models_save
#define portable_fingerprint_clock spectrum_scene_background_clock
#include "spectrum_scene_model.inc"
#include "spectrum_scene_shared.inc"
#include "spectrum_scene_presenter.inc"
#include "spectrum_scene_canvas.h"
static bool ss_terminal,ss_leave,ss_explicit,ss_chart_unavailable;
static bool ss_background_pause(void);
static bool ss_background_step(bool allow);
static bool ss_background_close(void);
static bool ss_background_init(void);
static bool ss_bg_alive(void *context){(void)context;return spectrum_domain_alive();}
static void ss_bg_retain(void *context){(void)context;spectrum_domain_retain();}
static bool ss_background_init(void){return spectrum_scene_background_init(runtime,ss_bg_alive,ss_bg_retain,NULL);}
static bool ss_background_pause(void){return spectrum_scene_background_pause();}
static bool ss_background_step(bool allow){return spectrum_scene_background_step(allow);}
static bool ss_background_close(void){return spectrum_scene_background_close();}
static bool spectrum_domain_storage_boundary(void){return ss_background_pause();}
static risc_runtime_capability_v1 ss_scene_grant,ss_canvas_grant,ss_text_grant;
static const risc_scene_api_v1 *ss_scene;
static const risc_scene_components_api_v1 *ss_components;
static const risc_scene_lifecycle_api_v1 *ss_lifecycle;
static const risc_canvas_api_v1 *ss_canvas;
static const risc_text_entry_api_v1 *ss_text;
static risc_scene_resident_v1 ss_resident;
static risc_resident_attention_client_v1 ss_attention;
static uint32_t ss_attention_pending,ss_attention_at;
static bool ss_attention_available,ss_attention_checked,ss_attention_stopped;
static uint32_t ss_attention_rejected;
static bool ss_manual_neutral,ss_attention_back;
static risc_components_document_v1 ss_document,ss_candidate;
static risc_scene_navigation_v1 ss_nav;
static uint64_t ss_scene_session,ss_canvas_session,ss_receipt,ss_text_session,ss_sequence;
static uint32_t ss_revision,ss_flags,ss_encoding;
static spectrum_canvas_layout ss_layout;
static spectrum_canvas_input ss_input;
static risc_text_entry_state_v1 ss_text_state;
static unsigned ss_text_page;
static bool ss_text_closing,ss_text_home,ss_text_shared,ss_text_presenting;
static const risc_text_entry_api_v1_checkpoint *ss_text_checkpoint;
static bool ss_text_pausing,ss_text_paused,ss_text_continuity;
static bool spectrum_domain_alive(void){
 if(ss_terminal||!runtime)return false;
 if(risc_runtime_get_api(1)!=runtime){ss_terminal=true;return false;}
 return true;
}
static void spectrum_domain_retain(void){
 if(!spectrum_domain_alive())return;
 ss_terminal=true;
 if(runtime->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE&&runtime->retain_invocation)
  (void)runtime->retain_invocation();
}
static uint32_t ss_millis(void){
 uint32_t now=0;if(!spectrum_domain_alive())return 0;
 bool ok=runtime->monotonic_ms(&now);
 if(!spectrum_domain_alive())return 0;
 if(!ok)spectrum_domain_retain();
 return now;
}
static const struct spectrum_domain_clock ss_clock={ss_millis};
static bool ss_capture_active(void){return capture_requested&&running&&owned&&!uncertain;}
static bool ss_background_allowed(void){
 /* Learning and unsaved models inhibit idle, not safe RF/BLE progression.
  * Storage boundaries explicitly pause these owners before every call. */
 return !ss_text_pausing&&!ss_text_paused&&!uncertain&&!temporal_retained()&&
  !(ss_flags&RISC_SCENE_CLOSING);
}
static bool ss_background_deferred(void){return ss_text_session&&(ss_text_presenting||ss_text_closing);}
static bool ss_resident_bind(void){
 ss_resident=(risc_scene_resident_v1){0};
 ss_attention=(risc_resident_attention_client_v1){0};
 ss_attention_pending=ss_attention_at=0;
 ss_attention_rejected=0;
 ss_manual_neutral=ss_attention_back=false;
 ss_attention_available=ss_attention_checked=ss_attention_stopped=false;
 if(runtime->struct_size<RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE||!runtime->resident_shell)return true;
 ss_attention.base.struct_size=sizeof(ss_attention);
 bool found=runtime->resident_shell(&ss_attention.base);
 if(!spectrum_domain_alive())return false;
 if(!found)return true;
 const risc_resident_client_v1 *c=&ss_attention.base;
 if(c->api_version!=1||c->struct_size<sizeof(*c)||!c->invocation||c->role!=RISC_RESIDENT_ROLE_FOREGROUND||!c->checkpoint){spectrum_domain_retain();return false;}
 ss_resident.client=*c;ss_resident.enabled=true;
 ss_attention_available=risc_resident_attention_available_v1(&ss_attention);
 return true;
}
static bool ss_result(int32_t result){
 if(!spectrum_domain_alive())return false;
 if(result==RISC_SCENE_RETAINED){spectrum_domain_retain();return false;}
 return true;
}
static bool ss_canvas_receipt(void){
 if(!ss_receipt)return true;
 risc_canvas_receipt_v1 q={.struct_size=sizeof(q),.receipt=ss_receipt};
 int rc=ss_canvas->poll(ss_canvas->context,ss_canvas_session,ss_receipt,&q);
 if(!ss_result(rc))return false;
 if(rc==RISC_CANVAS_AGAIN)return false;
 if(rc!=RISC_CANVAS_OK||q.struct_size!=sizeof(q)||q.receipt!=ss_receipt||q.reserved){spectrum_domain_retain();return false;}
 if(q.state==RISC_CANVAS_QUEUED||q.state==RISC_CANVAS_PRESENTING)return false;
 if(q.state!=RISC_CANVAS_COMPLETE&&q.state!=RISC_CANVAS_SUPERSEDED){spectrum_domain_retain();return false;}
 rc=ss_canvas->forget_receipt(ss_canvas->context,ss_canvas_session,ss_receipt);
 if(!ss_result(rc))return false;
 if(rc!=RISC_CANVAS_OK){spectrum_domain_retain();return false;}
 ss_receipt=0;return true;
}
static bool ss_close_scene(void){
 for(unsigned i=0;ss_scene_session&&i<256&&spectrum_domain_alive();i++){
  int rc=ss_scene->close(ss_scene->context,ss_scene_session);if(!ss_result(rc))return false;
  if(rc==RISC_SCENE_OK){ss_scene_session=ss_sequence=0;ss_flags=0;return true;}
  if(rc!=RISC_SCENE_AGAIN){spectrum_domain_retain();return false;}runtime->yield_ms(1);
 }
 if(ss_scene_session){spectrum_domain_retain();return false;}return spectrum_domain_alive();
}
static bool ss_close_canvas(void){
 for(unsigned i=0;ss_canvas_session&&i<256&&spectrum_domain_alive();i++){
  if(ss_receipt&&!ss_canvas_receipt()){if(!spectrum_domain_alive())return false;runtime->yield_ms(1);continue;}
  int rc=ss_canvas->close(ss_canvas->context,ss_canvas_session);if(!ss_result(rc))return false;
  if(rc==RISC_CANVAS_OK){ss_canvas_session=0;memset(&ss_input,0,sizeof(ss_input));return true;}
  if(rc!=RISC_CANVAS_AGAIN){spectrum_domain_retain();return false;}runtime->yield_ms(1);
 }
 if(ss_canvas_session){spectrum_domain_retain();return false;}return spectrum_domain_alive();
}
static bool ss_close_ui(void){return ss_close_scene()&&ss_close_canvas();}
static bool ss_snapshot(void){
 risc_scene_navigation_v1 n={.api_version=1,.struct_size=sizeof(n)};uint32_t flags=0;
 int rc=ss_scene->snapshot(ss_scene->context,ss_scene_session,&n,&flags);if(!ss_result(rc))return false;
 if(rc!=RISC_SCENE_OK||n.api_version!=1||n.struct_size!=sizeof(n)||n.depth!=1||n.routes[0]!=1||n.reserved||(flags&~(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING|RISC_SCENE_ACTIVITY))){spectrum_domain_retain();return false;}
 for(unsigned i=1;i<RISC_SCENE_MAX_DEPTH;i++)if(n.routes[i]||n.focus[i]){spectrum_domain_retain();return false;}
 ss_nav=n;ss_flags=flags;return true;
}
static bool ss_publish_components(void){
 if(!ss_close_canvas())return false;
 if(ss_scene_session&&!ss_snapshot())return false;
 if(ss_scene_session&&(ss_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING)))return true;
 if(ss_scene_session&&!ss_lifecycle&&!ss_explicit)return true;
 spectrum_presenter_build(&ss_candidate);if(!spectrum_domain_alive())return false;
 if(ss_chart_unavailable&&page==PAGE_MAIN)sp_text(&ss_candidate,"Charts unavailable","Shared canvas is not available; controls and saved models remain accessible.");
 if(!ss_resident.enabled)sp_text(&ss_candidate,"Shared controls unavailable","No resident controls provider.");
 else if(!ss_attention_available)sp_text(&ss_candidate,"Capture attention unavailable","This resident cannot safely notify alarms or power-key intent during recording.");
 if(!ss_lifecycle)sp_text(&ss_candidate,"Automatic refresh unavailable","Use an explicit action to refresh.");
 ss_candidate.revision=++ss_revision;if(!ss_revision){spectrum_domain_retain();return false;}
 int rc;
 if(!ss_scene_session){
  uint32_t focus=ss_nav.focus[0];bool valid=ss_document.screen_key==ss_candidate.screen_key;
  if(valid&&focus){valid=false;for(unsigned i=0;i<ss_candidate.node_count;i++)if(ss_candidate.nodes[i].id==focus&&ss_candidate.nodes[i].action)valid=true;}
  ss_nav=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(ss_nav),.depth=1,.routes={1},.focus={valid?focus:0}};
  rc=ss_components->open(ss_scene->context,&ss_candidate,&ss_nav,&ss_scene_session);if(!ss_result(rc))return false;
  if(rc==RISC_SCENE_BUSY||rc==RISC_SCENE_AGAIN){if(ss_scene_session){spectrum_domain_retain();return false;}return true;}
  if(rc!=RISC_SCENE_OK||!ss_scene_session){ss_leave=true;return false;}
  ss_sequence=0;
  if(ss_lifecycle){rc=ss_lifecycle->configure(ss_scene->context,ss_scene_session,ss_resident.enabled?RISC_SCENE_FEATURE_SHARED_CONTROLS:0);if(!ss_result(rc))return false;if(rc!=RISC_SCENE_OK){spectrum_domain_retain();return false;}}
 }else {rc=ss_components->update(ss_scene->context,ss_scene_session,&ss_candidate);if(!ss_result(rc))return false;if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY)return true;if(rc!=RISC_SCENE_OK){spectrum_domain_retain();return false;}}
 ss_document=ss_candidate;spectrum_presenter_commit(&ss_document);dirty=ss_explicit=false;return true;
}
static spectrum_canvas_model ss_chart_model(void){
 return (spectrum_canvas_model){.config=&dsp_config,.preferences=&prefs,.levels=spec_levels,.peaks=peak_levels,.db=spec_db,.waterfall_db=fall_db,.level_count=SPECTRUM_HISTORY_FRAMES,.history=&history[0][0],.history_frames=SPECTRUM_HISTORY_FRAMES,.history_bands=SPECTRUM_HISTORY_BANDS,.history_next=history_next,.history_count=history_count,.palette=palette,.label_colors=label_colors,.labels=labels,.label_active=label_active,.view=view,.running=running,.frozen=frozen,.cursor_visible=cursor_visible,.cursor_hz=cursor_hz,.status=message,.room=cfa_name(CF_ROOM,"UNKNOWN ROOM"),.event=cfa_name(CF_EVENT,"")};
}
static bool ss_publish_canvas(void){
 if(!ss_canvas){ss_chart_unavailable=true;return ss_publish_components();}
 if(!ss_close_scene())return false;
 if(!ss_canvas_session){
  risc_canvas_open_v1 request={sizeof(request),ss_encoding,RISC_CANVAS_RESPONSIVE,0,0};
  int rc=ss_canvas->open(ss_canvas->context,&request,&ss_canvas_session);if(!ss_result(rc))return false;
  if(rc==RISC_CANVAS_UNSUPPORTED&&ss_encoding==RISC_CANVAS_RGBA8&&!ss_canvas_session){ss_encoding=RISC_CANVAS_MONO1;return true;}
  if(rc==RISC_CANVAS_BUSY||rc==RISC_CANVAS_AGAIN){if(ss_canvas_session){spectrum_domain_retain();return false;}return true;}
  if(rc!=RISC_CANVAS_OK||!ss_canvas_session){if(ss_canvas_session){spectrum_domain_retain();return false;}ss_chart_unavailable=true;return ss_publish_components();}
  memset(&ss_input,0,sizeof(ss_input));dirty=true;
 }
 if(ss_receipt&&!ss_canvas_receipt())return spectrum_domain_alive();
 if(!dirty)return true;
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};
 int rc=ss_canvas->begin_frame(ss_canvas->context,ss_canvas_session,&f);if(!ss_result(rc))return false;
 if(rc==RISC_CANVAS_AGAIN||rc==RISC_CANVAS_BUSY){if(f.lease||f.pixels){spectrum_domain_retain();return false;}return true;}
 if(rc!=RISC_CANVAS_OK||!f.lease){spectrum_domain_retain();return false;}
 spectrum_canvas_model model=ss_chart_model();
 if(!spectrum_canvas_render(&f,&model,&ss_layout)){
  rc=ss_canvas->cancel_frame(ss_canvas->context,ss_canvas_session,f.lease);if(!ss_result(rc))return false;
  if(rc!=RISC_CANVAS_OK){spectrum_domain_retain();return false;}ss_chart_unavailable=true;return ss_close_canvas();
 }
 rc=ss_canvas->submit(ss_canvas->context,ss_canvas_session,f.lease,&ss_receipt);if(!ss_result(rc))return false;
 if(rc!=RISC_CANVAS_OK||!ss_receipt){spectrum_domain_retain();return false;}
 dirty=ss_explicit=false;return true;
}
static bool ss_pause(void){
 if(!ss_background_pause())return false;
 pause_capture();if(!spectrum_domain_alive()||temporal_retained())return false;
 return ss_close_ui();
}
static void keyboard_begin(void){
 if(ss_text_session||!spectrum_domain_alive())return;
 ss_text_page=page;ss_text_shared=shared_keyboard||page==PAGE_SHARED_EDIT;
 if(!ss_close_ui())return;
 if(!spectrum_acquire(RISC_TEXT_ENTRY_CAPABILITY,1,0,&ss_text_grant)){if(spectrum_domain_alive())notify("TEXT INPUT UNAVAILABLE / RETRY");return;}
 ss_text=ss_text_grant.api;
 if(ss_text->api_version!=1||ss_text->struct_size<sizeof(*ss_text)||!ss_text->open||!ss_text->poll||!ss_text->close){(void)spectrum_release(&ss_text_grant);notify("TEXT INPUT UNAVAILABLE / RETRY");return;}
 risc_text_entry_request_v1 request={.api_version=1,.struct_size=sizeof(request),.capacity=sizeof(editing.name)};
 if(risc_text_entry_home_reason(ss_text))request.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON;
 snprintf(request.label,sizeof(request.label),"%s",ss_text_shared?"SIGNAL NAME":event_keyboard?"EVENT NAME":signature_keyboard?"SIGNATURE NAME":"LABEL NAME");
 snprintf(request.text,sizeof(request.text),"%s",editing.name);
 int rc=ss_text->open(ss_text->context,&request,&ss_text_session);memset(&request,0,sizeof(request));if(!ss_result(rc))return;
 if(rc!=RISC_TEXT_ENTRY_OK||!ss_text_session){if(ss_text_session||(rc!=RISC_TEXT_ENTRY_BUSY&&rc!=RISC_TEXT_ENTRY_UNAVAILABLE&&rc!=RISC_TEXT_ENTRY_INVALID)){spectrum_domain_retain();return;}(void)spectrum_release(&ss_text_grant);notify("TEXT INPUT UNAVAILABLE / RETRY");return;}
 ss_text_checkpoint=risc_text_entry_checkpoint(ss_text);
 ss_text_pausing=ss_text_paused=ss_text_continuity=false;
 if(ss_text_checkpoint){
  rc=ss_text_checkpoint->checkpoint_enable(ss_text->context,ss_text_session);
  if(!ss_result(rc))return;
  if(rc!=RISC_TEXT_ENTRY_OK){spectrum_domain_retain();return;}
 }
 ss_text_closing=ss_text_home=false;ss_text_presenting=true;ss_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(ss_text_state)};set_page(PAGE_KEYBOARD);
}
static bool ss_text_policy(void){
 if(!ss_text_checkpoint||!ss_resident.enabled||ss_text_closing)return true;
 if(!ss_text_pausing&&!ss_text_paused){
  /* Even POLL crosses the native exit barrier. Inhibition flags do not
   * authorize checkpointing while a microphone or RF capture is owned. */
  risc_text_entry_checkpoint_state_v1 q={.struct_size=sizeof(q)};
  int rc=ss_text_checkpoint->checkpoint_snapshot(ss_text->context,ss_text_session,&q);
  if(!ss_result(rc))return false;
  if(rc==RISC_TEXT_ENTRY_BUSY){ss_text_presenting=true;return true;}
  if(rc!=RISC_TEXT_ENTRY_OK||q.struct_size!=sizeof(q)||(q.flags&~(RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING|RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY|RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY))){spectrum_domain_retain();return false;}
  ss_text_presenting=(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING)!=0;
  if(ss_capture_active()||spectrum_scene_background_capture_active())return true;
  uint32_t flags=(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING?RISC_SCENE_PRESENTING:0)|(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY?RISC_SCENE_INPUT_BUSY:0)|(q.flags&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY?RISC_SCENE_ACTIVITY:0);
  bool pending=ss_capture_active()||cfa_learning_kind||signature_goal||signature_pending||temporal_exit_blocked();
  rc=portable_text_resident_poll(&ss_resident,flags,pending?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0,&ss_text_continuity);
  if(!spectrum_domain_alive())return false;
  if(rc==RISC_RESIDENT_RETAINED){spectrum_domain_retain();return false;}
  if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){ss_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(ss_text_state),.state=RISC_TEXT_ENTRY_CANCELLED,.flags=RISC_TEXT_ENTRY_HOME_CANCEL};ss_text_closing=true;return true;}
  if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY&&rc!=RISC_RESIDENT_DENIED){spectrum_domain_retain();return false;}
  if(!ss_resident.policy)return true;
 }
 if(!ss_text_paused){
  int rc=ss_text_continuity?ss_text_checkpoint->pause(ss_text->context,ss_text_session):ss_text_checkpoint->pause_for_focus_change(ss_text->context,ss_text_session);
  if(!ss_result(rc))return false;
  if(rc==RISC_TEXT_ENTRY_BUSY&&!ss_text_pausing)return true;
  if(rc==RISC_TEXT_ENTRY_AGAIN){ss_text_pausing=true;return true;}
  if(rc!=RISC_TEXT_ENTRY_OK){spectrum_domain_retain();return false;}
  ss_text_pausing=false;ss_text_paused=true;
 }
 if(!ss_background_pause())return false;
 pause_capture();if(!spectrum_domain_alive()||temporal_retained())return false;
 uint32_t flags=0;int rc=portable_text_resident_policy(&ss_resident,ss_text_continuity,&flags);
 if(!spectrum_domain_alive())return false;
 if(rc==RISC_RESIDENT_RETAINED){spectrum_domain_retain();return false;}
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){ss_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(ss_text_state),.state=RISC_TEXT_ENTRY_CANCELLED,.flags=RISC_TEXT_ENTRY_HOME_CANCEL};ss_text_closing=true;return true;}
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_BUSY&&rc!=RISC_RESIDENT_DENIED){spectrum_domain_retain();return false;}
 rc=ss_text_checkpoint->resume(ss_text->context,ss_text_session,flags);
 if(!ss_result(rc))return false;
 if(rc!=RISC_TEXT_ENTRY_OK){spectrum_domain_retain();return false;}
 ss_text_paused=false;portable_audio_capture_resume();return spectrum_domain_alive();
}
static bool ss_text_step(void){
 if(!ss_text_closing&&(ss_text_pausing||ss_text_paused))return ss_text_policy();
 if(!ss_text_closing){
  risc_text_entry_state_v1 q={.struct_size=sizeof(q)};int rc=ss_text->poll(ss_text->context,ss_text_session,&q);if(!ss_result(rc))return false;
  if(rc==RISC_TEXT_ENTRY_AGAIN){ss_text_presenting=true;return true;}
  if(rc!=RISC_TEXT_ENTRY_OK||q.struct_size!=sizeof(q)||q.state>RISC_TEXT_ENTRY_CANCELLED||(q.flags&~(RISC_TEXT_ENTRY_HARDWARE|RISC_TEXT_ENTRY_PRESENTING|RISC_TEXT_ENTRY_HOME_CANCEL))||!memchr(q.text,0,sizeof(q.text))){spectrum_domain_retain();return false;}
  ss_text_presenting=(q.flags&RISC_TEXT_ENTRY_PRESENTING)!=0;
  if(q.state==RISC_TEXT_ENTRY_EDITING)return ss_text_policy();
  if(strlen(q.text)>=sizeof(editing.name)){spectrum_domain_retain();return false;}
  for(unsigned i=0;q.text[i];i++)if((unsigned char)q.text[i]<32||(unsigned char)q.text[i]>126){spectrum_domain_retain();return false;}
  ss_text_state=q;ss_text_closing=true;
 }
 int rc=ss_text->close(ss_text->context,ss_text_session);if(!ss_result(rc))return false;
 if(rc==RISC_TEXT_ENTRY_AGAIN)return true;
 if(rc!=RISC_TEXT_ENTRY_OK){spectrum_domain_retain();return false;}
 ss_text_session=0;ss_text_checkpoint=NULL;ss_text_paused=ss_text_pausing=false;if(!spectrum_release(&ss_text_grant))return false;
 set_page(ss_text_page);shared_keyboard=ss_text_shared;
 if(ss_text_state.state==RISC_TEXT_ENTRY_ACCEPTED){memset(editing.name,0,sizeof(editing.name));memcpy(editing.name,ss_text_state.text,strlen(ss_text_state.text));if(ss_text_shared)shared_keyboard_done();}
 else shared_keyboard=false;
 ss_text_home=(ss_text_state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)!=0;memset(&ss_text_state,0,sizeof(ss_text_state));ss_explicit=dirty=true;
 if(ss_text_home){if(spectrum_model_exit_ready())ss_leave=true;}else if(!ss_attention_pending)portable_audio_capture_resume();
 return spectrum_domain_alive();
}
static void ss_handoff(uint32_t reason){
 if(!ss_resident.enabled){notify("SHARED CONTROLS UNAVAILABLE");return;}
 if(!ss_pause())return;
 uint32_t flags=0;int rc=risc_scene_resident_dispatch_reply_v1(&ss_resident,reason,&flags);
 if(!spectrum_domain_alive())return;
 if(rc==RISC_RESIDENT_RETAINED){spectrum_domain_retain();return;}
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){if(spectrum_model_exit_ready())ss_leave=true;}
 else if(rc==RISC_RESIDENT_OK||rc==RISC_RESIDENT_BUSY||rc==RISC_RESIDENT_DENIED)portable_audio_capture_resume();
 else {spectrum_domain_retain();return;}
 dirty=ss_explicit=true;
}
/* The selected portable Spectrum navigation contract treats a completed short
 * key as Back and ignores long/combined keys. Consume the host's advisory key
 * latch before applying that semantic action; never acknowledge an alarm here. */
static bool ss_attention_manual(void){
 if(!spectrum_domain_alive())return false;
 uint32_t manual=ss_attention_pending&(RISC_RESIDENT_ATTENTION_CONTROLS|RISC_RESIDENT_ATTENTION_EXIT);
 if(!manual)return true;
 ss_attention_rejected=manual;
 int rc=ss_attention.reject_manual(ss_attention.base.invocation,manual);
 if(!spectrum_domain_alive())return false;
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK){spectrum_domain_retain();return false;}
 ss_attention_pending&=~manual;ss_attention_rejected=0;
 if(!ss_attention_back)return true;
 ss_attention_back=false;
 if(ss_text_session){
  ss_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(ss_text_state),.state=RISC_TEXT_ENTRY_CANCELLED};
  ss_text_closing=true;
 }else{
  if(!ss_close_ui())return false;
  if(!spectrum_presenter_back())return false;
  if(spectrum_presenter_exit_requested)ss_leave=true;
 }
 dirty=ss_explicit=true;return spectrum_domain_alive();
}
/* Observation never crosses the native capture exit barrier. Only a copied
 * attention intent authorizes settling owners and the ordinary host handshake. */
static bool ss_attention_step(void){
 if(!spectrum_domain_alive())return false;
 if(!ss_resident.enabled)return true;
 if(!ss_attention_pending&&ss_attention_available){
  uint32_t now=ss_millis();if(!spectrum_domain_alive())return false;
  if(ss_attention_checked&&(uint32_t)(now-ss_attention_at)<10u)return true;
  ss_attention_checked=true;ss_attention_at=now;
  risc_resident_attention_v1 q={.struct_size=sizeof(q)};
  int rc=ss_attention.attention(ss_attention.base.invocation,&q);
  if(!spectrum_domain_alive())return false;
  if(rc==RISC_RESIDENT_BUSY||rc==RISC_RESIDENT_INCOMPATIBLE){
   if(q.struct_size!=sizeof(q)||q.supported_flags||q.pending_flags||q.monotonic_ms){spectrum_domain_retain();return false;}
   if(rc==RISC_RESIDENT_INCOMPATIBLE){ss_attention_available=false;dirty=ss_explicit=true;}
   return true;
  }
  if(rc!=RISC_RESIDENT_OK||q.struct_size!=sizeof(q)||(q.supported_flags&~RISC_RESIDENT_ATTENTION_ALL)||(q.pending_flags&~q.supported_flags)){
   spectrum_domain_retain();return false;
  }
  ss_attention_pending=q.pending_flags;
  ss_attention_rejected=0;
  uint32_t manual=q.pending_flags&(RISC_RESIDENT_ATTENTION_CONTROLS|RISC_RESIDENT_ATTENTION_EXIT);
  ss_attention_back=manual==RISC_RESIDENT_ATTENTION_CONTROLS&&ss_manual_neutral;
  ss_manual_neutral=manual==0;
 }
 if(!ss_attention_pending)return true;
 if(!ss_attention_manual())return false;
 if(ss_attention_rejected||!ss_attention_pending||ss_leave)return true;
 if(!ss_attention_stopped){
  if(!ss_background_pause())return false;
  pause_capture();if(!spectrum_domain_alive()||temporal_retained())return false;
  ss_attention_stopped=true;
 }
 if(ss_text_session){
  /* Alarm attention cancels the editor, not the whole application. */
  ss_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(ss_text_state),.state=RISC_TEXT_ENTRY_CANCELLED};
  ss_text_closing=true;
  if(!ss_text_step())return false;
  if(ss_text_session)return true;
 }
 if(!ss_close_ui())return false;
 int rc=risc_scene_resident_poll_work_v1(&ss_resident,0,RISC_RESIDENT_POLL_INHIBIT_IDLE);
 if(!spectrum_domain_alive())return false;
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK){spectrum_domain_retain();return false;}
 if(!ss_resident.policy)return true;
 uint32_t flags=0;
 rc=risc_scene_resident_dispatch_reply_v1(&ss_resident,RISC_RESIDENT_CHECKPOINT_POLICY,&flags);
 if(!spectrum_domain_alive())return false;
 if(rc==RISC_RESIDENT_BUSY)return true;
 if(rc!=RISC_RESIDENT_OK&&rc!=RISC_RESIDENT_EXIT&&rc!=RISC_RESIDENT_HANDOFF){spectrum_domain_retain();return false;}
 ss_attention_pending=0;ss_attention_stopped=false;
 if(rc==RISC_RESIDENT_EXIT||rc==RISC_RESIDENT_HANDOFF){if(spectrum_model_exit_ready())ss_leave=true;}
 else portable_audio_capture_resume();
 dirty=ss_explicit=true;return spectrum_domain_alive();
}
static void ss_canvas_action(spectrum_canvas_command c){
 if(!c.action||!spectrum_domain_alive())return;
 ss_explicit=true;
 if(c.action==SC_CURSOR){cursor_hz=(uint16_t)c.value;cursor_visible=true;dirty=true;return;}
 if(c.action==SC_HIDE_CURSOR){cursor_visible=false;dirty=true;return;}
 if(c.action==SC_SHARED_CONTROLS){ss_handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS);return;}
 if(!ss_close_ui())return;
 switch(c.action){
 case SC_VIEW:if(c.value>=0&&c.value<=2){view=(unsigned)c.value;lab_edit=false;set_page(PAGE_MAIN);}break;
 case SC_TOGGLE:toggle();break;
 case SC_FREEZE:if(running)freeze();else toggle();break;
 case SC_CONTROLS:set_page(PAGE_CONTROLS);break;
 case SC_LABEL:if(cursor_visible)open_label(-1,cursor_hz);break;
 case SC_BACK:(void)spectrum_presenter_back();if(spectrum_presenter_exit_requested)ss_leave=true;break;
 default:break;
 }
 dirty=true;
}
static void ss_scene_event(const risc_scene_event_v1 *e){
 if(e->struct_size!=sizeof(*e)||!e->sequence||e->sequence<=ss_sequence)return;
 ss_sequence=e->sequence;if(e->document_revision!=ss_document.revision)return;
 if(e->kind==RISC_SCENE_CONTROLS_EVENT){ss_handoff(RISC_RESIDENT_CHECKPOINT_CONTROLS);return;}
 if(e->kind==RISC_SCENE_SUSPEND_EVENT){if(ss_pause()&&spectrum_model_exit_ready())ss_leave=true;return;}
 if(e->kind!=RISC_SCENE_ACTION_EVENT)return;
 /* The presenter validates its committed copy. Close UI before domain actions;
  * guarded storage calls pause RF/BLE only if the action actually needs I/O. */
 if(!ss_close_ui())return;
 if(spectrum_presenter_action(e)){ss_explicit=dirty=true;if(spectrum_presenter_exit_requested)ss_leave=true;}
}
static bool ss_inputs(void){
 if(ss_scene_session){
  if(!ss_snapshot())return false;
  for(unsigned i=0;i<32&&ss_scene_session&&spectrum_domain_alive()&&!ss_leave;i++){
   risc_scene_event_v1 e={.struct_size=sizeof(e)};int rc=ss_scene->next(ss_scene->context,ss_scene_session,&e);if(!ss_result(rc))return false;
   if(rc==RISC_SCENE_OK)ss_scene_event(&e);else if(rc==RISC_SCENE_IDLE||rc==RISC_SCENE_AGAIN)break;else{spectrum_domain_retain();return false;}
  }
 }
 if(ss_canvas_session){
  for(unsigned i=0;i<32&&ss_canvas_session&&spectrum_domain_alive()&&!ss_leave;i++){
   risc_canvas_event_v1 e={.struct_size=sizeof(e)};int rc=ss_canvas->next(ss_canvas->context,ss_canvas_session,&e);if(!ss_result(rc))return false;
   if(rc==RISC_CANVAS_IDLE||rc==RISC_CANVAS_AGAIN)break;
   if(rc!=RISC_CANVAS_OK||e.struct_size!=sizeof(e)||e.reserved){spectrum_domain_retain();return false;}
   ss_canvas_action(spectrum_canvas_event(&e,&ss_layout,&ss_input));
  }
  if(ss_canvas_session&&ss_input.blocked&&spectrum_domain_alive()){
   size_t count=0;uint64_t sequence=0;int rc=ss_canvas->contacts(ss_canvas->context,ss_canvas_session,NULL,0,&count,&sequence);if(!ss_result(rc))return false;
   if(rc!=RISC_CANVAS_OK&&rc!=RISC_CANVAS_AGAIN){spectrum_domain_retain();return false;}
   if(rc==RISC_CANVAS_OK){
    if(sequence<ss_input.sequence){spectrum_domain_retain();return false;}
    if(!count){ss_input.sequence=sequence;spectrum_canvas_neutral(&ss_input);}
   }
  }
 }
 return spectrum_domain_alive();
}
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
__attribute__((visibility("default"))) int app_main(void){
 runtime=risc_runtime_get_api(1);ss_terminal=ss_leave=false;ss_explicit=true;ss_encoding=RISC_CANVAS_RGBA8;app=&ss_clock;
 ss_chart_unavailable=false;ss_scene=NULL;ss_components=NULL;ss_lifecycle=NULL;ss_canvas=NULL;ss_text=NULL;
 ss_scene_session=ss_canvas_session=ss_text_session=ss_receipt=ss_sequence=0;ss_revision=ss_flags=0;
 ss_nav=(risc_scene_navigation_v1){0};ss_document=(risc_components_document_v1){0};
 spectrum_presenter_exit_requested=false;shared_kind=CF_ROOM;shared_selected=-1;shared_offset=shared_focus=shared_targets=0;shared_discard_confirm=false;shared_name[0]=0;
 if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!runtime->acquire||!runtime->release||!runtime->yield_ms||!runtime->monotonic_ms)return 0;
 if(!spectrum_acquire(RISC_SCENE_CAPABILITY,1,0,&ss_scene_grant))return 0;
 ss_scene=ss_scene_grant.api;ss_components=risc_scene_components_get_v1(ss_scene);ss_lifecycle=risc_scene_lifecycle_get_v1(ss_scene);
 if(!ss_components||!ss_scene->next||!ss_scene->snapshot||!ss_scene->close){(void)spectrum_release(&ss_scene_grant);return 0;}
 if(!ss_resident_bind())return 0;
 if(spectrum_acquire(RISC_CANVAS_CAPABILITY,1,0,&ss_canvas_grant)){
  const risc_canvas_api_v1 *candidate=ss_canvas_grant.api;ss_canvas=risc_canvas_get_v1(candidate,candidate->struct_size);
  if(!ss_canvas&&!spectrum_release(&ss_canvas_grant))return 0;
 }else if(spectrum_domain_alive()){
  /* Only clean standalone absence selects the additive semantic UI accessor.
   * Its lifetime remains owned by the already-held scene grant. */
  ss_canvas=risc_scene_canvas_get_v1(ss_scene);
 }
 if(!spectrum_domain_alive()||!ss_background_init())return 0;
 spectrum_model_init();if(!spectrum_domain_alive())return 0;
 while(spectrum_domain_alive()&&!ss_leave){
  if(temporal_retained()||uncertain){spectrum_domain_retain();break;}
  if(!ss_attention_step())break;
  if(ss_leave)break;
  if(ss_attention_pending){if(!ss_attention_stopped)spectrum_model_step();if(!spectrum_domain_alive())break;runtime->yield_ms(1);continue;}
  if(ss_text_session){if(!ss_text_step())break;}
  else {
   bool charts=page==PAGE_MAIN&&view<2&&!lab_edit&&!ss_chart_unavailable;
   if(charts){if(!ss_publish_canvas()&&!spectrum_domain_alive())break;}
   else if(!ss_scene_session||dirty){if(!ss_publish_components()&&!spectrum_domain_alive())break;}
   if(!ss_inputs())break;
  }
  if(!spectrum_domain_alive()||ss_leave)break;
  spectrum_model_step();if(!spectrum_domain_alive()||temporal_retained())break;
  bool work=ss_text_session||cfa_learning_kind||signature_goal||signature_pending||temporal_exit_blocked();
  if(!ss_background_deferred()&&!ss_background_step(ss_background_allowed()))break;
  uint32_t flags=ss_flags;
  if(ss_canvas_session&&ss_receipt)flags|=RISC_SCENE_PRESENTING;
  if(ss_canvas_session&&(ss_input.down||ss_input.blocked))flags|=RISC_SCENE_INPUT_BUSY;
  bool resident_work=work||ss_capture_active()||spectrum_scene_background_capture_active();
  bool capture_busy=ss_capture_active()||spectrum_scene_background_capture_active();
  if(flags&RISC_SCENE_ACTIVITY)ss_resident.activity=true;
  int rr=(ss_text_session||capture_busy)?RISC_RESIDENT_BUSY:risc_scene_resident_poll_work_v1(&ss_resident,flags,resident_work?RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY:0);
  if(!spectrum_domain_alive())break;
  if(rr==RISC_RESIDENT_RETAINED){spectrum_domain_retain();break;}
  if(ss_resident.policy&&!resident_work&&!ss_text_session)ss_handoff(RISC_RESIDENT_CHECKPOINT_POLICY);
  if(runtime->struct_size>=RISC_RUNTIME_EXIT_REQUESTED_V1_SIZE&&runtime->exit_requested&&runtime->exit_requested()){
   if(!spectrum_domain_alive())break;
   if(ss_text_session){ss_text_state=(risc_text_entry_state_v1){.struct_size=sizeof(ss_text_state),.state=RISC_TEXT_ENTRY_CANCELLED,.flags=RISC_TEXT_ENTRY_HOME_CANCEL};ss_text_closing=true;}
   else if(ss_pause()&&spectrum_model_exit_ready())ss_leave=true;
  }
  if(spectrum_domain_alive()&&!ss_leave)runtime->yield_ms(running?1:8);
 }
 if(!spectrum_domain_alive())return 0;
 if(temporal_retained()||uncertain){spectrum_domain_retain();return 0;}
 if(!ss_pause()||!ss_background_close())return 0;
 if(ss_text_session){spectrum_domain_retain();return 0;}
 if(!spectrum_release(&grant)||!spectrum_release(&store_grant)||!spectrum_release(&ss_canvas_grant)||!spectrum_release(&ss_scene_grant))return 0;
 return 0;
}
