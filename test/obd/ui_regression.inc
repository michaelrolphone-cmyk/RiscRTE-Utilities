/* Regression input is deliberately delivered while a frame is in flight.
 * The ordinary suite waits for presentation, which cannot expose this fault. */
int telemetry_test_logical_target(unsigned,int,int*,int*);
int telemetry_test_scroll(void);
static uint32_t ui_began,ui_frames,ui_writes;
static bool ui_settled(void){
 if(!app.session||presenting||held||event_at<event_count)return false;
 risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t flags=0;
 assert(api->snapshot(NULL,app.session,&n,&flags)==RISC_SCENE_OK);
 return !(flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY));
}
static int ui_value(unsigned action_id){
 for(unsigned i=0;i<app.document.node_count;i++)if(app.document.nodes[i].action==action_id)return app.document.nodes[i].value;
 assert(0);return -1;
}
static void ui_tap(unsigned action_id,int value){
 int x,y;assert(telemetry_test_logical_target(action_id,value,&x,&y)==1);
 touch_tap((unsigned)x,(unsigned)y);
}
static bool ui_regression(void){
 if(step<3){
  if(!ui_settled())return true;
  if(step==0&&app.screen==INTRO)click(ACT_SCAN,0);
  else if(step==1&&app.screen==PEERS)click(ACT_PEER,0);
  else if(step==2&&app.screen==SELECTED)click(ACT_CONNECT,0);
  return true;
 }
 if(step==3){
  if(app.screen!=DASHBOARD||!ui_settled())return true;
  assert(app.elm.state==OBD_LIVE);click(ACT_CUSTOM,0);return true;
 }
 if(step==4){
  if(app.screen!=CUSTOM||!ui_settled())return true;
  ui_writes=sf_replacements;
  if(scenario==25){
   assert(on_dashboard(&app,OBD_COOLANT)&&on_dashboard(&app,OBD_LOAD));
   ui_tap(ACT_ON_DASH+OBD_COOLANT,0);step=5;
  }else if(scenario==26){
   ui_began=(uint32_t)ms;ui_frames=submits;swipe(1);step=5;
  }else assert(0);
  return true;
 }
 if(scenario==25){
  if(step==5){
   if(on_dashboard(&app,OBD_COOLANT))return true;
   if(!(held||presenting))return true;
   /* Second, different row while the first checkbox is rendering. */
   ui_began=(uint32_t)ms;ui_tap(ACT_ON_DASH+OBD_LOAD,0);step=6;return true;
  }
  if(step==6){
   if(on_dashboard(&app,OBD_LOAD))return true;
   printf("checkbox semantic acknowledgement: elapsed=%u ms, frame-pending=%u, model=%u document=%d\n",
      (unsigned)((uint32_t)ms-ui_began),(unsigned)(held||presenting),(unsigned)on_dashboard(&app,OBD_LOAD),ui_value(ACT_ON_DASH+OBD_LOAD));fflush(stdout);
   assert(ui_value(ACT_ON_DASH+OBD_LOAD)==0);
   assert((uint32_t)ms-ui_began<=40);
   assert(sf_replacements==ui_writes);step=7;return true;
  }
  if(step==7&&ui_settled()){
   assert(sf_replacements==ui_writes); /* no synchronous per-checkbox save */
   click(ACT_BACK,0);step=8;return true;
  }
  if(step==8&&app.screen==DASHBOARD&&ui_settled()&&sf_replacements>ui_writes){
   assert(!on_dashboard(&app,OBD_COOLANT)&&!on_dashboard(&app,OBD_LOAD));app.done=true;
   puts("Two single taps during rendering acknowledged; one coalesced save on leaving selector PASS");
  }
 }else if(step==5&&telemetry_test_scroll()>=400&&submits>ui_frames&&ui_settled()){
  uint32_t elapsed=(uint32_t)ms-ui_began;
  printf("sensor-list swipe -> completed frame: %u ms, scroll=%d, BLE polls=%u\n",(unsigned)elapsed,telemetry_test_scroll(),radio_polls);fflush(stdout);
  assert(elapsed<=500);app.done=true;
  puts("Actual-app/native-scene scrolling under live BLE with bounded raster pumping PASS");
 }
 return true;
}
