/* Production Buttons app and FT6336U, with physical I2C/GPIO/time fixtures.
 * Reuse the existing app fixture; do not replace app, queue or gesture code. */
#define HID_TOUCHPAD 0
#define main original_hid_app_test_main
#include "hid_app_test.c"
#undef main
const risc_touch_api_v1 *hid_watch_touch_start(void);
void hid_watch_touch_stop(void);
void hid_renderer_watch_report(risc_touch_snapshot_v1*s){*s=sample;}
uint64_t hid_renderer_watch_millis(void){return tick;}
static const risc_touch_api_v1 *physical;
static void physical_start(void){
 reset();physical=hid_watch_touch_start();start_session(false);
 assert(unsub(NULL,subscription));raw=physical;raw_grant.api=physical;
 subscription=physical->subscribe(physical->context);assert(subscription);
 risc_touch_snapshot_v1 value={0};assert(physical->snapshot(physical->context,&value));sequence=value.sequence;
 tick=20;pump();assert(status.state==RISC_HID_READY&&assignment_ready(assignments)&&!button_gate);
 assignments[0]=(hid_assignment){.kind=HID_ACTION_KEY,.modifiers=5,.key=76};
}
static void point(unsigned at,unsigned count,unsigned id,unsigned x,unsigned y){
 tick=at;sample.contact_count=(uint8_t)count;
 sample.contacts[0]=(risc_touch_contact_v1){.id=(uint8_t)id,.x=(uint16_t)x,.y=(uint16_t)y};
 sample.contacts[1]=(risc_touch_contact_v1){.id=2,.x=170,.y=100};
 assert(physical->poll(physical->context,1));
}
static void finish(void){if(token)stop_session("done");assert(!grants&&!raw_subs&&!subscription&&!have_token);hid_watch_touch_stop();}
static bool rejected_poll(void*c,size_t n){assert(physical->poll(c,n));return false;}
static bool reject_neutral(void*c,uint64_t t,uint8_t m,const uint8_t*k){return hid_keyboard(c,t,m,k)&&(m||k[0]);}
#ifdef HID_REPORT_SEAM
/* The Python central owns the real HID connection; this bridge only supplies
 * its capability to the production app pump during a real touch transaction. */
void hid_buttons_watch_report_seam(const risc_bluetooth_hid_v1 *provider,uint64_t connection){
 physical_start();hid=provider;token=connection;status=(risc_bluetooth_hid_status_v1){.struct_size=sizeof(status)};
 tick=21;pump();assert(status.state==RISC_HID_READY&&assignment_ready(assignments)&&!button_gate);
 point(25,1,1,30,100);point(45,0,1,30,100);tick=60;pump();
 assert(!closing&&token==connection&&held_button==-1);
 hid=&fake_hid;token=7;finish();
}
#else
int main(int argc,char**argv){
 assert(argc==2);const char*name=argv[1];physical_start();bool accepted=true,mouse=false;
 if(!strcmp(name,"mouse")){assignments[0]=(hid_assignment){.kind=HID_ACTION_MOUSE,.buttons=2};mouse=true;}
 if(!strcmp(name,"wheel")){assignments[0]=(hid_assignment){.kind=HID_ACTION_WHEEL,.wheel=-3};mouse=true;}
 if(!strcmp(name,"motion")){assignments[0]=(hid_assignment){.kind=HID_ACTION_MOVE,.dx=-15,.dy=7};mouse=true;}
 if(!strcmp(name,"unarmed")){button_gate=true;accepted=false;}
 if(!strcmp(name,"settings")){view=HID_VIEW_SETTINGS;accepted=false;}
 point(!strcmp(name,"deadline100")||!strcmp(name,"deadline101")?20:25,1,1,30,100);
 if(!strcmp(name,"sampled")||!strcmp(name,"held-release")){pump();assert(key_reports==1);}
 if(!strcmp(name,"move-inside"))point(30,1,1,32,102);
 if(!strcmp(name,"move-outside")){point(30,1,1,120,100);point(35,1,1,30,100);accepted=false;}
 if(!strcmp(name,"multi-contact")){point(30,2,1,30,100);accepted=false;}
 if(!strcmp(name,"changed-id")){point(30,1,2,30,100);accepted=false;}
 if(!strcmp(name,"second-tap")){point(30,0,1,30,100);point(35,1,1,30,100);accepted=false;}
 if(!strcmp(name,"queue-overflow")){for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH+2u;i++)point(26+i,1,1,30,100);accepted=false;}
 unsigned end=!strcmp(name,"deadline100")?120:!strcmp(name,"deadline101")?121:!strcmp(name,"clock-rollback")?20:!strcmp(name,"queue-overflow")?90:45;
 point(end,0,1,30,100);tick=end>60?end:60;
 if(!strcmp(name,"deadline101")||!strcmp(name,"clock-rollback"))accepted=false;
 if(!strcmp(name,"keyboard-unready")){fake_status.flags&=~RISC_HID_KEYBOARD_READY;accepted=false;}
 if(!strcmp(name,"no-auth")){fake_status.flags&=~RISC_HID_AUTHENTICATED;accepted=false;}
 if(!strcmp(name,"no-encryption")){fake_status.flags&=~RISC_HID_ENCRYPTED;accepted=false;}
 if(!strcmp(name,"generation-change")){fake_status.connection_generation++;accepted=false;}
 if(!strcmp(name,"disconnected")){fake_status.state=RISC_HID_ADVERTISING;accepted=false;}
 risc_touch_api_v1 failed=*physical;
 if(!strcmp(name,"poll-fail")){failed.poll=rejected_poll;raw=&failed;accepted=false;}
 if(!strcmp(name,"press-fail"))report_bad=true;
 if(!strcmp(name,"release-fail"))fake_hid.keyboard=reject_neutral;
 pump();
 if(!strcmp(name,"press-fail")){assert(key_reports==1&&!token&&!grants);}
 else if(!strcmp(name,"release-fail")){assert(key_reports==2&&!token&&!grants);}
 else if(mouse){
  assert(mouse_reports==2&&key_reports==0);
  assert(reports[0].buttons==assignments[0].buttons&&reports[0].dx==assignments[0].dx&&reports[0].dy==assignments[0].dy&&reports[0].wheel==assignments[0].wheel);
  assert(!reports[1].buttons&&!reports[1].dx&&!reports[1].dy&&!reports[1].wheel);
 }else{
  assert(key_reports==(accepted?2u:0u));
  if(accepted)assert(reports[0].mod==5&&reports[0].key==76&&!reports[1].mod&&!reports[1].key);
 }
 printf("Watch FT6336U queued Buttons %s: keyboard=%u mouse=%u PASS\n",name,key_reports,mouse_reports);finish();return 0;
}
#endif
