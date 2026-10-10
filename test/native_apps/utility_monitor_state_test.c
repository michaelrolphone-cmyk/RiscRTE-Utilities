/* Real Spectrum loop and row handlers; only microphone/display/storage I/O is
 * simulated. Repeat the same trace with every frame withheld and immediately
 * ready, including deletion of the current last page and detector churn. */
#define main original_spectrum_test_main
#include "audio_spectrum_test.c"
#undef main
static bool frame_available;
static unsigned ready_checks,trace_at,trace[2][32],pass;
static bool frame_ready(void){ready_checks++;return frame_available;}
static void record(void){assert(trace_at<32);trace[pass][trace_at++]=page*10000+list_scroll*100+saved_count();}
static void seed(unsigned n){
 memset(labels,0,sizeof(labels));memset(label_active,0,sizeof(label_active));
 for(unsigned i=0;i<n;i++){labels[i].present=true;labels[i].frequency_hz=(uint16_t)(100+i*100);labels[i].color=(uint8_t)i;snprintf(labels[i].name,sizeof(labels[i].name),"Label %u",i);label_active[i]=true;label_snr[i]=600;}
 dirty=true;
}
static void begin_list(void){started=true;view=2;lab_edit=true;seed(8);}
static void last_page(void){assert(list_scroll==6&&saved_count()==8);record();}
static void first_delete(void){assert(!labels[6].present&&labels[7].present&&list_scroll==6&&undo_slot==6);record();}
static void second_delete(void){assert(!labels[7].present&&saved_count()==6&&list_scroll==4&&undo_slot==7);record();}
static void opened_row(void){assert(page==PAGE_LABEL&&edit_slot==4&&!strcmp(editing.name,"Label 4"));record();}
static void begin_monitor(void){page=PAGE_MAIN;lab_edit=false;list_scroll=0;toast_message=NULL;undo_slot=-1;seed(8);}
static void monitor_last(void){assert(list_scroll==5);record();seed(5);}
static void shrunk(void){assert(list_scroll==2);record();seed(0);}
static void emptied(void){assert(!list_scroll);record();seed(8);}
static void repopulated(void){assert(!list_scroll);record();}
static void expire_event(void){seed(3);list_scroll=3;signatures[0].kind=SPECTRUM_SIGNATURE_EVENT;event_slot=0;last_event_at=now-2001u;}
static void expired_event(void){assert(event_slot==-1&&!list_scroll);record();}
static void release_frame(void){if(pass){assert(!frames&&utility_frame_pending);}else assert(frames);frame_available=true;dirty=true;}
static void latest_frame(void){assert(!utility_frame_pending&&frames&&(pass==0||frames==1)&&!list_scroll);record();}
int main(void){
 for(pass=0;pass<2;pass++){
  reset();utility_frame_reset();frame_available=pass==0;ready_checks=trace_at=0;api.frame_ready=frame_ready;
  check(begin_list);for(unsigned i=0;i<6;i++)tap(210,214,0);check(last_page);
  tap(201,94,0);check(first_delete);tap(201,94,0);check(second_delete);tap(100,94,0);check(opened_row);
  check(begin_monitor);for(unsigned i=0;i<5;i++)tap(215,224,0);check(monitor_last);
  check(shrunk);check(emptied);check(repopulated);check(expire_event);events[count-1].input=(t5_app_input_t){.tapped=true,.touch_x=15,.touch_y=224};check(expired_event);check(release_frame);check(latest_frame);back();run();
  assert(!escaped&&!live&&!grant_live&&!store_live&&ready_checks>10&&puts_count==2&&trace_at==10);
 }
 assert(!memcmp(trace[0],trace[1],sizeof(trace[0])));
 puts("PASS Spectrum ready/withheld equivalence: label delete/open, shrink/empty/repopulate, latest-only redraw");return 0;
}
