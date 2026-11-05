/* Touchpad-native controls and gesture area use the same capability pixels. */
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
 hid_native_layout l=hid_native_geometry();char text[64];
 portable_nova_begin();hid_native_button(l.back,"",false);
 int icon=hid_native_min(20,hid_native_min(l.back.w,l.back.h)-8);
 if(icon>0)(void)portable_nova_native_icon(l.back.x+(l.back.w-icon)/2,l.back.y+(l.back.h-icon)/2,icon,"solid:f053",NOVA_CYAN);
 hid_native_text(l.height>=360?0:1,l.title,view==HID_VIEW_SETTINGS?"HID SETTINGS":"BLE TOUCHPAD",NOVA_CAP);
 if(status.state==RISC_HID_PAIR_CONFIRM&&token){
  hid_native_text(2,hid_native_cell(l.content,0,1,3,0),l.width<200?"Compare host code":"Compare with your host",NOVA_TEXT);
  snprintf(text,sizeof(text),"%06lu",(unsigned long)status.pairing_number);
  hid_native_text(0,hid_native_cell(l.content,1,1,3,0),text,NOVA_CYAN);
  hid_native_text(2,hid_native_cell(l.content,2,1,3,0),l.width<240?"Accept if codes match":"Accept only if both numbers match",NOVA_CAP);
  hid_native_button(l.footer[0],"Reject",false);hid_native_button(l.footer[1],"Accept",true);
 }else if(view==HID_VIEW_FORGET){
  hid_native_text(2,hid_native_cell(l.content,0,1,3,0),"Forget paired host?",NOVA_TEXT);
  hid_native_text(2,hid_native_cell(l.content,1,1,3,0),l.width<240?"Remove Watch on host":"Remove Watch in host settings",NOVA_CAP);
  hid_native_text(2,hid_native_cell(l.content,2,1,3,0),"before pairing again.",NOVA_CAP);
  hid_native_button(l.footer[0],"Cancel",false);hid_native_button(l.footer[1],"Forget",false);
 }else if(view==HID_VIEW_SETTINGS){
  hid_native_button(l.setting_pair[0],"Pair",false);hid_native_button(l.setting_pair[1],"Reconnect",true);
  hid_native_button(l.setting_forget,"Forget host",false);
  hid_native_text(2,hid_native_cell(l.content,2,1,3,l.gap),"Back returns to touchpad",NOVA_CAP);
 }else{
  hid_native_text(2,l.status,message?message:"Pair or reconnect in Settings",NOVA_CAP);
  hid_native_rect pad=hid_touchpad_native_pad(l);
  portable_nova_native_round(pad.x,pad.y,pad.w,pad.h,6,NOVA_LINE);
  hid_native_rect inner=hid_native_inset(pad,3);
  hid_native_text(1,hid_native_cell(inner,0,1,3,0),"Move pointer",NOVA_TEXT);
  hid_native_text(2,hid_native_cell(inner,1,1,3,0),"Tap / double-tap drag",NOVA_CAP);
  hid_native_text(2,hid_native_cell(inner,2,1,3,0),"Two fingers: scroll",NOVA_CAP);
  hid_native_button(l.footer[0],token?"Stop":"Reconnect",false);hid_native_button(l.footer[1],"Settings",false);
 }
 portable_ble_buttons_native_present(false);dirty=false;
}
static void hid_native_tap(int x,int y){
 hid_native_layout l=hid_native_geometry();
 if(view==HID_VIEW_FORGET){
  if(hid_native_inside(l.footer[0],x,y)){view=HID_VIEW_MAIN;message="Forget canceled";dirty=true;}
  else if(hid_native_inside(l.footer[1],x,y)){forget();dirty=true;}
  return;
 }
 if(view==HID_VIEW_SETTINGS){
  if(hid_native_inside(l.setting_pair[0],x,y)){view=HID_VIEW_MAIN;start_session(true);}
  else if(hid_native_inside(l.setting_pair[1],x,y)){view=HID_VIEW_MAIN;start_session(false);}
  else if(hid_native_inside(l.setting_forget,x,y))view=HID_VIEW_FORGET;
  dirty=true;return;
 }
 if(hid_native_inside(l.footer[0],x,y)){if(token)stop_session("Stopped");else start_session(false);}
 else if(hid_native_inside(l.footer[1],x,y)){stop_session("Stopped");view=HID_VIEW_SETTINGS;dirty=true;}
}
