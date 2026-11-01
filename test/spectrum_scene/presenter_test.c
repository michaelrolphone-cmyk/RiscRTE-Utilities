#include <assert.h>
#include <stdlib.h>
#include "spectrum_scene_model.inc"
#include "spectrum_scene_shared.inc"
#include "spectrum_scene_presenter.inc"
#include "spectrum_scene_canvas.h"
static bool live=true;static unsigned writes,keys,now;static risc_runtime_api_v1 rt;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&rt:NULL;}
static bool spectrum_domain_alive(void){return live;}
static bool spectrum_domain_storage_boundary(void){return live;}
static void spectrum_domain_retain(void){live=false;}
static void keyboard_begin(void){keys++;}
static bool portable_fingerprint_clock(const risc_runtime_api_v1*r,uint64_t*out){(void)r;*out=0;return true;}
static uint32_t millis(void){return now;}
static bool acquire(const char*c,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){(void)c;(void)v;(void)i;(void)g;return false;}
static bool release(risc_runtime_capability_v1*g){memset(g,0,sizeof(*g));g->struct_size=sizeof(*g);return true;}
static int32_t get(void*c,const char*k,void*b,uint32_t n,uint32_t*z){(void)c;(void)k;(void)b;(void)n;*z=0;return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;(void)k;(void)b;(void)n;writes++;return RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static risc_components_document_v1 doc;static uint32_t revision;
static void reset(void){live=true;writes=keys=now=0;rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};runtime=&rt;static const struct spectrum_domain_clock clock={millis};app=&clock;storage=&kv;store_acquired=true;page=PAGE_MAIN;view=0;prefs=spectrum_preferences_default();sample_rate=16000;configure_dsp();build_palette();message=store_message=toast_message=NULL;pending_save=load_errors=0;signature_pending=signature_errors=signature_goal=0;signature_slot=0;event_edit_slot=0;event_delete_slot=event_delete_example=-1;event_edit_shift=0;event_files.pending=-1;event_files.ready=3;shared_selected=-1;shared_kind=CF_ROOM;cfa_learning_kind=0;cfa_learning_saved=false;cfa_failed=false;cfa_loaded=true;event_armed=false;spectrum_presenter_exit_requested=false;running=frozen=lab_edit=false;cursor_visible=false;undo_slot=-1;memset(labels,0,sizeof(labels));memset(signatures,0,sizeof(signatures));memset(&event_library,0,sizeof(event_library));memset(&event_training,0,sizeof(event_training));memset(&cfa_bank,0,sizeof(cfa_bank));cfa_room.slot=cfa_event.slot=-1;}
static void build(void){spectrum_presenter_build(&doc);doc.revision=++revision;assert(doc.node_count<=96&&doc.route_count==1&&doc.root==1);unsigned headers=0;for(unsigned i=0;i<doc.node_count;i++){assert(doc.nodes[i].id==i+1&&doc.nodes[i].route==1);headers+=doc.nodes[i].kind==RISC_COMPONENT_HEADER_ACTION;}assert(headers<=2);spectrum_presenter_commit(&doc);}
static bool has(unsigned action){for(unsigned i=0;i<doc.node_count;i++)if(doc.nodes[i].action==action)return true;return false;}
static bool act(unsigned action){risc_scene_event_v1 e={.struct_size=sizeof(e),.document_revision=doc.revision,.kind=RISC_SCENE_ACTION_EVENT,.action=action,.sequence=1};for(unsigned i=0;i<doc.node_count;i++)if(doc.nodes[i].action==action)e.node=doc.nodes[i].id;return spectrum_presenter_action(&e);}
int main(void){unsigned tests=0;reset();for(unsigned p=0;p<=PAGE_SHARED_CAPTURE;p++){page=p;build();assert(has(SP_BACK)&&has(SP_TOGGLE));}assert(!writes);tests++;
 reset();page=PAGE_CONTROLS;build();for(unsigned i=1;i<=13;i++)assert(has(SP_SETTING_BASE+i*3));assert(!writes);assert(act(SP_SETTING_BASE+3*3+1)&&!prefs.log_frequency&&writes==1);tests++;
 reset();page=PAGE_CONTROLS;build();load_errors=1;int gain=prefs.gain_db;assert(act(SP_SETTING_BASE+7*3+1)&&prefs.gain_db==gain&&!writes);tests++;
 reset();page=PAGE_LABEL;editing=(spectrum_label){.present=true,.frequency_hz=440};strcpy(editing.name,"  Tone  ");edit_slot=-1;build();assert(act(SP_COLOR_BASE+7)&&editing.color==7);assert(act(SP_LABEL_NAME)&&keys==1&&!writes);assert(act(SP_LABEL_SAVE)&&!strcmp(labels[0].name,"Tone")&&writes==1);tests++;
 reset();page=PAGE_MAIN;cursor_visible=true;cursor_hz=440;build();assert(act(SP_LABEL_NEW)&&page==PAGE_LABEL&&editing.frequency_hz==440&&!writes);tests++;
 reset();page=PAGE_EVENT_LABEL;build();assert(act(SP_EVENT_SHIFT_UP)&&event_edit_shift==1);assert(act(SP_EVENT_EXAMPLES)&&page==PAGE_EVENT_EXAMPLES);tests++;
 reset();page=PAGE_EVENT_CAPTURE;event_training.ready=true;event_training.event.flags=ST_CLIPPED;build();assert(has(SP_EVENT_END)&&has(SP_EVENT_SAVE));assert(act(SP_EVENT_END)&&(event_training.event.flags&ST_CONFIRMED_END));tests++;
 reset();page=PAGE_SHARED;build();assert(act(SP_SHARED_NEW)&&page==PAGE_SHARED_EDIT);build();assert(act(SP_SHARED_TRAIN)&&page==PAGE_SHARED_EDIT&&!writes);tests++;
 reset();page=PAGE_SHARED_CAPTURE;cfa_learning_saved=true;build();assert(act(SP_SHARED_CANCEL)&&shared_discard_confirm&&page==PAGE_SHARED_CAPTURE&&!writes);tests++;
 reset();page=PAGE_MAIN;build();page=PAGE_LABEL;assert(!act(SP_CONTROLS));tests++;
 reset();page=PAGE_MAIN;build();live=false;assert(!act(SP_CONTROLS));tests++;
 reset();page=PAGE_CONTROLS;build();for(unsigned i=1;i<=10;i++){assert(has(SP_SETTING_BASE+i*3)&&has(SP_SETTING_BASE+i*3+1));assert(act(SP_SETTING_BASE+i*3));assert(spectrum_preferences_valid(&prefs));build();assert(act(SP_SETTING_BASE+i*3+1));assert(spectrum_preferences_valid(&prefs));build();}tests++;
 reset();page=PAGE_CONTROLS;build();assert(act(SP_SETTING_BASE+12*3)&&page==PAGE_SHARED&&shared_kind==CF_ROOM);page=PAGE_CONTROLS;build();assert(act(SP_SETTING_BASE+13*3)&&shared_kind==CF_EVENT);tests++;
 reset();labels[0]=(spectrum_label){.present=true,.frequency_hz=440,.name="Tone"};open_label(0,0);build();assert(act(SP_LABEL_DELETE)&&!labels[0].present&&undo_slot==0);build();assert(act(SP_UNDO)&&labels[0].present&&!strcmp(labels[0].name,"Tone"));tests++;
 reset();page=PAGE_SIGNATURES;signatures[7].kind=SPECTRUM_SIGNATURE_ROOM;strcpy(signatures[7].name,"Last room");build();assert(has(SP_SIGNATURE_BASE+7)&&has(SP_SIGNATURE_USE_BASE+7));assert(act(SP_SIGNATURE_BASE+7)&&signature_slot==7&&page==PAGE_SIGNATURE_EDIT);tests++;
 reset();page=PAGE_EVENTS;event_library.labels[ST_LABELS-1].present=true;strcpy(event_library.labels[ST_LABELS-1].name,"Last sound");build();assert(has(SP_EVENT_BASE+ST_LABELS-1)&&has(SP_EVENT_DELETE_BASE+ST_LABELS-1));tests++;
 reset();page=PAGE_EVENT_EXAMPLES;event_library.labels[0].examples[ST_EXAMPLES-1].id=99;build();assert(has(SP_EXAMPLE_DELETE_BASE+ST_EXAMPLES-1));tests++;
 reset();page=PAGE_SHARED;cfa_bank.profiles[CF_PROFILES-1].kind=CF_ROOM;strcpy(cfa_bank.profiles[CF_PROFILES-1].name,"Last shared");build();assert(has(SP_SHARED_BASE+CF_PROFILES-1));assert(act(SP_SHARED_BASE+CF_PROFILES-1)&&shared_selected==CF_PROFILES-1&&page==PAGE_SHARED_EDIT);tests++;
 reset();page=PAGE_MAIN;build();risc_scene_event_v1 invalid={.struct_size=sizeof(invalid),.sequence=1,.document_revision=doc.revision-1,.kind=RISC_SCENE_ACTION_EVENT,.action=SP_BACK};assert(!spectrum_presenter_action(&invalid));invalid.document_revision=doc.revision;invalid.kind=RISC_SCENE_VALUE_EVENT;assert(!spectrum_presenter_action(&invalid));tests++;
 reset();page=PAGE_MAIN;running=true;build();assert(act(SP_FREEZE)&&frozen&&!running);tests++;
 reset();page=PAGE_EVENT_CAPTURE;event_armed=true;build();assert(act(SP_BACK)&&page==PAGE_EVENT_CAPTURE&&event_armed);tests++;
 reset();page=PAGE_EVENT_CAPTURE;event_training.ready=true;build();assert(act(SP_BACK)&&page==PAGE_EVENT_CAPTURE&&event_training.ready);tests++;
 reset();page=PAGE_EVENT_CAPTURE;build();assert(act(SP_BACK)&&page==PAGE_EVENT_LABEL);tests++;
 reset();page=PAGE_EVENT_CAPTURE;event_training.ready=true;build();assert(act(SP_EVENT_CANCEL)&&!event_training.ready);tests++;
 reset();page=PAGE_MAIN;lab_edit=true;build();assert(act(SP_BACK)&&page==PAGE_MAIN&&!lab_edit&&!spectrum_presenter_exit_requested);tests++;
 for(unsigned p=0;p<=PAGE_SHARED_CAPTURE;p++){reset();page=p;build();assert(act(SP_BACK));unsigned result_page=page;bool result_exit=spectrum_presenter_exit_requested,result_lab=lab_edit;unsigned result_writes=writes;reset();page=p;assert(spectrum_presenter_back());assert(page==result_page&&spectrum_presenter_exit_requested==result_exit&&lab_edit==result_lab&&writes==result_writes);}tests++;
 reset();page=PAGE_EVENT_CAPTURE;event_armed=true;assert(spectrum_presenter_back()&&event_armed&&page==PAGE_EVENT_CAPTURE);tests++;
 reset();page=PAGE_MAIN;lab_edit=true;assert(spectrum_presenter_back()&&!lab_edit&&!spectrum_presenter_exit_requested);tests++;
 reset();page=PAGE_LABEL;live=false;assert(!spectrum_presenter_back()&&page==PAGE_LABEL&&!writes);tests++;
 reset();page=PAGE_MAIN;running=true;signature_goal=64;assert(spectrum_presenter_back()&&!spectrum_presenter_exit_requested&&running&&signature_goal==64);tests++;
 reset();page=PAGE_MAIN;running=true;event_initialized=true;event_training.ready=true;assert(spectrum_presenter_back()&&!spectrum_presenter_exit_requested&&running&&event_training.ready);tests++;
 reset();page=PAGE_EVENTS;toast_message="old";assert(spectrum_presenter_back()&&page==PAGE_CONTROLS&&!toast_message);tests++;
 reset();page=PAGE_EVENT_LABEL;toast_message="old";assert(spectrum_presenter_back()&&page==PAGE_EVENTS&&!toast_message);tests++;
 printf("Spectrum presenter/domain %u cases PASS\n",tests);return 0;}
