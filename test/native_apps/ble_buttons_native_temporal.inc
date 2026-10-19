/* Stateful actual-adapter presentation controls; included after the real app. */
bool bb_diag_poll(unsigned,t5_app_input_t*),bb_diag_drain(void),bb_diag_visible(void),bb_diag_failed(void);
uint64_t bb_diag_generation(void);unsigned bb_diag_owner(void);
static int temporal_x,temporal_y;
static t5_app_input_t temporal_one(void){t5_app_input_t in={0};assert(bb_diag_poll(0,&in));assert(!bb_diag_failed());return in;}
static void temporal_neutral(void){for(unsigned i=0;i<4;i++)assert(!temporal_one().tapped);}
static void temporal_edge(bool down){ticks++;risc_touch_contact_v1 point={.id=1,.x=temporal_x,.y=temporal_y};touch_edge(down?RISC_TOUCH_EVENT_DOWN:RISC_TOUCH_EVENT_UP,1,temporal_x,temporal_y);prior_touch.contact_count=down;prior_touch.contacts[0]=point;prior_touch.sequence=touch_sequence;prior_touch.timestamp_ms=ticks;prior_touch.width=test_width;prior_touch.height=test_height;}
static void temporal_fresh(void){temporal_edge(true);assert(!temporal_one().tapped);temporal_edge(false);t5_app_input_t in=temporal_one();assert(in.tapped&&in.touch_x==temporal_x&&in.touch_y==temporal_y);temporal_neutral();}
static void temporal_submitted(void){for(unsigned i=0;!pending_token&&i<20000;i++)assert(!temporal_one().tapped);assert(pending_token&&bb_diag_owner()==5);}
static void temporal_completed_one(void){unsigned target=present_completions+1;for(unsigned i=0;present_completions<target&&i<20000;i++)assert(!temporal_one().tapped);assert(present_completions==target);}
static void temporal_start(void){
 manual_touch=true;stop_poll=1000000;present_delay=30;app=t5_app_get_api(1);runtime=risc_runtime_get_api(1);utility_frame_reset();portable_ble_buttons_native_begin();
 hid=0;raw=0;hid_acquired=raw_acquired=held_acquired=closing=had_ready=close_error=save_failed=pair_armed=pair_down=pair_sent=false;token=subscription=sequence=0;held_button=-1;button_gate=true;view=HID_VIEW_MAIN;selected=0;dirty=true;message="Temporal fixture";status=(risc_bluetooth_hid_status_v1){.struct_size=sizeof(status)};hid_gesture_reset(&gesture,true);load_assignments();reset_button_queue();
 hid_native_layout l=hid_native_geometry();temporal_x=l.footer[0].x+l.footer[0].w/2;temporal_y=l.footer[0].y+l.footer[0].h/2;
 draw();temporal_submitted();assert(!bb_diag_visible());
}
static void temporal_raw_step(void){(void)temporal_one();pump();}
static void temporal_raw_ready(void){
 assert(bb_diag_drain());temporal_neutral();start_session(false);pump();draw();assert(bb_diag_drain());pump();temporal_neutral();assert(token&&status.state==RISC_HID_READY&&!button_gate);
 hid_native_layout l=hid_native_geometry();temporal_x=l.buttons[0].x+l.buttons[0].w/2;temporal_y=l.buttons[0].y+l.buttons[0].h/2;
}
static void temporal_raw(unsigned mode){
 temporal_raw_ready();unsigned reports=hid_keyboard_reports;
 if(mode==14){draw();temporal_submitted();temporal_edge(true);temporal_raw_step();temporal_edge(false);temporal_raw_step();assert(hid_keyboard_reports==reports&&!held_key);assert(bb_diag_drain());pump();}
 else if(mode==15){temporal_edge(true);temporal_edge(false);temporal_edge(true);temporal_edge(false);temporal_raw_step();pump();assert(hid_keyboard_reports==reports+4&&!held_key);}
 else{
  temporal_edge(true);temporal_raw_step();assert(held_key&&held_button==0&&hid_keyboard_reports==reports+1);
  if(mode==11){draw();temporal_submitted();temporal_edge(false);temporal_raw_step();assert(!held_key&&held_button<0&&hid_keyboard_reports==reports+2);assert(bb_diag_drain());}
  else if(mode==12){touch_edge(RISC_TOUCH_EVENT_DOWN,2,temporal_x+1,temporal_y);prior_touch.contacts[1]=(risc_touch_contact_v1){.id=2,.x=temporal_x+1,.y=temporal_y};prior_touch.contact_count=2;prior_touch.sequence=touch_sequence;temporal_raw_step();assert(!held_key&&held_button<0);touch_edge(RISC_TOUCH_EVENT_UP,2,temporal_x+1,temporal_y);prior_touch.contact_count=1;prior_touch.sequence=touch_sequence;temporal_edge(false);temporal_raw_step();assert(!held_key);}
  else if(mode==13){view=HID_VIEW_SETTINGS;draw();temporal_submitted();pump();assert(!held_key&&held_button<0);temporal_edge(false);temporal_raw_step();assert(bb_diag_drain());}

  else assert(!"Unknown raw mode");
 }
 assert(portable_radio_suspend());assert(!hid_owned&&!held_key);assert(bb_diag_drain());
}
static int temporal_run(unsigned mode){
 temporal_start();
 if(mode>=11){temporal_raw(mode);}
 else if(mode<=1){temporal_edge(true);if(mode==0)temporal_edge(false);assert(bb_diag_drain()&&bb_diag_visible());temporal_neutral();if(mode==1){temporal_edge(false);assert(!temporal_one().tapped);temporal_neutral();}temporal_fresh();}
 else{
  assert(bb_diag_drain()&&bb_diag_visible());temporal_neutral();
  if(mode==2||mode==3||mode==6||(mode>=7&&mode<=9)){temporal_edge(true);assert(!temporal_one().tapped);}
  draw();temporal_submitted();assert(!bb_diag_visible());
  if(mode==2){assert(bb_diag_drain()&&bb_diag_visible());temporal_edge(false);assert(temporal_one().tapped);temporal_neutral();temporal_fresh();}
  else if(mode==3){uint64_t gen=bb_diag_generation();view=HID_VIEW_SETTINGS;draw();assert(bb_diag_generation()>gen);temporal_completed_one();assert(!bb_diag_visible());if(utility_frame_pending)draw();assert(bb_diag_drain()&&bb_diag_visible());temporal_neutral();temporal_edge(false);assert(!temporal_one().tapped);temporal_neutral();temporal_fresh();}
  else if(mode==4){uint64_t gen=bb_diag_generation();portable_ble_buttons_native_begin();assert(bb_diag_generation()>gen&&pending_token&&bb_diag_owner()==5);draw();temporal_completed_one();assert(!bb_diag_visible());if(utility_frame_pending)draw();assert(bb_diag_drain()&&bb_diag_visible());temporal_neutral();temporal_fresh();}
  else if(mode==5){assert(bb_diag_drain());portable_ble_buttons_native_begin();hid_native_layout_select();assert(app->frame_ready());app->clear();app->present(false);for(unsigned i=0;!pending_token&&i<20000;i++)assert(!temporal_one().tapped);assert(pending_token&&bb_diag_owner()==0);assert(bb_diag_drain()&&!bb_diag_visible());temporal_edge(true);assert(!temporal_one().tapped);temporal_edge(false);assert(!temporal_one().tapped);temporal_neutral();draw();assert(bb_diag_drain()&&bb_diag_visible());temporal_neutral();temporal_fresh();}
  else if(mode==6){temporal_edge(false);temporal_edge(true);temporal_edge(false);assert(bb_diag_drain()&&bb_diag_visible());assert(temporal_one().tapped);assert(!temporal_one().tapped);temporal_neutral();temporal_fresh();}
  else if(mode>=7&&mode<=9){snapshot_fault=mode-6;bool drained=bb_diag_drain();assert(!bb_diag_visible());if(mode==7){assert(!drained&&bb_diag_failed());}else{assert(drained);snapshot_fault=0;temporal_edge(false);temporal_neutral();draw();assert(bb_diag_drain()&&bb_diag_visible());temporal_neutral();temporal_fresh();}}
  else if(mode==10){presentation_fault=true;assert(!bb_diag_drain()&&bb_diag_failed()&&pending_token);pending_image_check();}
  else assert(!"Unknown temporal mode");
 }
#ifdef HID_RENDER_FULL_PROFILE
 if(mode==7||mode==10){assert(full_retains==1);unsigned g=grants,f=frames,u=subs;app_module_fini();assert(grants==g&&frames==f&&subs==u&&g);pending_image_check();printf("TEMPORAL case=%u retained=%u grants=%u subs=%u pending=%llu PASS\n",mode,full_retains,grants,subs,(unsigned long long)pending_token);return 0;}
#endif
 assert(!pending_token&&!frames);app_module_fini();assert(!grants&&!subs&&!frames);printf("TEMPORAL case=%u generation=%llu raw_sequence=%llu PASS\n",mode,(unsigned long long)bb_diag_generation(),(unsigned long long)touch_sequence);return 0;
}
