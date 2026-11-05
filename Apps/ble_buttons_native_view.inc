/* Native Buttons alone opts in. All rectangles are shared with hit testing. */
static void hid_native_text(unsigned face,hid_native_rect r,const char*s,uint32_t color){
 if(!s||r.w<1||r.h<12)return;
 if((face==0&&r.h<24)||(face==1&&r.h<18))face=2;
 int size=portable_nova_measure(face,s);
 if(size>r.w&&face!=2){face=2;size=portable_nova_measure(face,s);}
 char fit[100];
 if(size>r.w){
  size_t n=strlen(s);if(n>95)n=95;memcpy(fit,s,n);fit[n]=0;
  int dots=portable_nova_measure(face,"...");if(dots>r.w)return;
  while(n&&portable_nova_measure(face,fit)+dots>r.w)fit[--n]=0;
  memcpy(fit+n,"...",4);s=fit;size=portable_nova_measure(face,s);
 }
 int text_height=face==0?22:face==1?16:12;
 int inset=(r.w-size)/2;
 portable_nova_native_text(face,r.x+inset,r.y+(r.h-text_height)/2-2,r.w-inset,s,color);
}
static hid_native_rect hid_native_inset(hid_native_rect r,int amount){
 if(r.w<=2*amount||r.h<=2*amount)return (hid_native_rect){0};
 return (hid_native_rect){r.x+amount,r.y+amount,r.w-2*amount,r.h-2*amount};
}
static void hid_native_button(hid_native_rect r,const char*s,bool active){
 portable_nova_native_round(r.x,r.y,r.w,r.h,6,active?NOVA_CYAN:NOVA_DIM);
 hid_native_rect inner=hid_native_inset(r,1);
 portable_nova_native_round(inner.x,inner.y,inner.w,inner.h,5,active?0x062d35u:0x031115u);
 hid_native_text(1,hid_native_inset(r,3),s,NOVA_CYAN);
}
static void hid_native_draw(void){
 hid_native_layout l=hid_native_geometry();char text[64],title[24];
 snprintf(title,sizeof(title),"BUTTON %u",selected+1);
 portable_nova_begin();hid_native_button(l.back,"",false);
 int icon=hid_native_min(20,hid_native_min(l.back.w,l.back.h)-8);
 if(icon>0)(void)portable_nova_native_icon(l.back.x+(l.back.w-icon)/2,l.back.y+(l.back.h-icon)/2,icon,"solid:f053",NOVA_CYAN);
 hid_native_text(NOVA_FACE_ORBITRON_10,l.title,view==HID_VIEW_EDIT?(save_failed?"SAVE FAILED":title):view==HID_VIEW_MODIFIERS?"MODIFIERS":view==HID_VIEW_SETTINGS?"HID SETTINGS":"BLE BUTTONS",NOVA_CAP);
 if(status.state==RISC_HID_PAIR_CONFIRM&&token){
  hid_native_rect r=hid_native_cell(l.content,0,1,3,0);
  hid_native_text(2,r,l.width<200?"Compare host code":"Compare with your host",NOVA_TEXT);
  snprintf(text,sizeof(text),"%06lu",(unsigned long)status.pairing_number);
  hid_native_text(0,hid_native_cell(l.content,1,1,3,0),text,NOVA_CYAN);
  hid_native_text(2,hid_native_cell(l.content,2,1,3,0),l.width<240?"Accept if codes match":"Accept only if both numbers match",NOVA_CAP);
  hid_native_button(l.footer[0],"Reject",false);hid_native_button(l.footer[1],"Accept",true);
 }else if(view==HID_VIEW_MODIFIERS){
  static const char*names[]={"L Ctrl","L Shift","L Alt","L GUI","R Ctrl","R Shift","R Alt","R GUI"};
  for(unsigned i=0;i<8;i++)hid_native_button(l.modifiers[i],names[i],!!(draft.modifiers&(1u<<i)));
 }else if(view==HID_VIEW_EDIT){
  hid_native_button(l.edit_kind,kind_name(draft.kind),false);
  hid_native_button(l.edit_prev,"<",false);hid_native_button(l.edit_next,">",false);
  assignment_label(&draft,text);hid_native_text(1,l.edit_value,text,NOVA_TEXT);
  if(draft.kind==HID_ACTION_KEY){snprintf(text,sizeof(text),"Modifiers: 0x%02X",draft.modifiers);hid_native_button(l.edit_modifiers,text,false);}
  hid_native_button(l.footer[0],"Cancel",false);hid_native_button(l.footer[1],save_failed?"Retry save":"Save",true);
 }else if(view==HID_VIEW_FORGET){
  hid_native_text(2,hid_native_cell(l.content,0,1,3,0),"Forget paired host?",NOVA_TEXT);
  hid_native_text(2,hid_native_cell(l.content,1,1,3,0),l.width<240?"Remove Watch on host":"Remove Watch in host settings",NOVA_CAP);
  hid_native_text(2,hid_native_cell(l.content,2,1,3,0),"before pairing again.",NOVA_CAP);
  hid_native_button(l.footer[0],"Cancel",false);hid_native_button(l.footer[1],"Forget",false);
 }else if(view==HID_VIEW_SETTINGS){
  hid_native_button(l.setting_pair[0],"Pair",false);hid_native_button(l.setting_pair[1],"Reconnect",true);
  hid_native_button(l.setting_forget,"Forget host",false);
  for(unsigned i=0;i<4;i++){snprintf(text,sizeof(text),"%u",i+1);hid_native_button(l.setting_buttons[i],text,false);}
  hid_native_text(2,(hid_native_rect){l.footer[0].x,l.footer[0].y,l.width-2*l.footer[0].x,l.footer[0].h},l.width<200?"1-4: edit mapping":"Choose a button to configure",NOVA_CAP);
 }else{
  hid_native_text(2,l.status,message?message:"Pair or reconnect in Settings",NOVA_CAP);
  for(unsigned i=0;i<4;i++){
   hid_native_rect r=l.buttons[i];bool ready=assignment_ready(assignments+i);
   portable_nova_native_round(r.x,r.y,r.w,r.h,6,held_button==(int)i?NOVA_CYAN:ready?NOVA_DIM:NOVA_LINE);
   assignment_label(assignments+i,text);
   hid_native_rect inner=hid_native_inset(r,3);
   if(inner.h>=32){
    hid_native_text(1,(hid_native_rect){inner.x,inner.y,inner.w,inner.h/2},text,ready?NOVA_TEXT:NOVA_CAP);
    if(assignments[i].modifiers){
     static const char*mods[]={"LC","LS","LA","LG","RC","RS","RA","RG"};unsigned used=0;text[0]=0;
     for(unsigned bit=0;bit<8;bit++)if(assignments[i].modifiers&(1u<<bit))used+=(unsigned)snprintf(text+used,sizeof(text)-used,"%s%s",used?" ":"",mods[bit]);
    }else snprintf(text,sizeof(text),"%u / %s",i+1,assignments[i].kind==0?"key":"mouse");
    hid_native_text(2,(hid_native_rect){inner.x,inner.y+inner.h/2,inner.w,inner.h-inner.h/2},text,NOVA_CAP);
   }else hid_native_text(2,inner,text,ready?NOVA_TEXT:NOVA_CAP);
  }
  hid_native_button(l.footer[0],token?"Stop":"Reconnect",false);hid_native_button(l.footer[1],"Settings",false);
 }
 portable_ble_buttons_native_present(false);dirty=false;
}
static void hid_native_tap(int x,int y){
 hid_native_layout l=hid_native_geometry();
 if(view==HID_VIEW_MODIFIERS){for(unsigned i=0;i<8;i++)if(hid_native_inside(l.modifiers[i],x,y)){draft.modifiers^=(uint8_t)(1u<<i);dirty=true;break;}return;}
 if(view==HID_VIEW_EDIT){
  if(hid_native_inside(l.footer[0],x,y)){view=HID_VIEW_MAIN;message="Changes canceled";}
  else if(hid_native_inside(l.footer[1],x,y)){if(save_draft())view=HID_VIEW_MAIN;}
  else if(hid_native_inside(l.edit_kind,x,y))change_kind();
  else if(hid_native_inside(l.edit_prev,x,y))change_value(-1);
  else if(hid_native_inside(l.edit_next,x,y))change_value(1);
  else if(draft.kind==HID_ACTION_KEY&&hid_native_inside(l.edit_modifiers,x,y))view=HID_VIEW_MODIFIERS;
  dirty=true;return;
 }
 if(view==HID_VIEW_FORGET){
  if(hid_native_inside(l.footer[0],x,y)){view=HID_VIEW_MAIN;message="Forget canceled";dirty=true;}
  else if(hid_native_inside(l.footer[1],x,y)){forget();dirty=true;}
  return;
 }
 if(view==HID_VIEW_SETTINGS){
  if(hid_native_inside(l.setting_pair[0],x,y)){view=HID_VIEW_MAIN;start_session(true);}
  else if(hid_native_inside(l.setting_pair[1],x,y)){view=HID_VIEW_MAIN;start_session(false);}
  else if(hid_native_inside(l.setting_forget,x,y))view=HID_VIEW_FORGET;
  else for(unsigned i=0;i<4;i++)if(hid_native_inside(l.setting_buttons[i],x,y)){selected=i;draft=assignments[i];view=HID_VIEW_EDIT;message=NULL;save_failed=false;break;}
  dirty=true;return;
 }
 if(hid_native_inside(l.footer[0],x,y)){if(token)stop_session("Stopped");else start_session(false);}
 else if(hid_native_inside(l.footer[1],x,y)){stop_session("Stopped");view=HID_VIEW_SETTINGS;dirty=true;}
}
