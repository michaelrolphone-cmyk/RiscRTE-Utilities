static void scan_native_text(unsigned face,scan_native_rect r,const char*s,uint32_t color){
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
static scan_native_rect scan_native_inset(scan_native_rect r,int amount){
 if(r.w<=2*amount||r.h<=2*amount)return (scan_native_rect){0};
 return (scan_native_rect){r.x+amount,r.y+amount,r.w-2*amount,r.h-2*amount};
}
static void scan_native_button(scan_native_rect r,const char*s,bool active){
 portable_nova_native_round(r.x,r.y,r.w,r.h,6,active?NOVA_CYAN:NOVA_DIM);
 scan_native_rect inner=scan_native_inset(r,1);
 portable_nova_native_round(inner.x,inner.y,inner.w,inner.h,5,active?0x062d35u:0x031115u);
 scan_native_text(1,scan_native_inset(r,3),s,NOVA_CYAN);
}
/* Identity is the current action-to-device mapping, not volatile RSSI/text.
 * New scans and naming transitions invalidate prior contacts even if addresses
 * happen to repeat. The adapter only arms the completed matching generation. */
static void scan_native_select(void){
 if(!scan_native_enabled())return;
 uint64_t id=UINT64_C(1469598103934665603);unsigned list[BLE_MAX_DEVICES],n=indexes(list);
 const uint64_t values[]={scan_native_epoch,(uint32_t)app->screen_width(),(uint32_t)app->screen_height(),detail,sensors,naming,scroll,detail_scroll,detail_index,n,active()};
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){id^=values[i];id*=UINT64_C(1099511628211);}
 for(unsigned i=0;i<n;i++){ble_device*d=&scan.devices[list[i]];id^=list[i];id*=UINT64_C(1099511628211);id^=d->address_type;id*=UINT64_C(1099511628211);for(unsigned j=0;j<6;j++){id^=d->address[j];id*=UINT64_C(1099511628211);}}
 portable_ble_scanner_native_layout(id);
}
static void scan_native_bar(scan_native_rect r,unsigned first,unsigned rows,unsigned total){
 if(total<=rows||r.h<1)return;
 int size=r.h*(int)rows/(int)total;if(size<4)size=scan_native_min(4,r.h);
 int top=(r.h-size)*(int)first/(int)(total-rows);
 portable_nova_native_fill(r.x+r.w-2,r.y,2,r.h,NOVA_LINE);
 portable_nova_native_fill(r.x+r.w-2,r.y+top,2,size,NOVA_CYAN);
}
static void scan_native_line(unsigned index,const char*s){
 scan_native_layout l=scan_native_geometry();
 if(index>=detail_scroll&&index<detail_scroll+l.lines){
  scan_native_rect r={l.details.x,l.details.y+(int)(index-detail_scroll)*l.line_height,l.details.w-4,l.line_height};
  scan_native_text(l.line_height>=21?1:2,r,s,NOVA_TEXT);
 }
}
static void scan_native_draw(void){
 scan_native_layout l=scan_native_geometry();unsigned list[BLE_MAX_DEVICES],n=indexes(list);char text[80];
 portable_nova_begin();scan_native_button(l.back,"<",false);
 scan_native_text(l.height>=360?0:1,l.title,detail?"SENSOR DETAILS":"BLE SCANNER",NOVA_CAP);
 if(detail&&detail_index<scan.count){
  unsigned lines=detail_lines(scan_native_line);scan_native_bar(l.details,detail_scroll,l.lines,lines);
  scan_native_button(l.footer[0],"Results",false);scan_native_button(l.footer[1],name_save_failed?"Retry name":"Name",false);
 }else{
  scan_native_text(l.height>=360?1:2,l.status,message?message:"Tap Scan to discover",NOVA_CAP);
  for(unsigned row=0;row<l.rows&&row+scroll<n;row++){
   unsigned idx=row+scroll;ble_device*d=&scan.devices[list[idx]];char name[32];address(d,name);
   scan_native_rect r=scan_native_row(l,row);portable_nova_native_round(r.x,r.y,r.w,r.h,5,idx==selected?NOVA_DIM:NOVA_LINE);
   scan_native_rect inner=scan_native_inset(r,3);int top=inner.h/2;
   scan_native_text(1,(scan_native_rect){inner.x,inner.y,inner.w-2,top},aliases[list[idx]][0]?aliases[list[idx]]:d->name[0]?d->name:name,NOVA_TEXT);
   if(d->rssi==127)snprintf(text,sizeof(text),"Signal unavailable");else snprintf(text,sizeof(text),"%s %d dBm",d->bthome?"BTHome":d->service_count?"Services":"BLE",d->rssi);
   scan_native_text(2,(scan_native_rect){inner.x,inner.y+top,inner.w-2,inner.h-top},text,NOVA_CAP);
  }
  if(!n)scan_native_text(1,l.list,active()?"Listening...":"No devices",NOVA_TEXT);
  scan_native_bar(l.list,scroll,l.rows,n);
  scan_native_button(l.footer[0],active()?"Stop":"Scan",false);scan_native_button(l.footer[1],sensors?"Sensors":"All devices",sensors);
 }
 portable_ble_scanner_native_present(false);dirty=false;
}
static void scan_native_tap(int x,int y,bool*action){
 scan_native_layout l=scan_native_geometry();
 if(scan_native_hit(l.footer[0],x,y)){if(detail){detail=name_save_failed=false;dirty=true;}else *action=true;}
 else if(scan_native_hit(l.footer[1],x,y)){if(detail)begin_name();else{sensors=!sensors;scroll=selected=0;dirty=true;}}
 else if(!detail){unsigned list[BLE_MAX_DEVICES],n=indexes(list);for(unsigned row=0;row<l.rows&&scroll+row<n;row++)if(scan_native_hit(scan_native_row(l,row),x,y)){selected=scroll+row;detail_index=list[selected];detail=true;detail_scroll=0;dirty=true;break;}}
}
