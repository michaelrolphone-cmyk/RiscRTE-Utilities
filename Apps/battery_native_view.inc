/* Native Battery view uses the same rectangles for rendering and input.
 * No provider access, layout bitmap or shared lifecycle mutation occurs here. */
static battery_native_layout power_native_layout(unsigned controls){
 battery_native_layout l={0};
 (void)battery_native_layout_make(&l,power_app->screen_width(),power_app->screen_height(),controls);
 return l;
}
static bool power_native_hit(int x,int y,unsigned controls,unsigned which){
 battery_native_layout l=power_native_layout(controls);
 return which<controls&&battery_native_inside(l.footer[which],x,y);
}
static bool power_native_back_hit(int x,int y){return battery_native_inside(power_native_layout(2).back,x,y);}
static unsigned power_detail_count(void);
/* Bounded fit at native font metrics. The ellipsis is visible and stays inside
 * the same rectangle; no intermediate bitmap or fractional glyph scaling. */
static void power_native_text(unsigned face,battery_native_rect r,const char *text,uint32_t color){
 if(r.w<=0||r.h<12)return;
 int size=portable_nova_measure(face,text);if(size>r.w&&face!=2){face=2;size=portable_nova_measure(face,text);}
 char fit[100];
 if(size>r.w){
  size_t n=strlen(text);if(n>95)n=95;
  memcpy(fit,text,n);fit[n]=0;
  while(n && portable_nova_measure(face,fit)+portable_nova_measure(face,"...")>r.w)fit[--n]=0;
  if(portable_nova_measure(face,"...")>r.w)return;
  memcpy(fit+n,"...",4);text=fit;size=portable_nova_measure(face,text);
 }
 int inset=(r.w-size)/2;
 portable_nova_native_text(face,r.x+inset,r.y,r.w-inset,text,color);
}
static void power_native_button(battery_native_rect r,const char *text){
 if(r.w<=0||r.h<=0)return;
 portable_nova_native_round(r.x,r.y,r.w,r.h,6,NOVA_DIM);
 if(r.w>2&&r.h>2)portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,5,0x031115u);
 if(r.w>8&&r.h>=16)power_native_text(2,(battery_native_rect){r.x+4,r.y+(r.h-12)/2-2,r.w-8,12},text,NOVA_CYAN);
}
static void power_native_begin(battery_native_layout l,const char *title){
 portable_nova_begin();power_native_button(l.back,"<");
 if(l.title.h>=16)power_native_text(NOVA_FACE_ORBITRON_10,(battery_native_rect){l.title.x,l.title.y+(l.title.h-12)/2-2,l.title.w,12},title,NOVA_CAP);
}
static void power_native_line(battery_native_rect c,int offset,const char *text,uint32_t color){
 if(offset<0||offset+12>c.h)return;
 power_native_text(2,(battery_native_rect){c.x,c.y+offset,c.w,12},text,color);
}
static void power_overview(void){
 battery_native_layout l=power_native_layout(2);battery_native_rect c=l.content;
 uint32_t color=power_color();char value[20];
 power_native_begin(l,"BATTERY");
 if(power_percent_known())snprintf(value,sizeof(value),"%u%% estimate",(unsigned)power_sample.percent);else strcpy(value,"Unavailable");
 if(c.h>=120&&c.w>=176){
  int diameter=battery_native_min(c.w-16,c.h-42),r=diameter/2-4,cx=c.x+c.w/2,cy=c.y+diameter/2;
  for(unsigned i=0;i<100;i++){
   int x=cx+power_circle[i][0]*r/100,y=cy+power_circle[i][1]*r/100;
   portable_nova_native_round(x-2,y-2,5,5,2,power_percent_known()&&i<power_sample.percent?color:NOVA_LINE);
  }
  if(power_percent_known()){
   char digits[4];snprintf(digits,sizeof(digits),"%u",(unsigned)power_sample.percent);
   int digits_width=portable_nova_measure(4,digits),left=cx-(digits_width+15)/2;
   portable_nova_native_text(4,left,cy-25,digits_width+1,digits,color);
   portable_nova_native_text(1,left+digits_width+3,cy-8,12,"%",color);
   int interior=2*r-24;
   const char *caption=portable_nova_measure(2,"Battery estimate")<=interior?"Battery estimate":portable_nova_measure(2,"Estimate")<=interior?"Estimate":"Est.";
   power_native_text(2,(battery_native_rect){cx-interior/2,cy+12,interior,12},caption,NOVA_CAP);
  }else power_native_text(1,(battery_native_rect){c.x,cy-10,c.w,20},value,color);
  power_native_line(c,diameter+2,power_title(),color);
  power_native_line(c,diameter+18,power_caption(),NOVA_CAP);
 }else{
  power_native_text(1,(battery_native_rect){c.x,c.y,c.w,c.h>=20?20:c.h},value,color);
  power_native_line(c,22,power_title(),color);
  power_native_line(c,38,power_caption(),NOVA_CAP);
 }
 power_native_button(l.footer[0],"Details");power_native_button(l.footer[1],power_read_ok?"Refresh":"Retry");
 portable_battery_native_present(false);
}
static void power_native_row(battery_native_rect c,const char *label,const char *value){
 if(c.h>=28){power_native_line(c,0,label,NOVA_WHITE);power_native_line(c,16,value,NOVA_CYAN);}
 else if(c.h>=12){char line[96];snprintf(line,sizeof(line),"%s: %s",label,value);power_native_line(c,0,line,NOVA_CYAN);}
}
static void power_detail_view(void){
 battery_native_layout l=power_native_layout(3);battery_native_rect c=l.content;char value[40];
 power_native_begin(l,"DETAILS");
 int rows_height=c.h>=88?c.h-18:c.h;
 battery_native_rect first={c.x,c.y,c.w,rows_height/2},second={c.x,c.y+rows_height/2,c.w,rows_height-rows_height/2};
 if(power_detail_page==0){
  if(power_read_ok)snprintf(value,sizeof(value),"%u mV",(unsigned)power_sample.millivolts);else strcpy(value,"Unavailable");
  power_native_row(first,"Battery voltage",value);power_native_row(second,"USB available",power_yes_no(PORTABLE_POWER_INPUT_READY));
 }else if(power_detail_page==1){power_native_row(first,"Charger enabled",power_yes_no(PORTABLE_POWER_CHARGER_ENABLED));power_native_row(second,"Charge complete",power_yes_no(PORTABLE_POWER_CHARGE_DONE));}
 else if(power_detail_page==2){power_native_row(first,"Battery present",power_yes_no(PORTABLE_POWER_BATTERY_PRESENT));power_native_row(second,"PMIC thermal limit",power_yes_no(PORTABLE_POWER_THERMAL_LIMIT));}
 else if(power_detail_count()==6){
  if(power_detail_page==3){power_native_line(c,0,"PMIC owns charging",NOVA_WHITE);power_native_line(c,20,"Continues in sleep",NOVA_TEXT);}
  else if(power_detail_page==4){power_native_line(c,0,"USB and protection",NOVA_TEXT);power_native_line(c,20,"conditions apply",NOVA_TEXT);}
  else {power_native_line(c,0,"No cell temperature",NOVA_CAP);power_native_line(c,20,"or time-to-full",NOVA_CAP);}
 }else{
  power_native_line(c,0,"Charging is autonomous",NOVA_WHITE);
  power_native_line(c,16,"Sleep keeps PMIC charging",NOVA_TEXT);
  power_native_line(c,32,"USB/protection still apply",NOVA_TEXT);
  power_native_line(c,48,"No cell temperature",NOVA_CAP);
  power_native_line(c,64,"or time-to-full measurement",NOVA_CAP);
 }
 if(c.h>=88){snprintf(value,sizeof(value),"%u / %u%s",power_detail_page+1,power_detail_count(),power_read_ok?"":" - Read error");power_native_line(c,c.h-14,value,power_read_ok?NOVA_CAP:POWER_RED);}
 power_native_button(l.footer[0],"Prev");power_native_button(l.footer[1],power_read_ok?(l.footer[1].w<60?"Read":"Refresh"):"Retry");power_native_button(l.footer[2],"Next");
 portable_battery_native_present(false);
}
#ifdef PORTABLE_BLE_BROADCAST
static void power_broadcast_view(void){
 battery_native_layout l=power_native_layout(1);battery_native_rect c=l.content;
 power_native_begin(l,"BLE BROADCAST");
 if(c.w<200 || c.h<116){
  /* Keep failure and open-broadcast privacy visible even on compact panels. */
  const char *state=power_broadcast_error?"Not saved. Try again.":power_broadcast_state.omitted_fields?"Some fields omitted":power_broadcast_label();
  power_native_line(c,0,state,power_broadcast_error?POWER_RED:power_broadcast_state.omitted_fields?POWER_AMBER:NOVA_CYAN);
  power_native_line(c,18,"Open, no pairing",NOVA_CAP);
  power_native_line(c,36,"No cell temperature",NOVA_CAP);
 }else{
 power_native_line(c,0,power_broadcast_label(),NOVA_CYAN);
 power_native_line(c,20,"Available sensor values only",NOVA_TEXT);
 power_native_line(c,36,"No estimated temperature",NOVA_TEXT);
 power_native_line(c,52,"Open to nearby receivers",NOVA_CAP);
 power_native_line(c,68,"without pairing",NOVA_CAP);
 if(power_broadcast_state.omitted_fields)power_native_line(c,88,"Some fields exceed BLE limit",POWER_AMBER);
 if(power_broadcast_error)power_native_line(c,104,"Not saved. Try again.",POWER_RED);
 }
 power_native_button(l.footer[0],power_broadcast_error?"Retry":power_broadcast_state.enabled?"Disable":"Enable");
 portable_battery_native_present(false);
}
#endif
