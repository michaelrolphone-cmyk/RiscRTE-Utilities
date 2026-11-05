/* Production subpage presenter + unchanged controller/model. Recording native
 * primitives make target equivalence and side-effect boundaries observable. */
#define PORTABLE_CONTEXTS_LIVE() true
#define PORTABLE_CONTEXTS_CHECK_LIVE() do{}while(0)
#define portable_contexts_callback(value) (value)
#define main original_spectrum_fixture_main
#include "audio_spectrum_test.c"
#undef main
#include "../../Apps/spectrum_native_layout.h"
static spectrum_native_layout native_layout;
typedef struct {spectrum_native_rect r;int x,y;} mapped_target;
static mapped_target mapped[64],painted_map[64];
static unsigned mapped_count,clock_calls,paint_calls,text_count;
static bool mapping_only;
static char rendered_text[128][100];
static void bound(spectrum_native_rect r){assert(r.x>=0&&r.y>=0&&r.w>=0&&r.h>=0);assert((int64_t)r.x+r.w<=native_layout.width&&(int64_t)r.y+r.h<=native_layout.height);}
static spectrum_native_rect nrow(spectrum_native_rect r,unsigned i,unsigned n){spectrum_native_rect q={r.y,r.x,r.h,r.w};q=spectrum_native_column(q,i,n,native_layout.gap);return(spectrum_native_rect){q.y,q.x,q.h,q.w};}
static void native_target(spectrum_native_rect r,int x,int y){bound(r);if(!r.w||!r.h)return;assert(mapped_count<64);mapped[mapped_count++]=(mapped_target){r,x,y};}
static void ntext(bool paint,spectrum_native_rect r,unsigned face,const char*s,uint32_t color){(void)face;(void)color;if(!paint)return;bound(r);assert(text_count<128);snprintf(rendered_text[text_count++],100,"%s",s?s:"");paint_calls++;}
static void npill(bool paint,spectrum_native_rect r,const char*s,bool selected,int x,int y){(void)selected;native_target(r,x,y);ntext(paint,r,1,s,0);}
static void test_round(int x,int y,int w,int h,int radius,uint32_t color){(void)radius;(void)color;assert(!mapping_only);bound((spectrum_native_rect){x,y,w,h});paint_calls++;}
#define fill(x,y,w,h,color) test_round(x,y,w,h,0,color)
#define round_rect test_round
#include "../../Apps/spectrum_native_subpages.inc"
#undef round_rect
#undef fill
static uint32_t test_clock(void){assert(!mapping_only);clock_calls++;return now;}
static uint64_t hash_bytes(uint64_t h,const void*p,size_t n){const uint8_t*b=p;for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
static uint64_t model_hash(void){
 uint64_t h=UINT64_C(1469598103934665603);
 #define HASH(v) h=hash_bytes(h,&(v),sizeof(v))
 HASH(page);HASH(running);HASH(dirty);HASH(prefs);HASH(editing);HASH(signature_capture);HASH(signatures);HASH(signature_scroll);HASH(signature_goal);HASH(signature_slot);HASH(signature_pending);HASH(signature_errors);HASH(signature_delete_confirm);HASH(signature_mode);HASH(manual_room);HASH(room_tracker);HASH(event_library);HASH(event_files);HASH(event_training);HASH(event_match);HASH(event_neural);HASH(event_armed);HASH(event_capture_kind);HASH(event_list_scroll);HASH(event_example_scroll);HASH(event_edit_slot);HASH(event_delete_slot);HASH(event_delete_example);HASH(event_edit_shift);HASH(event_message);HASH(cfa_bank);HASH(cfa_learning);HASH(cfa_learning_at);HASH(cfa_learning_kind);HASH(cfa_learning_samples);HASH(cfa_learning_saved);HASH(shared_kind);HASH(shared_offset);HASH(shared_focus);HASH(shared_targets);HASH(shared_bounds);HASH(shared_name);HASH(shared_selected);HASH(shared_discard_confirm);HASH(toast_message);HASH(toast_until);
 #undef HASH
 return h;
}
static bool overlaps(spectrum_native_rect a,spectrum_native_rect b){return a.x<b.x+b.w&&b.x<a.x+a.w&&a.y<b.y+b.h&&b.y<a.y+a.h;}
static unsigned verify_layout(void){
 uint64_t before=model_hash();unsigned clocks=clock_calls,providers=acquires+reads+opens+closes+releases+puts_count+store_acquires+store_releases;
 mapped_count=text_count=paint_calls=0;mapping_only=true;assert(spectrum_native_subpage(false));mapping_only=false;
 assert(!paint_calls&&!text_count&&clock_calls==clocks&&model_hash()==before);
 assert(providers==acquires+reads+opens+closes+releases+puts_count+store_acquires+store_releases);
 unsigned n=mapped_count;memcpy(painted_map,mapped,n*sizeof(*mapped));
 for(unsigned i=0;i<n;i++)for(unsigned j=i+1;j<n;j++)assert(!overlaps(mapped[i].r,mapped[j].r));
 mapped_count=0;assert(spectrum_native_subpage(true));assert(model_hash()==before&&mapped_count==n);
 for(unsigned i=0;i<n;i++)assert(!memcmp(&mapped[i],&painted_map[i],sizeof(mapped[i])));
 return n;
}
static bool has_action(int x,int y){for(unsigned i=0;i<mapped_count;i++)if(mapped[i].x==x&&mapped[i].y==y)return true;return false;}
static bool has_text(const char*s){for(unsigned i=0;i<text_count;i++)if(strstr(rendered_text[i],s))return true;return false;}
static void invoke(int x,int y){assert(has_action(x,y));bool t=false;if(page==PAGE_SIGNATURES||page==PAGE_SIGNATURE_EDIT)signature_tap(x,y,&t);else if(page>=PAGE_SHARED)shared_tap(x,y);else event_tap(x,y,&t);}
static void fresh(void){
 reset();app=&api;runtime=&rt;api.millis=test_clock;storage=NULL;microphone=NULL;running=owned=uncertain=dirty=false;toast_message=NULL;undo_slot=-1;
 memset(signatures,0,sizeof(signatures));memset(&signature_capture,0,sizeof(signature_capture));memset(&event_library,0,sizeof(event_library));memset(&event_training,0,sizeof(event_training));memset(&event_match,0,sizeof(event_match));memset(&event_neural,0,sizeof(event_neural));memset(&cfa_bank,0,sizeof(cfa_bank));memset(&cfa_learning,0,sizeof(cfa_learning));memset(&room_tracker,0,sizeof(room_tracker));memset(&ambient,0,sizeof(ambient));
 signature_scroll=signature_goal=signature_mode=signature_pending=signature_errors=0;signature_slot=signature_capture_slot=manual_room=-1;signature_delete_confirm=false;event_slot=-1;
 st_files_init(&event_files,NULL);event_files.ready=3;event_list_scroll=event_example_scroll=0;event_armed=false;event_edit_slot=0;event_delete_slot=event_delete_example=-1;event_edit_shift=1;event_capture_kind=ST_POSITIVE;event_match.selected=-1;event_missed=0;event_initialized=true;
 shared_kind=CF_ROOM;shared_offset=0;shared_selected=-1;shared_focus=7;shared_targets=15;memset(shared_bounds,0x5a,sizeof(shared_bounds));shared_name[0]=0;shared_discard_confirm=false;cfa_learning_saved=false;cfa_learning_kind=0;cfa_learning_samples=0;cfa_changed=cfa_failed=false;cfa_loaded=true;
 clock_calls=0;now=40000;sample_rate=16000;editing=(spectrum_label){0};
}
static void matrix(int w,int h){
 fresh();assert(spectrum_native_layout_make(&native_layout,w,h));
 page=PAGE_SIGNATURES;assert(verify_layout()==9);assert(!has_action(24,208)&&has_action(215,208));
 signatures[0].kind=1;signatures[0].frames=123;strcpy(signatures[0].name,"Long room sample");signatures[1].kind=2;strcpy(signatures[1].name,"Event sample");assert(verify_layout()==13);assert(has_action(200,120)&&has_action(200,162));
 signature_scroll=6;verify_layout();assert(has_action(24,208)&&!has_action(215,208));
 signature_goal=100;signature_capture.frames=42;verify_layout();assert(has_action(63,127)&&has_action(177,127)&&!has_action(200,120)&&!has_action(77,208));assert(has_text("PAUSED ROOM 42 / 100"));running=true;verify_layout();assert(has_text("SAMPLING ROOM 42 / 100"));signature_goal=0;running=false;
 page=PAGE_SIGNATURE_EDIT;signature_slot=0;editing.color=1;verify_layout();assert(has_action(64,210)&&!has_action(176,210));signature_delete_confirm=true;verify_layout();assert(has_text("CONFIRM"));signature_pending=1;verify_layout();assert(has_action(176,210));signatures[0].kind=0;verify_layout();assert(has_text("RETRY SAVE"));
 page=PAGE_EVENTS;verify_layout();assert(!has_action(205,80)&&!has_action(29,203)&&has_action(210,203));event_library.labels[0].present=true;strcpy(event_library.labels[0].name,"Door bell");event_library.labels[1].present=true;verify_layout();assert(has_action(80,80)&&has_action(205,129));event_delete_slot=0;event_delete_example=-2;verify_layout();assert(has_text("SURE"));
 event_files.pending=0;event_files.errors[0]=RISC_APP_DATA_COMMIT_UNKNOWN;verify_layout();assert(has_action(65,203)&&has_action(175,203)&&!has_action(120,203)&&has_text("SAVE UNKNOWN"));event_delete_example=-3;verify_layout();assert(has_text("CONFIRM"));event_files.pending=-1;cfa_changed=true;verify_layout();assert(has_text("FINGERPRINT UNSAVED"));cfa_changed=false;
 event_list_scroll=6;verify_layout();assert(has_action(29,203)&&!has_action(210,203));event_list_scroll=0;
 page=PAGE_EVENT_LABEL;verify_layout();assert(has_action(80,54)&&has_action(200,54)&&has_action(200,88)&&has_action(64,121)&&has_action(176,121)&&has_action(90,161)&&has_action(213,161));event_edit_shift=0;verify_layout();assert(!has_action(90,161));event_edit_shift=2;verify_layout();assert(!has_action(213,161));
 page=PAGE_EVENT_CAPTURE;assert(verify_layout()==1);assert(has_action(120,201)&&has_text("CAPTURE STOPPED"));event_armed=true;assert(verify_layout()==2);assert(has_action(64,203)&&has_action(176,203));event_armed=false;event_training.ready=true;event_training.event.duration_ms=1024;event_training.event.flags=0;assert(verify_layout()==2);assert(has_action(64,193)&&!has_action(120,163));event_training.event.flags=ST_CLIPPED;assert(verify_layout()==3);assert(has_action(64,206)&&has_action(120,163)&&has_text("SAVE WINDOW"));event_training.event.flags|=ST_CONFIRMED_END;verify_layout();assert(has_text("UNDO WHOLE EVENT")&&has_text("WHOLE EVENT / END CONFIRMED"));event_training.ready=false;
 page=PAGE_EVENT_EXAMPLES;verify_layout();assert(!has_action(206,79));event_library.labels[0].examples[0]=(st_example){.id=1,.kind=ST_POSITIVE,.flags=ST_CLIPPED,.peak_hz=440,.duration_ms=640};verify_layout();assert(has_action(206,79)&&has_text("CLIP /"));event_delete_slot=0;event_delete_example=0;verify_layout();assert(has_text("SURE"));event_example_scroll=4;verify_layout();assert(has_action(32,205)&&!has_action(208,205));event_example_scroll=0;
 page=PAGE_SHARED;verify_layout();assert(has_action(197,20)&&has_action(45,60)&&!has_action(120,106));for(unsigned i=0;i<5;i++){cfa_bank.profiles[i].kind=CF_ROOM;snprintf(cfa_bank.profiles[i].name,17,"Room %u",i+1);}verify_layout();assert(has_action(120,106)&&has_action(120,150));shared_offset=99;verify_layout();assert(!has_action(120,106)&&has_action(201,212)&&has_text("LIST CHANGED"));assert(shared_offset==99);shared_offset=0;
 shared_kind=CF_EVENT;for(unsigned i=0;i<5;i++)cfa_bank.profiles[i].kind=CF_EVENT;verify_layout();assert(!has_action(45,60)&&has_action(120,64)&&has_action(120,108)&&has_action(120,152));
 page=PAGE_SHARED_EDIT;verify_layout();assert(has_action(120,84)&&has_action(120,135)&&!has_action(120,177));shared_kind=CF_ROOM;shared_selected=0;verify_layout();assert(has_action(120,177));
 page=PAGE_SHARED_CAPTURE;cfa_learning_kind=CF_ROOM;cfa_learning_samples=3;cfa_learning_at=0;verify_layout();assert(has_action(120,163)&&has_action(120,214)&&has_text("SAVE TO SHARED LIBRARY"));now=1000;verify_layout();assert(has_text("RESUME"));running=true;verify_layout();assert(has_text("CAPTURING..."));cfa_learning_saved=true;verify_layout();assert(has_text("RETRY SAVE")&&has_text("DISCARD"));shared_discard_confirm=true;verify_layout();assert(has_text("CONFIRM"));
 page=PAGE_MAIN;mapped_count=0;assert(!spectrum_native_subpage(false)&&!mapped_count);
}
static void controller_regressions(void){
 fresh();assert(spectrum_native_layout_make(&native_layout,410,502));
 page=PAGE_SIGNATURES;signatures[0].kind=1;strcpy(signatures[0].name,"Office");verify_layout();invoke(70,120);assert(page==PAGE_SIGNATURE_EDIT&&signature_slot==0&&!strcmp(editing.name,"Office"));verify_layout();invoke(64,210);assert(signature_delete_confirm);verify_layout();assert(has_text("CONFIRM"));invoke(176,167);assert(page==PAGE_SIGNATURES);
 page=PAGE_EVENTS;event_library.labels[0].present=true;strcpy(event_library.labels[0].name,"Knock");verify_layout();invoke(205,80);assert(event_delete_slot==0&&event_delete_example==-2);verify_layout();invoke(80,80);assert(page==PAGE_EVENT_LABEL&&event_edit_slot==0);verify_layout();invoke(213,161);assert(event_edit_shift==1);verify_layout();invoke(90,161);assert(event_edit_shift==0);verify_layout();assert(!has_action(90,161));invoke(64,219);assert(page==PAGE_EVENT_EXAMPLES);
 event_library.labels[0].examples[0]=(st_example){.id=1,.kind=ST_POSITIVE};verify_layout();invoke(206,79);assert(event_delete_example==0&&event_delete_slot==0);verify_layout();assert(has_text("SURE"));
 page=PAGE_EVENT_CAPTURE;event_training.ready=true;event_training.event.flags=ST_CLIPPED;verify_layout();invoke(120,163);assert(event_training.event.flags&ST_CONFIRMED_END);verify_layout();invoke(176,206);assert(page==PAGE_EVENT_LABEL&&!event_training.ready);
 page=PAGE_SHARED;shared_offset=99;cfa_bank.profiles[0].kind=CF_ROOM;strcpy(cfa_bank.profiles[0].name,"Kitchen");verify_layout();invoke(201,212);assert(!shared_offset);verify_layout();invoke(120,106);assert(page==PAGE_SHARED_EDIT&&shared_selected==0&&!strcmp(shared_name,"Kitchen"));verify_layout();invoke(120,217);assert(page==PAGE_SHARED);
 page=PAGE_SHARED_CAPTURE;cfa_learning_saved=true;shared_discard_confirm=false;verify_layout();invoke(120,214);assert(shared_discard_confirm&&page==PAGE_SHARED_CAPTURE);verify_layout();assert(has_text("CONFIRM"));
}
int main(void){
 const int sizes[][2]={{160,160},{240,240},{410,502},{502,410},{480,800},{800,480},{640,240},{240,640}};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++)matrix(sizes[i][0],sizes[i][1]);
 controller_regressions();
 puts("Spectrum native subpages: nine pages across eight displays; pure current-model target construction, paint/input target equality, bounded nonoverlap, branch/action/status coverage and unchanged-controller confirmation flows passed");return 0;
}
