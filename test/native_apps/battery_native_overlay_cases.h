/* Full-feature actual Battery controller, adapter and Watch bridges. Only
 * capability providers and deterministic physical reports are modeled. */
#include "PortableQuickLayout.h"
#include "PortableAlarmLayout.h"
pqa_layout diag_quick_layout(void);
unsigned diag_quick_state(unsigned);
static unsigned overlay_start,overlay_end,overlay_ack_count,overlay_ack_exact;
static unsigned overlay_pending_edges,overlay_pending_action_edges,overlay_quick_seen,overlay_alarm_seen;
static bool overlay_active,overlay_done,overlay_stale;
static alarm_token_v1 overlay_token={ALARM_KIND_COUNTDOWN,4,123456,77};
static bool overlay_raw(void *c,size_t n){
 assert(ticks<overlay_end+3000 || !overlay_end);
 if(diag_quick_state(0))overlay_quick_seen++;
 if(diag_quick_state(9))overlay_alarm_seen++;
 uint64_t before=fake_touch_state.sequence;
 bool ok=fake_touch_poll(c,n);
 if(pending_token&&fake_touch_state.sequence!=before)overlay_pending_edges++;
 if(pending_token&&fake_touch_state.sequence!=before&&ticks>=overlay_start+500&&ticks<overlay_start+900)overlay_pending_action_edges++;
 return ok;
}
static int32_t overlay_get(void *c,const char *k,void *b,uint32_t cap,uint32_t *size){
 int rc=uat_read(c,k,b,cap,size);
 return rc==RISC_ATOMIC_BUFFER_SMALL?RISC_KEY_VALUE_BUFFER_SMALL:rc;
}
static int32_t overlay_status(void *c,alarm_status_v1 *s){
 (void)c;
 if((mode==26||mode==29||mode==30||mode==31||mode==32)&&overlay_start&&ticks>=overlay_start+(mode==31?1200u:mode==32?800u:600u)&&!overlay_done)overlay_active=true;
 *s=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*s),.state=overlay_active?ALARM_STATE_ALERT:ALARM_STATE_READY,.mode=ALARM_MODE_BOTH,.snapshot=2};
 if(overlay_active){s->occurrence=overlay_token;memcpy(s->label,"COUNTDOWN FINISHED",19);}
 return ALARM_OK;
}
static int32_t overlay_ack(void *c,const alarm_token_v1 *t){
 (void)c;overlay_ack_count++;
 assert(overlay_active&&!memcmp(t,&overlay_token,sizeof(*t)));
 if(mode==29&&!overlay_stale){overlay_stale=true;overlay_token.generation++;return ALARM_STALE;}
 overlay_ack_exact++;overlay_active=false;overlay_done=true;return ALARM_OK;
}
static void overlay_point(unsigned after,int x,int y){assert(action_count<256);actions[action_count++]=(typeof(actions[0])){(overlay_start+after)/25u,x,y};}
static void overlay_tap(unsigned after,pqa_rect r){overlay_point(after,r.x+r.w/2,r.y+r.h/2);}
static void overlay_record(const char *key,unsigned value){int i=uat_find(key);assert(i>=0&&uat.records[i].size==1&&uat.records[i].value[0]==value);}
static void overlay_case(void){
 /* Initialize and visibly complete the real app before opening either modal. */
 manual_start();assert(diag_drain()&&diag_visible());neutral();
 present_delay=mode==27?45:3;
 assert(diag_quick_state(2)==test_width&&diag_quick_state(3)==test_height);
 uint64_t app_hash=frame_hash();pqa_layout l=diag_quick_layout();
 overlay_start=(ticks/25u+4)*25u;overlay_end=overlay_start+6500;
 overlay_point(0,test_width/2,0);overlay_point(25,test_width/2,test_height-1);
 if(mode==26||mode==29||mode==30||mode==31||mode==32){
  if(mode==31)overlay_tap(600,l.tiles[5]); /* Torch must restore on alarm interruption. */
  if(mode==32)for(unsigned at=600;at<1000;at+=25)overlay_point(at,l.slider_right,l.slider_y[0]);
  portable_alarm_layout a;assert(portable_alarm_layout_for(test_width,test_height,false,&a));
  if(mode==30){ /* Outside right edge must not ACK; a later interior tap must. */
   overlay_point(900,a.dismiss.x+a.dismiss.w,a.dismiss.y+a.dismiss.h/2);
   overlay_point(1300,a.dismiss.x+a.dismiss.w-1,a.dismiss.y+a.dismiss.h-1);
  }else{
   overlay_point(mode>=31?1600:900,a.dismiss.x+a.dismiss.w/2,a.dismiss.y+a.dismiss.h/2);
   if(mode==29)overlay_point(1400,a.dismiss.x+a.dismiss.w/2,a.dismiss.y+a.dismiss.h/2);
  }
 }else{
  if(mode==28)uat.fail_on_submit=1;
  overlay_point(600,l.slider_right,l.slider_y[0]);
  if(mode==25){
   overlay_point(1000,l.slider_left,l.slider_y[1]);
   overlay_tap(1400,l.tiles[1]); /* DND */
   overlay_tap(1800,l.tiles[3]); /* Wi-Fi */
   overlay_tap(2200,l.tiles[4]); /* Bluetooth */
   overlay_tap(2600,l.tiles[2]); /* Airplane */
  }
  overlay_tap(3300,l.handle);
  if(mode==25){ /* Reopen from persisted provider state without a repair write. */
   overlay_point(4200,test_width/2,0);overlay_point(4225,test_width/2,test_height-1);
   overlay_tap(5500,l.handle);
  }
 }
 for(unsigned i=0;ticks<overlay_end&&i<20000;i++){t5_app_input_t in=one();assert(!in.tapped&&!in.buttons);ticks+=4;}
 assert(diag_drain()&&!diag_failed()&&!retained);
 assert(overlay_quick_seen&&!diag_quick_state(0)&&!diag_quick_state(1)&&!diag_quick_state(7)&&!diag_quick_state(8)&&!diag_quick_state(9)&&!diag_quick_state(10));
 assert(frame_hash()==app_hash); /* Exact completed Battery underlay restored. */
 if(mode==26||mode==29||mode==30||mode==31||mode==32){assert(overlay_alarm_seen&&overlay_done&&overlay_ack_exact==1&&overlay_ack_count==(mode==29?2u:1u));if(mode==31)assert(overlay_brightness==PQA_BRIGHTNESS_DEFAULT&&!uat.commits);if(mode==32)assert(!uat.commits&&uat_find(PQA_BRIGHTNESS_KEY)<0);}
 else if(mode==28){assert(uat.submits==1&&!uat.commits&&uat_find(PQA_BRIGHTNESS_KEY)<0);}
 else{overlay_record(PQA_BRIGHTNESS_KEY,100);assert(overlay_brightness==100);if(mode==25){overlay_record(PQA_VOLUME_KEY,0);overlay_record(PQA_DND_KEY,1);assert(uat.commits==6&&diag_quick_state(4)==100&&diag_quick_state(5)==0);}if(mode==27)assert(overlay_pending_action_edges);}
 assert(!uat.token&&!pending_token&&!frames);
 printf("OVERLAYS case=%u native=%ux%u quick=%u alarm=%u pending_edges=%u action_edges=%u commits=%u ack=%u exact=%u restoration=exact PASS\n",mode,test_width,test_height,overlay_quick_seen,overlay_alarm_seen,overlay_pending_edges,overlay_pending_action_edges,uat.commits,overlay_ack_count,overlay_ack_exact);
}
