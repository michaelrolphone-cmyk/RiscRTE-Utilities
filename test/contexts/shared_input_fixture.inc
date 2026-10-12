/* Fault transport wraps the real ui.text-input host; successful calls always
 * execute its shipping implementation, backed by the real scene host above. */
static const risc_driver_v2 *text_driver;
static const risc_text_entry_api_v1 *text_real;
static risc_text_entry_api_v1_home_reason text_api;
static risc_text_entry_api_v1 text_old;
static bool text_attention_pending;
static bool text_active,text_granted,text_closing,text_custom,text_frozen,text_usb_connected,text_usb_subscribed;
static unsigned text_reason,text_opens,text_polls,text_closes,text_releases,text_close_again,text_stage,text_action,text_wait,text_open_steps;
static unsigned text_poll_frozen,text_native_calls,text_cycles;
static uint64_t text_token;
static const char *text_mode;
static char text_initial[33],text_expected[33],text_input[100];
static cr_store text_before;
static uint8_t text_stored_before[CR_FILE_BYTES];
static risc_text_entry_state_v1 text_observed;
static risc_usb_keyboard_event_v1 text_keys[2];
static unsigned text_key_at,text_key_count;
static uint64_t text_key_sequence;
static bool tm(const char*s){return !strcmp(text_mode,s);}
static bool text_fault(const char*s){return text_custom&&tm(s);}
static void text_boundary(const char*site){assert(native_alive);text_native_calls++;if(text_custom&&text_mode&&!strncmp(text_mode,"loss-",5)&&!strcmp(text_mode+5,site))native_alive=false;}
static uint64_t text_usb_subscribe(void*c,uint64_t d){(void)c;assert(!d&&!text_usb_subscribed);text_usb_subscribed=true;return 78;}
static bool text_usb_unsubscribe(void*c,uint64_t s){(void)c;assert(s==78&&text_usb_subscribed);text_usb_subscribed=false;return true;}
static bool text_usb_poll(void*c,size_t n){(void)c;assert(n<=8);return true;}
static int32_t text_usb_next(void*c,uint64_t s,risc_usb_keyboard_event_v1*out){(void)c;assert(s==78);if(text_key_at==text_key_count)return 0;*out=text_keys[text_key_at++];return 1;}
static bool text_usb_snapshot(void*c,risc_usb_keyboard_state_v1*out,size_t*n){(void)c;assert(*n);*n=text_usb_connected?1:0;if(*n)*out=(risc_usb_keyboard_state_v1){.device=9,.connected=1};return true;}
static const risc_usb_keyboard_api_v1 text_usb={1,sizeof(text_usb),NULL,text_usb_subscribe,text_usb_unsubscribe,text_usb_poll,text_usb_next,text_usb_snapshot};
static void text_hardware(unsigned ch){
 unsigned usage=0,mods=0;
 if(ch>='a'&&ch<='z')usage=4+ch-'a';else if(ch>='A'&&ch<='Z'){usage=4+ch-'A';mods=2;}
 else if(ch>='1'&&ch<='9')usage=30+ch-'1';else if(ch=='0')usage=39;
 else switch(ch){case ' ':usage=0x2c;break;case '!':usage=30;mods=2;break;case '?':usage=0x38;mods=2;break;case '_':usage=0x2d;mods=2;break;case '[':usage=0x2f;break;case ']':usage=0x30;break;case '{':usage=0x2f;mods=2;break;case '}':usage=0x30;mods=2;break;case '|':usage=0x31;mods=2;break;case '~':usage=0x35;mods=2;break;case RISC_SCENE_KEY_BACKSPACE:usage=0x2a;break;case RISC_SCENE_KEY_DONE:usage=0x28;break;case RISC_SCENE_KEY_CANCEL:usage=0x29;break;default:assert(0);}
 assert(text_key_at==text_key_count);text_key_at=0;text_key_count=1;text_keys[0]=(risc_usb_keyboard_event_v1){++text_key_sequence,9,3,(uint8_t)usage,(uint8_t)mods,0};
}
static int32_t text_open(void*c,const risc_text_entry_request_v1*q,uint64_t*out){
 (void)c;assert(!cs_session&&!text_active&&!held&&!presenting&&!subscribed&&text_granted);
 for(unsigned i=0;i<cs_document.node_count;i++)assert(cs_document.nodes[i].kind!=RISC_SCENE_KEYBOARD_NODE);
 assert(q->capacity==(cu_name_reason==4?33u:17u));assert(q->reserved&RISC_TEXT_ENTRY_REQUEST_HOME_REASON);
 text_opens++;text_open_steps=service_steps;text_before=cu_store;memcpy(text_stored_before,stored,sizeof(stored));strcpy(text_initial,cu_name);
 text_boundary("open");if(!native_alive)return RISC_TEXT_ENTRY_RETAINED;
 if(text_fault("retry")&&text_opens==1)return RISC_TEXT_ENTRY_UNAVAILABLE;
 if(text_fault("open-unavailable"))return RISC_TEXT_ENTRY_UNAVAILABLE;
 if(text_fault("open-busy"))return RISC_TEXT_ENTRY_BUSY;
 if(text_fault("open-retained"))return RISC_TEXT_ENTRY_RETAINED;
 if(text_fault("open-token")){*out=41;return RISC_TEXT_ENTRY_UNAVAILABLE;}
 int rc=text_real->open(text_real->context,q,out);if(!rc){text_active=true;text_closing=false;text_token=*out;}
 return rc;
}
static int32_t text_poll(void*c,uint64_t s,risc_text_entry_state_v1*out){
 (void)c;assert(text_active&&text_granted&&!cs_session&&s==text_token&&!text_closing);text_polls++;
 assert(!memcmp(stored,text_stored_before,sizeof(stored)));assert(!memcmp(&cu_store,&text_before,sizeof(cu_store)));
 text_boundary("poll");if(!native_alive)return RISC_TEXT_ENTRY_RETAINED;
 if(text_fault("poll-retained"))return RISC_TEXT_ENTRY_RETAINED;
 if(text_fault("poll-error"))return RISC_TEXT_ENTRY_INVALID;
 int rc=text_real->poll(text_real->context,s,out);
 if(!rc){
  if(text_fault("poll-revision"))out->revision=0;
  if(text_fault("poll-flags"))out->flags|=128;
  if(text_fault("poll-state"))out->state=99;
  if(text_fault("poll-unterminated"))memset(out->text,'A',sizeof(out->text));
  if(text_fault("poll-control"))out->text[0]='\n';
  text_observed=*out;text_attention_pending=true;if(text_frozen)text_poll_frozen++;
 }
 return rc;
}
static int32_t text_close(void*c,uint64_t s){
 (void)c;assert(text_active&&text_granted&&!cs_session&&s==text_token);text_closing=true;text_closes++;if(text_fault("home-close"))native_exit_pending=true;
 assert(!strcmp(cu_name,text_initial));assert(!memcmp(&cu_store,&text_before,sizeof(cu_store)));
 text_boundary("close");if(!native_alive)return RISC_TEXT_ENTRY_RETAINED;
 if(text_fault("close-retained"))return RISC_TEXT_ENTRY_RETAINED;
 if(text_fault("close-error"))return RISC_TEXT_ENTRY_INVALID;
 int rc=text_real->close(text_real->context,s);if(rc==RISC_TEXT_ENTRY_AGAIN)text_close_again++;if(!rc){text_active=false;text_token=0;}return rc;
}
static bool text_test_acquire(risc_runtime_capability_v1*g){
 assert(!text_granted);text_boundary("acquire");if(!native_alive)return false;
 if(text_fault("missing"))return false;
 if(text_fault("acquire-dirty")){g->slot=13;return false;}
 text_granted=true;g->api=text_fault("old-host")?(const void*)&text_old:&text_api;g->slot=++app_acquires;g->generation=1;return true;
}
static bool text_test_is_grant(const risc_runtime_capability_v1*g){return g->api==&text_api||g->api==&text_old;}
static bool text_test_release(risc_runtime_capability_v1*g){
 assert(text_granted&&!text_active);assert(!text_opens||!cs_session);text_boundary("release");if(!native_alive)return false;
 if(text_fault("release-fail"))return false;
 if(text_fault("release-dirty"))return true;
 if(text_fault("home-release"))native_exit_pending=true;
 text_granted=false;text_releases++;app_releases++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 if(text_fault("resume-fault"))bad_snapshot=true;
 return true;
}
static bool text_test_exit_requested(void){
 if(!text_custom||!text_mode)return native_exit_pending;
 bool after_release=text_releases&&!text_granted;
 if((text_attention_pending&&tm("loss-attention"))||
    (after_release&&!cs_session&&tm("loss-resume"))||
    (after_release&&cs_session&&tm("loss-post-resume")))native_alive=false;
 if((after_release&&!cs_session&&tm("home-resume"))||
    (after_release&&cs_session&&tm("home-post-resume")))native_exit_pending=true;
 text_attention_pending=false;
 return native_exit_pending;
}
static void text_key(unsigned value){
 if(text_usb_connected){text_hardware(value);return;}
 int x=0,y=0;assert(contexts_test_target(1,(int)value,&x,&y));touch_tap((unsigned)x,(unsigned)y);
}
static void text_header(bool home){int x,y;assert(contexts_test_header(home,&x,&y));touch_tap((unsigned)x,(unsigned)y);}
static void text_route(void){
 switch(text_stage++){
 case 0:assert(cu_page==CU_HOME);tap(text_reason==2||text_reason==4?CU_ROW_RULES:CU_ROW_LIBRARY,-999);break;
 case 1:if(text_reason==2||text_reason==4)tap(CU_ROW_RULE,-999);else if(text_reason==3)tap(CS_TAB,2);else tap(text_reason?CU_ROW_ITEMS:CU_ROW_NEW,-999);break;
 case 2:if(tm("prior-edit-cancel")){tap(CU_ROW_ENABLE,1);break;}if(text_reason==3)tap(CU_ROW_ITEMS+2,-999);else if(text_reason==4)tap(CU_ROW_WORKFLOW,-999);else {tap(CU_ROW_RENAME,-999);text_stage=10;}break;
 case 3:if(text_reason==4)tap(CU_ROW_ADD_STEP,-999);else {tap(CU_ROW_RENAME,-999);text_stage=10;}break;
 case 4:tap(CU_ROW_MESSAGE,-999);text_stage=10;break;
 default:assert(0);
 }
}
static void text_prepare_input(void){
 text_input[0]=0;
 if(tm("limit"))memset(text_input,'A',sizeof(text_input)-1),text_input[sizeof(text_input)-1]=0;
 else if(tm("hardware"))strcpy(text_input,"Az 09!?_[]{}|~");
 else if(tm("reopen")){text_input[0]=(char)('A'+text_cycles);text_input[1]=0;}
 else if(!tm("empty"))strcpy(text_input,!strncmp(text_mode,"frozen",6)||tm("rapid")?"QWERTYUIOPASDF":"A");
 strcpy(text_expected,text_initial);size_t n=strlen(text_expected),cap=text_reason==4?33:17;
 for(unsigned i=0;text_input[i]&&n+1<cap;i++)text_expected[n++]=text_input[i];
 text_expected[n]=0;
}
static bool text_test_yield(void){
 if(!text_custom){
  if(!text_active)return cu_page==CU_NAME&&!cs_text_retry;
  if(event_at!=event_count||nav_pressed)return true;
  if(text_closing){allow_complete=true;return true;}
  if(phase==25){assert(cu_page==CU_NAME);frame("keyboard");text_key('A');phase++;}
  else if(phase==26){assert(strstr(text_observed.text,"A"));text_key(RISC_SCENE_KEY_DONE);phase++;}
  return true;
 }
 if(native_exit_pending)return true;
 if(text_stage==40){if(event_at!=event_count||nav_pressed)return true;assert(cu_page==cu_return);native_exit_pending=true;text_stage=30;return true;}
 if(text_stage>=31){
  if(event_at!=event_count||nav_pressed)return true;
  if(tm("learn-save")){
   if(text_stage==31){assert(cu_page==CU_LEARN&&learning);ready=true;text_stage=32;return true;}
   if(cu_page!=CU_DONE)return true;
   assert(learn_saves==1&&revision==2&&!strcmp(cu_store.items[cu_item].name,text_expected));
  }else{
   assert(tm("explicit-save"));
   if(text_stage==31){assert(cu_page==CU_RULE);tap(CU_ROW_SAVE,-999);text_stage=32;return true;}
   assert(revision==2&&!cu_pending&&!cu_edit);
  }
  cr_store decoded;assert(cr_decode(&decoded,stored,sizeof(stored)));assert(!memcmp(&decoded,&cu_saved,sizeof(decoded)));
  native_exit_pending=true;text_stage=30;return true;
 }
 if(text_stage<10){if(cs_session&&event_at==event_count&&!nav_pressed)text_route();return true;}
 if(text_active){
  if(text_closing){assert(++text_wait<1000);if(text_wait==12){allow_complete=true;busy_acquires=0;}return true;}
  if(event_at!=event_count||nav_pressed||text_key_at!=text_key_count)return true;
  if(!text_action){text_prepare_input();if(tm("delayed"))presentation_delay=500;if(!strncmp(text_mode,"frozen",6)){allow_complete=false;text_frozen=true;}text_action=1;return true;}
  if((tm("delayed")||!strncmp(text_mode,"frozen",6))&&!text_wait){if(!presenting)return true;text_wait=1;}
  if(tm("empty")&&text_action<=strlen(text_initial)){text_key(RISC_SCENE_KEY_BACKSPACE);text_action++;return true;}
  unsigned at=text_action-1;
  if(text_input[at]){text_key((unsigned char)text_input[at]);text_action++;return true;}
  if(tm("home")||tm("frozen-home"))nav_pressed=RISC_NAV_HOME;
  else if(tm("header-home"))text_header(true);
  else if((tm("cancel")||tm("frozen-back")||tm("prior-edit-cancel"))||(tm("reopen")&&text_cycles==1))nav_pressed=RISC_NAV_BACK;
  else if(tm("header-back"))text_header(false);
  else if(tm("native-home")){strcpy(cs_launch,"calculator.elf");native_exit_pending=true;}
  else text_key(RISC_SCENE_KEY_DONE);
  text_stage=20;return true;
 }
 if(!cs_session||(!text_opens&&!cs_text_retry))return true;
 if(tm("retry")&&text_opens==1){
  if(!cs_text_retry)return true;
  assert(cs_text_retry&&text_releases==1&&cu_page==CU_NAME);
  if(++text_wait<8)return true;
  tap(CS_TEXT_RETRY,-999);text_wait=0;return true;
 }
 if(tm("reopen")&&text_opens<=text_cycles)return true;
 if(tm("reopen")&&text_cycles<2){
  assert(text_releases==text_cycles+1);
  assert(text_cycles? !strcmp(cu_name,text_initial):!strcmp(cu_name,text_expected));
  text_cycles++;text_action=0;text_wait=0;tap(CU_ROW_RENAME,-999);return true;
 }
 if(event_at!=event_count||nav_pressed)return true;
 assert(!text_granted);assert(text_releases==text_opens||tm("missing")||tm("old-host"));
 if(tm("missing")||tm("old-host")||tm("open-unavailable")||tm("open-busy")||tm("empty")){
  assert(cu_page==CU_NAME&&!memcmp(&cu_store,&text_before,sizeof(cu_store)));
  assert(ctx_message&&ctx_message[0]);back_input();text_stage=40;return true;
 }else{
  bool cancel=tm("cancel")||tm("frozen-back")||tm("prior-edit-cancel")||tm("header-back")||(tm("reopen")&&text_cycles==1);
  if(cancel){assert(!strcmp(cu_name,text_initial));assert(!memcmp(&cu_store,&text_before,sizeof(cu_store)));}
  else{
   if(strcmp(cu_name,text_expected))fprintf(stderr,"name actual=%s expected=%s observed=%s stage=%u action=%u opens=%u polls=%u page=%u\n",cu_name,text_expected,text_observed.text,text_stage,text_action,text_opens,text_polls,cu_page);
   assert(!strcmp(cu_name,text_expected));
   if(text_reason==0){assert(cu_page==CU_INTRO&&!learning&&!learn_saves&&!profile_renames);}
   if(text_reason==1){assert(cu_page==CU_ITEM&&profile_renames==1&&!strcmp(cu_store.items[0].name,text_expected));}
   if(text_reason==2){assert(cu_page==CU_RULE&&cu_edit&&!strcmp(cu_store.rules[0].name,text_expected));}
   if(text_reason==3){assert(cu_page==CU_TRIGGER&&cu_edit&&!strcmp(cu_store.items[2].name,text_expected));}
   if(text_reason==4){assert(cu_page==CU_WORKFLOW&&cu_edit&&cu_store.rules[0].steps==1&&!strcmp(cu_store.rules[0].workflow[0].text,text_expected));}
  }
 }
 if(tm("learn-save")){tap(CU_ROW_START,-999);text_stage=31;return true;}
 if(tm("explicit-save")){if(text_reason==4){back_input();text_stage=31;}else{tap(CU_ROW_SAVE,-999);text_stage=32;}return true;}
 if(tm("prior-edit-cancel"))assert(cu_edit&&cu_store.rules[0].enabled);
 text_stage=30;native_exit_pending=true;return true;
}
static void text_test_setup(const char*mode){
 text_custom=!strncmp(mode,"text-",5);text_mode=text_custom?mode+7:"legacy";
 if(text_custom){assert(mode[5]>='0'&&mode[5]<='4'&&mode[6]=='-');text_reason=(unsigned)(mode[5]-'0');}
 text_driver=contexts_text_driver_get(2);assert(text_driver);text_real=text_driver->capability;
 text_api=(risc_text_entry_api_v1_home_reason){{1,sizeof(text_api),NULL,text_open,text_poll,text_close},RISC_TEXT_ENTRY_HOME_REASON_TAG,RISC_TEXT_ENTRY_HOME_REASON_VERSION};
 text_old=(risc_text_entry_api_v1){1,sizeof(text_old),NULL,NULL,text_poll,text_close};
 text_usb_connected=text_custom&&tm("hardware");
 risc_provider_dependency_v1 deps[]={{RISC_SCENE_CAPABILITY,1,api},{"usb.hid.keyboard",1,&text_usb}};assert(text_driver->start(deps,2));
 if(text_custom&&(tm("missing")||tm("old-host")))text_before=bank;
}
static bool text_test_finish(void){
 if(!text_custom){if(native_alive){assert(text_driver->quiesce());text_driver->stop();}return false;}
 bool terminal=!strncmp(text_mode,"loss-",5)||strstr(text_mode,"retained")||!strncmp(text_mode,"poll-",5)||tm("open-token")||tm("acquire-dirty")||!strncmp(text_mode,"close-",6)||!strncmp(text_mode,"release-",8)||tm("resume-fault");
 if(terminal){assert(!native_alive&&cs_terminal&&!app_launches&&!profile_renames&&!learn_saves);assert(revision==1);}
 else{
  assert(native_alive&&!text_active&&!text_granted&&!held&&!presenting&&!subscribed&&!text_usb_subscribed);assert(app_acquires==app_releases&&!app_launches);
  assert(text_driver->quiesce());text_driver->stop();assert(driver->quiesce());driver->stop();
  bool home=tm("home")||tm("frozen-home")||tm("header-home")||tm("native-home")||!strncmp(text_mode,"home-",5);
  assert(home?ctx_exit:text_stage==30);
  if(home){assert(!strcmp(cu_name,text_initial)&&!profile_renames&&!learn_saves&&revision==1&&!cs_launch[0]);}
  if(!strncmp(text_mode,"frozen",6)){assert(text_poll_frozen>10&&text_close_again>=10);assert(text_observed.state==(tm("frozen")?RISC_TEXT_ENTRY_ACCEPTED:RISC_TEXT_ENTRY_CANCELLED));assert(!strcmp(text_observed.text,text_expected));}
  if(tm("delayed"))assert(text_close_again);
  if(text_opens&&text_polls>3)assert(service_steps>text_open_steps);
  if(tm("reopen"))assert(text_opens==3&&text_releases==3&&!strcmp(cu_name,"HOMEAC"));
  if(tm("retry"))assert(text_opens==2&&text_releases==2);
 }
 printf("Contexts shared text %s reason=%u %s PASS (opens=%u polls=%u close_again=%u releases=%u)\n",text_mode,text_reason,device,text_opens,text_polls,text_close_again,text_releases);return true;
}
