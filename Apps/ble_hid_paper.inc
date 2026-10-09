/* Capability-selected portrait paper UI; the HID state machine remains shared.
 * All coordinates are logical 480x800. Raw contacts retain their native motion
 * resolution; only discrete UI releases map into the existing action handler. */
static void hp_text(int x,int y,int w,const char*s,bool heading,bool black){paper->text(x,y,w,s,(heading?2u:1u)|PAPER_TEXT_LITERAL,heading,black);}
static void hp_button(int x,int y,int w,int h,const char*s,const char*icon,bool selected){
 app->fill_rect(x,y,w,h,true);if(!selected)app->fill_rect(x+3,y+3,w-6,h-6,false);
 int n=paper->measure(s,false),left=x+(w-n-(icon?36:0))/2;if(left<x+8)left=x+8;
 if(icon){app->draw_icon(left,y+(h-24)/2,icon,24,!selected);left+=36;}
 hp_text(left,y+(h-26)/2,x+w-left-6,s,false,!selected);
}
static void paper_draw(void){
 char text[64],title[24];snprintf(title,sizeof(title),"BUTTON %u",selected+1);
 paper->begin();hp_button(32,16,64,88,"","solid:f053",false);
 hp_text(112,37,336,view==HID_VIEW_EDIT?(save_failed?"SAVE FAILED":title):view==HID_VIEW_MODIFIERS?"MODIFIERS":view==HID_VIEW_SETTINGS?"HID SETTINGS":HID_TOUCHPAD?"BLE TOUCHPAD":"BLE BUTTONS",true,true);
 app->fill_rect(32,106,416,3,true);
 if(status.state==RISC_HID_PAIR_CONFIRM&&token){
  hp_text(32,180,416,"Compare with your host",false,true);snprintf(text,sizeof(text),"%06lu",(unsigned long)status.pairing_number);
  int w=paper->measure(text,false)*3;paper->text((480-w)/2,280,416,text,1|PAPER_TEXT_CLOCK|PAPER_TEXT_LITERAL,false,true);
  hp_text(32,440,416,"Accept only if both numbers match",false,true);
  hp_button(32,688,200,88,"Reject",NULL,false);hp_button(248,688,200,88,"Accept",NULL,true);
 }else if(view==HID_VIEW_MODIFIERS){
  static const char*names[]={"L Ctrl","L Shift","L Alt","L GUI","R Ctrl","R Shift","R Alt","R GUI"};
  for(unsigned i=0;i<8;i++)hp_button(32+(int)(i%2)*216,176+(int)(i/2)*120,200,104,names[i],NULL,!!(draft.modifiers&(1u<<i)));
  hp_text(32,712,416,"Back returns to the unsaved edit",false,true);
 }else if(view==HID_VIEW_EDIT){
  hp_button(32,176,416,88,kind_name(draft.kind),"solid:f11c",false);
  hp_button(32,304,80,88,"","solid:f053",false);hp_button(368,304,80,88,"","solid:f054",false);
  assignment_label(&draft,text);hp_text(128,334,224,text,false,true);
  if(draft.kind==HID_ACTION_KEY){snprintf(text,sizeof(text),"Modifiers: 0x%02X",draft.modifiers);hp_button(32,432,416,88,text,NULL,false);}
  hp_text(32,568,416,save_failed?"Not saved - retry or cancel":"Changes remain unsaved until Save",false,true);
  hp_button(32,688,200,88,"Cancel",NULL,false);hp_button(248,688,200,88,save_failed?"Retry save":"Save",NULL,true);
 }else if(view==HID_VIEW_FORGET){
  hp_text(32,200,416,"Forget the paired host?",false,true);hp_text(32,280,416,"Remove Paper in host settings",false,true);hp_text(32,320,416,"before pairing again.",false,true);
  hp_button(32,688,200,88,"Cancel",NULL,false);hp_button(248,688,200,88,"Forget","solid:f2ed",false);
 }else if(view==HID_VIEW_SETTINGS){
  hp_button(32,176,200,88,"Pair","solid:f067",false);hp_button(248,176,200,88,"Reconnect",NULL,true);
  hp_button(32,304,416,88,"Forget host","solid:f2ed",false);
  if(!HID_TOUCHPAD){hp_text(32,440,416,"Choose a button to configure",false,true);for(unsigned i=0;i<4;i++){snprintf(text,sizeof(text),"Button %u",i+1);hp_button(32+(int)(i%2)*216,496+(int)(i/2)*104,200,88,text,NULL,false);}}
 }else{
  hp_text(32,152,416,message?message:"Pair or reconnect in Settings",false,true);
  if(HID_TOUCHPAD){app->fill_rect(32,224,416,400,true);app->fill_rect(35,227,410,394,false);hp_text(152,350,280,"TOUCHPAD",true,true);hp_text(64,410,352,"Tap: left click",false,true);hp_text(64,450,352,"Tap then drag: hold click",false,true);hp_text(64,490,352,"Two-finger tap: right click",false,true);hp_text(64,530,352,hid&&!scroll_api()?"Scroll needs driver update":"Two-finger move: scroll X/Y",false,true);hid_assignment mouse={.kind=HID_ACTION_MOUSE};hp_text(64,562,352,assignment_ready(&mouse)?"Mouse input ready":"Mouse input unavailable",false,true);}
  else for(unsigned i=0;i<4;i++){
   int x=32+(int)(i%2)*216,y=224+(int)(i/2)*208;assignment_label(assignments+i,text);hp_button(x,y,200,192,text,NULL,held_button==(int)i);
   snprintf(text,sizeof(text),"%u  %s  %02X",i+1,assignments[i].kind==0?"KEY":"MOUSE",assignments[i].modifiers);hp_text(x+16,y+136,168,text,false,held_button!=(int)i);hp_text(x+16,y+160,168,assignment_ready(assignments+i)?"READY":"UNAVAILABLE",false,held_button!=(int)i);
  }
  hp_button(32,688,200,88,token?"Stop":"Reconnect",token?"solid:f0c8":NULL,false);hp_button(248,688,200,88,"Settings","solid:f013",false);
 }
 app->present(false);dirty=false;
}
static bool hp_map(int *x,int *y,int l,int t,int w,int h,int tx,int ty){if(!portable_nova_hit(*x,*y,l,t,w,h))return false;*x=tx;*y=ty;return true;}
static bool paper_tap_map(int*x,int*y){
 if(view==HID_VIEW_MODIFIERS){for(unsigned i=0;i<8;i++)if(hp_map(x,y,32+(int)(i%2)*216,176+(int)(i/2)*120,200,104,40+(int)(i%2)*116,75+(int)(i/2)*46))return true;return false;}
 if(view==HID_VIEW_EDIT){
  if(hp_map(x,y,32,176,416,88,120,80)||hp_map(x,y,32,304,80,88,30,120)||hp_map(x,y,368,304,80,88,210,120)||hp_map(x,y,32,432,416,88,120,172))return true;
 }else if(view==HID_VIEW_SETTINGS){
  if(hp_map(x,y,32,176,200,88,50,76)||hp_map(x,y,248,176,200,88,180,76)||hp_map(x,y,32,304,416,88,120,124))return true;
  if(!HID_TOUCHPAD)for(unsigned i=0;i<4;i++)if(hp_map(x,y,32+(int)(i%2)*216,496+(int)(i/2)*104,200,88,30+(int)i*58,207))return true;
  return false;
 }
 return hp_map(x,y,32,688,200,88,50,218)||hp_map(x,y,248,688,200,88,180,218);
}

static bool hp_contact_down,hp_release_seen;
static void paper_input(t5_app_input_t*in){
 springboard_contact c={0};paper->contact(&c);
 if(!c.valid||c.cancelled||!c.tap_eligible)hp_contact_down=false;
 /* QuickActions replays a rejected top-edge tap as began+released. */
 if(!c.released)hp_release_seen=false;
 if(c.valid&&c.began&&(c.down||c.released)&&!hp_release_seen&&c.tap_eligible&&!c.cancelled)hp_contact_down=true;
 in->tapped=hp_contact_down&&c.valid&&c.released&&c.tap_eligible&&!c.cancelled;
 if(c.released){hp_contact_down=false;hp_release_seen=true;}
 if(paper_pair_release_gate){in->tapped=false;hp_contact_down=false;if(c.valid&&!c.down&&!c.released)paper_pair_release_gate=false;}
 if(in->tapped){in->touch_x=c.x;in->touch_y=c.y;}
}
