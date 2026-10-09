#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
static bool bp_release_seen;
#endif
/* Nova7 retaining-MONO1 presentation. The shared passive scanner and leased
 * sensor provider lifecycle remain authoritative. RF is changed only by an explicit tap. */
static bool bp_down,bp_list;
static int bp_x,bp_y,bp_last_y;
static unsigned bp_detail_lines;
static int bp_w(void){return app->screen_width();}
static int bp_h(void){return app->screen_height();}
static unsigned bp_rows(void){return (unsigned)(bp_h()-256)/88u;}
static void bp_text(int x,int y,int w,const char*s,bool heading,bool black){paper->text(x,y,w,s,(heading?2u:1u)|PAPER_TEXT_LITERAL,heading,black);}
static void bp_button(int x,int y,int w,const char*s,const char*icon,bool selected){
 app->fill_rect(x,y,w,88,true);if(!selected)app->fill_rect(x+3,y+3,w-6,82,false);
 int n=paper->measure(s,false),left=x+(w-n-(icon?36:0))/2;
 if(icon){(void)app->draw_icon(left,y+31,icon,24,!selected);left+=36;}
 bp_text(left,y+30,x+w-left-6,s,false,!selected);
}
static void bp_header(const char*s){
 bp_button(32,16,64,"","solid:f053",false);
 bp_text(112,37,bp_w()-144,s,true,true);app->fill_rect(32,106,bp_w()-64,3,true);
}
static void bp_line(unsigned row,const char*s){if(row>=detail_scroll&&row<detail_scroll+11)bp_text(40,120+(int)(row-detail_scroll)*46,bp_w()-80,s,false,true);}
/* The shared keyboard characters/actions use spacious paper hit cells. */
static portable_watch_key_rect bp_key_bounds(unsigned key){
 int width=bp_w()-64;
 if(key<PWK_CHARACTERS)return (portable_watch_key_rect){32+(int)(key%8)*(width/8),210+(int)(key/8)*88,width/8-4,80};
 return (portable_watch_key_rect){32+(int)(key-PWK_PAGE)*(width/3),570,width/3-4,72};
}
static void paper_draw(void){
 paper->begin();bp_header(naming?"SENSOR NAME":detail?"SENSOR DETAILS":"BLE SCANNER");
 if(naming){
  bp_text(32,126,bp_w()-64,editing[0]?editing:"Type a name",false,true);
  for(unsigned key=0;key<PWK_COUNT;key++){
   portable_watch_key_rect r=bp_key_bounds(key);
   app->fill_rect(r.x,r.y,r.w,r.h,true);app->fill_rect(r.x+2,r.y+2,r.w-4,r.h-4,key_choice==key);
   unsigned ch=portable_watch_key_character(key_page,key);char label[2]={(char)(ch?ch:' '),0};
   const char*caption=key==PWK_PAGE?"ABC/#":key==PWK_DELETE?"DELETE":key==PWK_DONE?"SAVE":ch==32?"_":label;
   bp_text(r.x+4,r.y+22,r.w-8,caption,false,key_choice!=key);
  }
  bp_text(32,bp_h()-146,bp_w()-64,name_save_failed?message:"Empty name clears; Back cancels",false,true);
  app->present(false);dirty=false;return;
 }
 unsigned list[BLE_MAX_DEVICES],n=indexes(list);char text[96];
 if(detail&&detail_index<scan.count){
  unsigned row=detail_lines(bp_line);
  bp_detail_lines=row;
  snprintf(text,sizeof(text),"LINES %u-%u OF %u",detail_scroll+1,detail_scroll+11<row?detail_scroll+11:row,row);
 }else{
  if(scroll>=n)scroll=0;
  for(unsigned row=0;row<bp_rows()&&scroll+row<n;row++){
   unsigned i=scroll+row;ble_device*d=&scan.devices[list[i]];int y=112+(int)row*88;bool chosen=i==selected;char addr[32];address(d,addr);
   if(chosen)app->fill_rect(32,y,bp_w()-64,88,true);
   (void)app->draw_icon(44,y+28,"solid:f7c0",28,!chosen);
   bp_text(88,y+10,bp_w()-136,aliases[list[i]][0]?aliases[list[i]]:d->name[0]?d->name:addr,false,!chosen);
   if(d->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");else snprintf(text,sizeof(text),"%s  %d dBm",d->bthome?"BTHome":d->service_count?"Services":"BLE",d->rssi);
   bp_text(88,y+44,bp_w()-136,text,false,!chosen);app->fill_rect(32,y+85,bp_w()-64,3,true);
  }
  if(!n)bp_text(40,164,bp_w()-80,active()?"Listening for advertisements":"No devices in results",false,true);
  snprintf(text,sizeof(text),"%u RESULTS  %s",n,sensors?"SENSORS":"ALL DEVICES");
 }
 bp_text(112,78,bp_w()-144,text,false,true);
 bp_text(32,bp_h()-146,bp_w()-64,message?message:"Tap Scan to discover",false,true);
 int w=(bp_w()-80)/2;
 bp_button(32,bp_h()-112,w,detail?"NAME":paper_enable_needed?"ENABLE BT":active()?"STOP":"SCAN",detail?NULL:"solid:f7c0",true);
 bp_button(bp_w()/2+8,bp_h()-112,w,detail?"MORE":sensors?"SHOW ALL":"SENSORS",detail?"solid:f054":NULL,false);
 app->present(false);dirty=false;
}
static void paper_enable(void){
 if(active()||uncertain)return;
 risc_runtime_capability_v1 kg={.struct_size=sizeof(kg)},hg={.struct_size=sizeof(hg)};uint8_t flags=0,prior=0,state=0;
 bool hardware_changed=false,policy_attempted=false,okay=false;
 message="Bluetooth enable failed";
 if(!runtime->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&kg)){message="Radio settings unavailable";goto done;}
 if(!portable_radio_load(kg.api,&flags)){message="Radio settings unreadable";goto done;}
 if(flags&PORTABLE_RADIO_AIRPLANE){message="Turn off Airplane mode first";goto done;}
 if(!runtime->acquire("bluetooth.hci",1,16,&hg)){message="Bluetooth unavailable";goto done;}
 const portable_bluetooth_control_v1*c=hg.api;
 if(!c||c->api_version!=1||c->struct_size<sizeof(*c)||!c->set_enabled||!c->status){message="Bluetooth controls unavailable";goto done;}
 if(!c->status(c->context,&prior)||prior>PORTABLE_BLUETOOTH_ON){message="Bluetooth busy - retry";goto done;}
 hardware_changed=true;
 if(c->set_enabled(c->context,true)&&c->status(c->context,&state)&&state==PORTABLE_BLUETOOTH_ON){policy_attempted=true;okay=portable_radio_save(kg.api,(uint8_t)(flags|PORTABLE_RADIO_BLUETOOTH));}
 if(!okay){
  if(policy_attempted&&!portable_radio_save(kg.api,flags)){uncertain=true;retain();}
  if(hardware_changed&&(!c->set_enabled(c->context,prior==PORTABLE_BLUETOOTH_ON)||!c->status(c->context,&state)||state!=prior)){uncertain=true;retain();}
 }else {paper_enable_needed=false;message="Bluetooth enabled. Tap Scan.";}
 done:
 if(hg.api&&!runtime->release(&hg)){uncertain=true;retain();}
 if(kg.api&&!runtime->release(&kg)){uncertain=true;retain();}
 dirty=true;
}
static void bp_more(void){if(detail){detail_scroll=detail_scroll+11<bp_detail_lines?detail_scroll+11:0;dirty=true;}else{sensors=!sensors;scroll=selected=0;dirty=true;}}
static bool paper_input(const t5_app_input_t*i){
 springboard_contact c={0};paper->contact(&c);bool tap=false,gesture=false;int x=0,y=0;
#ifdef PORTABLE_NATIVE_TIME_TOOLBAR
 /* Shared QuickActions may reject its top-edge gesture and return one
  * eligible began+released contact. Consume that replay exactly once. */
 if(!c.valid||c.cancelled||!c.tap_eligible){bp_down=false;c.down=false;}
 if(!c.released)bp_release_seen=false;
 if(c.valid&&c.began&&c.released&&c.tap_eligible&&!c.cancelled&&!bp_release_seen){bp_down=true;bp_x=c.x;bp_y=bp_last_y=c.y;bp_list=false;}
 if(c.released)bp_release_seen=true;
#endif
 if(c.down){if(!bp_down){bp_down=true;bp_x=c.x;bp_y=c.y;bp_list=!naming&&c.y>=112&&c.y<bp_h()-176;}bp_last_y=c.y;}
 else if(bp_down){int dy=bp_last_y-bp_y;bp_down=false;if(bp_list&&(dy>=44||dy<=-44)){gesture=true;if(detail){if(dy<0)bp_more();else{detail_scroll=detail_scroll>=11?detail_scroll-11:0;dirty=true;}}else move(dy<0?(int)bp_rows():-(int)bp_rows());}else if(dy<44&&dy>-44){tap=true;x=bp_x;y=bp_y;}}
 if((i->buttons&T5_APP_BUTTON_BACK)||(tap&&portable_nova_hit(x,y,32,16,64,88)))return navigate_back();
 if(naming){
  if(i->buttons&(T5_APP_BUTTON_LEFT|T5_APP_BUTTON_UP))key_choice=key_choice?key_choice-1:PWK_COUNT-1;
  if(i->buttons&(T5_APP_BUTTON_RIGHT|T5_APP_BUTTON_DOWN))key_choice=(key_choice+1)%PWK_COUNT;
  if(i->buttons&T5_APP_BUTTON_CONFIRM)name_key(key_choice);
  if(tap)for(unsigned key=0;key<PWK_COUNT;key++){portable_watch_key_rect r=bp_key_bounds(key);if(portable_nova_hit(x,y,r.x,r.y,r.w,r.h)){key_choice=key;name_key(key);break;}}
  if(i->buttons)dirty=true;
  return true;
 }
 if(i->buttons&T5_APP_BUTTON_UP)move(-1);
 if(i->buttons&T5_APP_BUTTON_DOWN)move(1);
 bool action=(i->buttons&T5_APP_BUTTON_CONFIRM)!=0;
 int w=(bp_w()-80)/2;
 if(tap){
  if(portable_nova_hit(x,y,32,bp_h()-112,w,88)){if(detail)begin_name();else action=true;}
  else if(portable_nova_hit(x,y,bp_w()/2+8,bp_h()-112,w,88))bp_more();
  else if(!detail&&portable_nova_hit(x,y,32,112,bp_w()-64,(int)bp_rows()*88)){unsigned list[BLE_MAX_DEVICES],n=indexes(list),at=scroll+(unsigned)(y-112)/88;if(at<n){selected=at;detail_index=list[at];detail=true;detail_scroll=0;dirty=true;}}
 }
 if(action){if(detail)begin_name();else if(paper_enable_needed)paper_enable();else start_scan();}
 if(detail&&bp_detail_lines&&detail_scroll>=bp_detail_lines)detail_scroll=bp_detail_lines>11?bp_detail_lines-11:0;
 if(dirty&&(tap||gesture||i->buttons)&&scan.phase!=BLE_STARTING)draw();
 return true;
}
