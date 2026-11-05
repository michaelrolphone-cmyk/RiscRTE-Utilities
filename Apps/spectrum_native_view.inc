/* Native Audio Spectrum presentation. The model and controllers are shared
 * with the opt-out profile; targets are rebuilt from current state before
 * dispatch, even when the display has not accepted the latest frame. */
typedef struct {spectrum_native_rect r;int x,y;} spectrum_native_target;
static spectrum_native_target native_targets[64];
static unsigned native_target_count;
static spectrum_native_rect native_toast;
static spectrum_native_rect nrow(spectrum_native_rect r,unsigned i,unsigned count){
 spectrum_native_rect rotated={r.y,r.x,r.h,r.w};
 rotated=spectrum_native_column(rotated,i,count,native_layout.gap);
 return (spectrum_native_rect){rotated.y,rotated.x,rotated.h,rotated.w};
}
static spectrum_native_rect ninset(spectrum_native_rect r,int amount){
 int x=spectrum_native_min(amount,r.w/2),y=spectrum_native_min(amount,r.h/2);
 return (spectrum_native_rect){r.x+x,r.y+y,r.w-2*x,r.h-2*y};
}
static spectrum_native_rect nclip(spectrum_native_rect r,spectrum_native_rect clip){
 int left=r.x>clip.x?r.x:clip.x,top=r.y>clip.y?r.y:clip.y;
 int64_t right=(int64_t)r.x+r.w,bottom=(int64_t)r.y+r.h;
 if(right>(int64_t)clip.x+clip.w)right=(int64_t)clip.x+clip.w;
 if(bottom>(int64_t)clip.y+clip.h)bottom=(int64_t)clip.y+clip.h;
 return (spectrum_native_rect){left,top,right>left?(int)(right-left):0,bottom>top?(int)(bottom-top):0};
}
static void native_target(spectrum_native_rect r,int x,int y){
 r=nclip(r,(spectrum_native_rect){0,0,native_layout.width,native_layout.height});
 if(r.w>0&&r.h>0&&native_target_count<64)native_targets[native_target_count++]=(spectrum_native_target){r,x,y};
}
/* Native glyphs are measured, never scaled. A bounded two-line wrap uses the
 * available rectangle; an unfinished final line is explicitly ellipsized. */
static void ntext(bool paint,spectrum_native_rect r,unsigned face,const char *s,uint32_t color){
 if(!paint||!s||r.w<=4||r.h<=0)return;
 /* Short labels should remain one word when an existing smaller font fits.
  * These faces have complete letter coverage; numeric-only faces cannot be
  * substituted for body text. Long prose still wraps at word boundaries. */
 if(!strchr(s,' ')&&text_width(face,s)>r.w-4&&text_width(2,s)<=r.w-4)face=2;
 int line_height=face==0?18:face==1?17:face==2?14:13;
 if(r.h<line_height){face=2;line_height=14;}
 if(r.h<line_height){face=3;line_height=13;}
 if(r.h<line_height)return;
 int rows=r.h/line_height;if(rows>2)rows=2;
 char lines[2][96]={{0}};unsigned used=0;
 for(int row=0;row<rows&&s[used];row++){
  unsigned n=0,space=0;
  while(s[used+n]&&n<sizeof(lines[0])-1){lines[row][n]=s[used+n];lines[row][n+1]=0;if(text_width(face,lines[row])>r.w-4){lines[row][n]=0;break;}if(s[used+n]==' ')space=n;n++;}
  if(s[used+n]&&row+1<rows&&space){n=space;lines[row][n]=0;}
  used+=n;while(s[used]==' ')used++;
  if(s[used]&&row+1==rows){while(n&&text_width(face,lines[row])+text_width(face,"...")>r.w-4)lines[row][--n]=0;size_t room=sizeof(lines[row])-n-1;if(room>=3&&text_width(face,"...")<=r.w-4)memcpy(lines[row]+n,"...",4);}
 }
 int actual=lines[1][0]?2:1;int y=r.y+(r.h-actual*line_height)/2;
 for(int row=0;row<actual;row++)center(face,r.x+2,y+row*line_height,r.w-4,lines[row],color);
}
static void npill(bool paint,spectrum_native_rect r,const char *s,bool selected,int x,int y){
 native_target(r,x,y);if(!paint||r.w<=0||r.h<=0)return;
 int radius=spectrum_native_min(16,spectrum_native_min(r.w,r.h)/2);
 round_rect(r.x,r.y,r.w,r.h,radius,selected?NOVA_CYAN:NOVA_DIM);
 if(!selected&&r.w>2&&r.h>2)round_rect(r.x+1,r.y+1,r.w-2,r.h-2,radius>0?radius-1:0,0);
 ntext(true,ninset(r,2),1,s,selected?0x001418u:NOVA_CYAN);
}
static unsigned spectrum_native_monitor_rows(void){
 int reserved=lab_edit?40:100,space=native_layout.content.h-reserved;
 unsigned rows=space>0?(unsigned)space/(lab_edit?48u:38u):1u;
 return rows?rows:1;
}
static int spectrum_native_controls_max(void){
 int maximum=CONTROLS_COUNT*56-native_layout.content.h;return maximum>0?maximum:0;
}
static void spectrum_native_cursor(char caption[40]){
 pill_w=pill_h=0;caption[0]=0;
 if(!cursor_visible||page!=PAGE_MAIN||!started||view>=2)return;
 spectrum_native_rect r=native_layout.plot;if(r.w<8||r.h<8)return;
 unsigned count=view==1?PLOT_H:PLOT_W;
 unsigned bin=spectrum_dsp_column_at(&dsp_config,cursor_hz,count);
 int p=(int)spectrum_dsp_column_at(&dsp_config,cursor_hz,(unsigned)(view==1?r.h:r.w));
 int db=view==1?fall_db[bin]:spec_db[bin];char f[20];freq_text(cursor_hz,f);
 snprintf(caption,40,"%s  %d dB",f,db/100);
 pill_w=spectrum_native_min(text_width(1,caption)+16,r.w-4);pill_h=spectrum_native_min(38,r.h-4);
 pill_x=view==1?r.x+r.w-pill_w-2:r.x+clamp_int(p-pill_w/2,2,r.w-pill_w-2);
 pill_y=view==1?r.y+clamp_int(p<pill_h+4?p+4:p-pill_h-4,2,r.h-pill_h-2):r.y+2;
}
static void native_axes(bool fall){
 spectrum_native_rect r=native_layout.plot;if(r.w<1||r.h<1)return;
 int last_end=-100;
 for(unsigned i=0;i<6;i++){
  unsigned dimension=(unsigned)(fall?r.h:r.w),at=(dimension-1)*i/5;
  unsigned hz=spectrum_dsp_frequency_at(&dsp_config,at,dimension);char label[20];short_freq(hz,label);
  if(fall){if((int)at<last_end)continue;for(int x=0;x<r.w;x+=6)fill(r.x+x,r.y+(int)at,spectrum_native_min(3,r.w-x),1,NOVA_DIM);text(1,r.x+2,r.y+clamp_int((int)at-8,0,r.h>17?r.h-17:0),r.w>48?48:r.w,label,NOVA_CAP);last_end=(int)at+24;}
  else{fill(r.x+(int)at,r.y,1,r.h,NOVA_LINE);int tw=text_width(1,label),x=clamp_int((int)at-tw/2,0,r.w>tw?r.w-tw:0);if(x>last_end){text(1,r.x+x,r.y+r.h-17,tw,label,NOVA_CAP);last_end=x+tw+5;}}
 }
 if(!fall)for(unsigned i=0;i<5;i++){int y=r.y+(r.h-1)*(int)i/4;fill(r.x,y,r.w,1,NOVA_LINE);char name[16];if(prefs.log_amplitude)snprintf(name,sizeof(name),"-%u dB",i*90/4);else snprintf(name,sizeof(name),"%u%%",100-i*25);text(1,r.x+2,clamp_int(y,r.y,r.y+(r.h>34?r.h-34:0)),r.w>48?48:r.w,name,NOVA_CAP);}
}
static void native_plot(void){
 spectrum_native_rect r=native_layout.plot;if(r.w<1||r.h<1)return;
 if(view==1){
  /* Empty history retains its right-aligned prefix. Each stored numeric cell
   * becomes a native rectangle; the analysis ring never changes size. */
  for(unsigned x=0;x<history_count;x++){
   unsigned source=(history_next+PLOT_W-history_count+x)%PLOT_W,at=PLOT_W-history_count+x;
   int x0=(int)((uint64_t)at*(unsigned)r.w/PLOT_W),x1=(int)((uint64_t)(at+1)*(unsigned)r.w/PLOT_W);
   unsigned alpha=at<12?at*255/12:at>PLOT_W-13?(PLOT_W-1-at)*255/12:255;
   for(unsigned y=0;y<PLOT_H;y++){
    int y0=(int)((uint64_t)y*(unsigned)r.h/PLOT_H),y1=(int)((uint64_t)(y+1)*(unsigned)r.h/PLOT_H);
    fill(r.x+x0,r.y+y0,x1-x0,y1-y0,fade(palette[history[source][y]],alpha));
   }
  }
  native_axes(true);
 }else{
  native_axes(false);int previous_x=r.x,previous_y=r.y+r.h-1,previous_peak=previous_y;
  for(unsigned i=0;i<PLOT_W;i++){
   int x=r.x+spectrum_native_axis(i,PLOT_W,r.w);
   int y=r.y+r.h-1-(int)((uint64_t)spec_levels[i]*(unsigned)(r.h-1)/32767u);
   int peak=r.y+r.h-1-(int)((uint64_t)peak_levels[i]*(unsigned)(r.h-1)/32767u);
   if(i){line(previous_x,previous_peak,x,peak,0x8c7429);line(previous_x,previous_y,x,y,NOVA_CYAN);}
   previous_x=x;previous_y=y;previous_peak=peak;
  }
 }
 if(prefs.show_labels)for(unsigned i=0;i<SPECTRUM_LABEL_MAX;i++){
  spectrum_label *l=&labels[i];if(!l->present||l->frequency_hz<(dsp_config.log_frequency&&dsp_config.low_hz<10?10:dsp_config.low_hz)||l->frequency_hz>dsp_config.high_hz)continue;
  int p=(int)spectrum_dsp_column_at(&dsp_config,l->frequency_hz,(unsigned)(view==1?r.h:r.w));
  uint32_t color=label_active[i]?label_colors[l->color]:fade(label_colors[l->color],102);
  int limit=view==1?r.w:r.h;for(int k=0;k<limit;k+=6){int len=spectrum_native_min(3,limit-k);if(view==1)fill(r.x+k,r.y+p,len,1,color);else fill(r.x+p,r.y+k,1,len,color);}
  if(label_active[i]){int width=spectrum_native_min(text_width(1,l->name),r.w);int x=view==1?r.x+r.w-width:r.x+clamp_int(p+4,0,r.w-width);int y=view==1?r.y+clamp_int(p-18,0,r.h>17?r.h-17:0):r.y+20+(int)(i%3)*17;fill(x,y,width,17,0);text(1,x,y,width,l->name,color);}
 }
 char detected[64];spectrum_native_rect status={r.x,r.y+r.h-54,r.w,18};
 if(cfa_room.slot>=0)snprintf(detected,sizeof(detected),"ROOM %.16s %u%%",cfa_name(CF_ROOM,"UNKNOWN"),(unsigned)(cfa_room.confidence*100.f));else snprintf(detected,sizeof(detected),"ROOM UNKNOWN");
 fill(status.x,status.y,status.w,status.h,0);ntext(true,status,1,detected,NOVA_WHITE);status.y+=18;
 if(cfa_event.slot>=0)snprintf(detected,sizeof(detected),"EVENT %.16s %u%%",cfa_name(CF_EVENT,"UNKNOWN"),(unsigned)(cfa_event.confidence*100.f));else if(event_slot>=0&&!event_match.complete)snprintf(detected,sizeof(detected),"EVENT %.16s %u%%",signatures[event_slot].name,event_confidence);else snprintf(detected,sizeof(detected),"EVENT UNKNOWN");
 fill(status.x,status.y,status.w,status.h,0);ntext(true,status,1,detected,cfa_event.slot>=0?0x3dff9au:NOVA_CAP);
 if(cfa_background())ntext(true,(spectrum_native_rect){r.x+r.w/2,r.y,r.w-r.w/2,18},1,"BG FILTER",0xffd24au);
 if(cursor_visible){
  int p=(int)spectrum_dsp_column_at(&dsp_config,cursor_hz,(unsigned)(view==1?r.h:r.w));char caption[40];spectrum_native_cursor(caption);
  if(view==1)fill(r.x,r.y+p,r.w,1,NOVA_WHITE);else fill(r.x+p,r.y,1,r.h,NOVA_WHITE);
  unsigned bin=spectrum_dsp_column_at(&dsp_config,cursor_hz,PLOT_W);
  int x=view==1?r.x+r.w-5:r.x+p,y=view==1?r.y+p:r.y+r.h-1-(int)((uint64_t)spec_levels[bin]*(unsigned)(r.h-1)/32767u);
  round_rect(x-3,y-3,7,7,3,NOVA_CYAN);
  spectrum_native_rect pill_rect={pill_x,pill_y,pill_w,pill_h};round_rect(pill_x,pill_y,pill_w,pill_h,12,NOVA_DIM);ntext(true,pill_rect,1,caption,NOVA_CYAN);
 }
}
static void native_monitor(bool paint){
 spectrum_native_rect c=native_layout.content;unsigned rows=spectrum_native_monitor_rows();char status[64];
 spectrum_native_rect title={c.x,c.y,c.w,36},edit={c.x+c.w*3/4,c.y,c.w-c.w*3/4,36};title.w=edit.x-title.x;
 if(lab_edit){snprintf(status,sizeof(status),pending_save?"EDIT LABELS / UNSAVED":"EDIT LABELS / %u SAVED",saved_count());}else if(!running)snprintf(status,sizeof(status),"%s / LAST CONTEXT",frozen?"FROZEN":"STOPPED");else if(!ambient.ready)snprintf(status,sizeof(status),"LEARNING ROOM / %u%%",ambient.frames*100u/SPECTRUM_BG_WARMUP);else if(event_match.complete&&event_match.reason==ST_RESULT_AMBIGUOUS)snprintf(status,sizeof(status),"EVENT / AMBIGUOUS");else if(event_match.complete&&event_match.reason==ST_RESULT_NEGATIVE)snprintf(status,sizeof(status),"EVENT / NONMATCH");else{monitor_item items[17];snprintf(status,sizeof(status),"%u SOUNDS%s",monitor_items(items),event_neural_used&&event_match.complete?" / NEURAL":"");}
 ntext(paint,title,1,status,NOVA_CAP);npill(paint,edit,lab_edit?"DONE":"EDIT",false,2100,0);
 int top=c.y+40;
 if(lab_edit){
  unsigned order[8],n=ordered_labels(order);
  if(!n)ntext(paint,(spectrum_native_rect){c.x,top,c.w,c.h-40},1,"Drag a frequency line, then tap its pill to name it",NOVA_CAP);
  for(unsigned row=0;row<rows&&row+list_scroll<n;row++){
   unsigned slot=order[row+list_scroll];spectrum_label *l=&labels[slot];spectrum_native_rect r={c.x,top+(int)row*48,c.w,44},del={r.x+r.w-44,r.y,44,r.h};r.w-=48;
   char frequency[20];freq_text(l->frequency_hz,frequency);snprintf(status,sizeof(status),"%.16s / %s",l->name,frequency);
   npill(paint,r,status,false,2000+(int)slot,0);npill(paint,del,"X",false,2010+(int)slot,0);
  }
 }else{
  unsigned rooms=monitor_room_count();bool verifying=room_tracker.candidate>=0&&room_tracker.candidate!=room_tracker.selected;
  bool holding=room_tracker.selected>=0&&(room_tracker.misses||room_tracker.ambiguous);int room=verifying?room_tracker.candidate:room_tracker.selected;
  const char *room_name=cfa_name(CF_ROOM,!rooms?"NO ROOM SAMPLES":!ambient.ready?"LEARNING...":room>=0&&room<(int)SPECTRUM_SIGNATURE_SLOTS&&signatures[room].kind==SPECTRUM_SIGNATURE_ROOM?cfa_name(CF_ROOM,signatures[room].name):room_tracker.ambiguous?"AMBIGUOUS":"UNKNOWN");
  char detail[64];
  if(cfa_room.slot>=0)snprintf(detail,sizeof(detail),"MATCH %u%% / SHARED ROOM",(unsigned)(cfa_room.confidence*100.f));
  else if(!rooms)snprintf(detail,sizeof(detail),"CAPTURE ROOM IN CONTROLS > SAMPLES");
  else if(!ambient.ready)snprintf(detail,sizeof(detail),"RAW ROOM PROFILE / WAIT FOR BASELINE");
  else if(room>=0&&holding)snprintf(detail,sizeof(detail),"HOLD / %s",room_tracker.ambiguous?"SIMILAR ROOM SOUNDS":"CHECKING ROOM");
  else if(room>=0)snprintf(detail,sizeof(detail),"%s %u%% AMP %d dB",verifying?"VERIFY":"MATCH",room_tracker.confidence,monitor_scene_db()/100);
  else snprintf(detail,sizeof(detail),"%s %u%% AMP %d dB",room_tracker.ambiguous?"AMBIGUOUS":"NO MATCH",room_tracker.confidence,monitor_scene_db()/100);
  spectrum_native_rect room_rect={c.x,top,c.w,54};native_target(room_rect,2110,0);
  ntext(paint,(spectrum_native_rect){c.x,top,c.w,26},1,room_name,NOVA_WHITE);
  ntext(paint,(spectrum_native_rect){c.x,top+26,c.w,28},1,detail,NOVA_CAP);top+=60;
  monitor_item items[17];unsigned n=monitor_items(items);
  if(!n)ntext(paint,(spectrum_native_rect){c.x,top,c.w,c.y+c.h-top},1,input_waiting?"WAITING FOR AUDIO":running?"NO SOUND IDENTIFIED / LEARN BELOW":"START MIC TO MONITOR",NOVA_CAP);
  for(unsigned row=0;row<rows&&row+list_scroll<n;row++){
   monitor_item *item=&items[row+list_scroll];snprintf(status,sizeof(status),"%s %.16s / %u%% / %d dB",item->kind==MONITOR_EVENT?"EVENT":item->kind==MONITOR_CANDIDATE?"MAYBE":item->kind==MONITOR_VOICE?"VOICE":"LABEL",item->name,item->confidence,item->amplitude_db/100);
   ntext(paint,(spectrum_native_rect){c.x,top+(int)row*38,c.w,36},1,status,item->color);
  }
 }
 spectrum_native_rect f=native_layout.footer;
 npill(paint,spectrum_native_column(f,0,4,native_layout.gap),"<",false,2200,0);
 npill(paint,spectrum_native_column(f,1,4,native_layout.gap),running?"STOP":"START",running,2201,0);
 npill(paint,spectrum_native_column(f,2,4,native_layout.gap),"LEARN",false,2202,0);
 npill(paint,spectrum_native_column(f,3,4,native_layout.gap),">",false,2203,0);
}
static void native_controls(bool paint){
 static const char *captions[]={"","LOW","HIGH","X SCALE","Y SCALE","LABELS","COLORS","GAIN","WINDOW","FFT SIZE","FLOOR","STORAGE","SAMPLES","EVENTS"};
 spectrum_native_rect c=native_layout.content;
 for(unsigned setting=1;setting<=CONTROLS_COUNT;setting++){
  int y=c.y+(int)(setting-1)*56-controls_scroll;
  if(y+56<=c.y||y>=c.y+c.h)continue;
  spectrum_native_rect row={c.x,y,c.w,52},label={c.x,y,c.w/3,52};ntext(paint,label,1,captions[setting],NOVA_TEXT);
  spectrum_native_rect field={c.x+c.w/3,y,c.w-c.w/3,52};char value[32];
  if(controls_segment(setting)){
   const char *a=setting==3?"LOG":setting==4?"DB":"ON",*b=setting==5?"OFF":"LIN";bool first=setting==3?prefs.log_frequency:setting==4?prefs.log_amplitude:prefs.show_labels;
   for(unsigned i=0;i<2;i++){spectrum_native_rect r=spectrum_native_column(field,i,2,native_layout.gap);unsigned before=native_target_count;npill(paint,r,i?b:a,i?!first:first,1000+(int)setting,(int)i);if(native_target_count>before)native_targets[native_target_count-1].r=nclip(r,c);}
  }else if(setting>=11){const char *s=setting>11?"OPEN":(pending_save||signature_pending||event_files.pending>=0)?"RETRY SAVE":(store_message||signature_errors||event_files.ready!=3)?"RETRY":storage?"SAVED":"RETRY";unsigned before=native_target_count;npill(paint,field,s,false,1000+(int)setting,0);if(native_target_count>before)native_targets[native_target_count-1].r=nclip(field,c);}
  else{
   switch(setting){case 1:snprintf(value,sizeof(value),"%u Hz",prefs.low_hz);break;case 2:snprintf(value,sizeof(value),"%u kHz",dsp_config.high_hz/1000);break;case 6:snprintf(value,sizeof(value),"%s",palette_names[prefs.palette]);break;case 7:snprintf(value,sizeof(value),"%+d dB",prefs.gain_db);break;case 8:snprintf(value,sizeof(value),"%s",window_names[prefs.window]);break;case 9:snprintf(value,sizeof(value),"%u",prefs.fft_size);break;default:snprintf(value,sizeof(value),"%d dB",prefs.threshold_db);break;}
   int button=spectrum_native_min(44,field.w/4);spectrum_native_rect minus={field.x,y,button,52},plus={field.x+field.w-button,y,button,52};
   unsigned before=native_target_count;npill(paint,minus,"-",false,1000+(int)setting,-1);if(native_target_count>before)native_targets[native_target_count-1].r=nclip(minus,c);
   ntext(paint,(spectrum_native_rect){field.x+button,y,field.w-2*button,52},1,value,NOVA_WHITE);
   before=native_target_count;npill(paint,plus,"+",false,1000+(int)setting,1);if(native_target_count>before)native_targets[native_target_count-1].r=nclip(plus,c);
  }
  if(paint)fill(row.x,y+55,row.w,1,NOVA_LINE);
 }
 if(paint){fill(0,0,native_layout.width,c.y,0);fill(0,c.y+c.h,native_layout.width,native_layout.height-c.y-c.h,0);}
 ntext(paint,native_layout.header,0,"CONTROLS / AUDIO",NOVA_CYAN);
 npill(paint,native_layout.footer,"BACK",false,120,215);
}
static void native_label(bool paint){
 spectrum_native_rect c=native_layout.content;char f[20];freq_text(editing.frequency_hz,f);
 ntext(paint,native_layout.header,0,"LABEL FREQUENCY",NOVA_CYAN);
 ntext(paint,nrow(c,0,3),0,f,NOVA_CYAN);
 npill(paint,nrow(c,1,3),editing.name[0]?editing.name:"NAME THIS SOUND",false,120,120);
 spectrum_native_rect colors=nrow(c,2,3);
 for(unsigned i=0;i<8;i++){spectrum_native_rect r=spectrum_native_column(colors,i,8,native_layout.gap);native_target(r,32+(int)i*23,150);if(paint){fill(r.x,r.y,r.w,r.h,label_colors[i]);if(i==editing.color){fill(r.x,r.y,r.w,2,NOVA_WHITE);fill(r.x,r.y+r.h-2,r.w,2,NOVA_WHITE);}}}
 spectrum_native_rect ftr=native_layout.footer;unsigned count=edit_slot>=0?3u:2u;
 npill(paint,spectrum_native_column(ftr,0,count,native_layout.gap),"CANCEL",false,50,185);
 if(edit_slot>=0)npill(paint,spectrum_native_column(ftr,1,count,native_layout.gap),"DELETE",false,110,185);
 npill(paint,spectrum_native_column(ftr,count-1,count,native_layout.gap),"SAVE",true,180,185);
}
#include "spectrum_native_subpages.inc"
static void native_layout_page(bool paint){
 native_target_count=0;
 if(page==PAGE_CONTROLS)native_controls(paint);
 else if(page==PAGE_LABEL)native_label(paint);
 else if(spectrum_native_subpage(paint)){}
 else if(!started){
  spectrum_native_rect c=native_layout.content;
  ntext(paint,native_layout.header,0,"AUDIO SPECTRUM",NOVA_CYAN);
  ntext(paint,nrow(c,0,3),1,capture_error?message:"LIVE MICROPHONE / 16 kHz MONO",capture_error?0xff6a5fu:NOVA_CAP);
  npill(paint,nrow(c,1,3),"START",true,120,120);
  npill(paint,nrow(c,2,3),"CONTROLS",false,120,215);
  npill(paint,native_layout.footer,"EXIT",false,25,20);
 }else{
  const char *tabs[]={"SPEC","FALL","MONITOR","SETTINGS"};int old_x[]={25,85,140,225};
  for(unsigned i=0;i<4;i++)npill(paint,native_layout.tabs[i],tabs[i],i==view,old_x[i],25);
  if(view==2)native_monitor(paint);
  else{
   if(paint)native_plot();
   npill(paint,native_layout.actions[0],running?"MIC / STOP":frozen?"FROZEN / START":"START",running,70,230);
   npill(paint,native_layout.actions[1],running?"FREEZE":"EXIT",false,200,230);
   if(paint&&(pending_save||signature_pending||event_files.pending>=0))fill(native_layout.width/2-2,native_layout.footer.y,4,4,0xff6a5fu);

  }
 }
 if(paint&&page==PAGE_MAIN&&started&&(capture_error||input_waiting)){spectrum_native_rect error={native_layout.content.x,native_layout.footer.y-40,native_layout.content.w,36};fill(error.x,error.y,error.w,error.h,NOVA_DIM);ntext(true,error,1,message,0xff6a5fu);}
 native_toast=(spectrum_native_rect){native_layout.margin,native_layout.footer.y-40,native_layout.width-2*native_layout.margin,36};
 if(toast_message&&page!=PAGE_KEYBOARD){
  native_target(native_toast,3000,0);
  if(paint){fill(native_toast.x,native_toast.y,native_toast.w,native_toast.h,NOVA_DIM);spectrum_native_rect r=native_toast;if(undo_slot>=0)r.w-=60;ntext(true,r,1,toast_message,NOVA_TEXT);}
  if(undo_slot>=0)npill(paint,(spectrum_native_rect){native_toast.x+native_toast.w-60,native_toast.y,60,native_toast.h},"UNDO",false,3001,0);
 }
}
static bool spectrum_native_draw(void){native_layout_page(true);return true;}
static bool spectrum_native_tap(int x,int y,bool *toggle_requested,bool *freeze_requested){
 if(text_active||text_client.retained||x<0||y<0||x>=native_layout.width||y>=native_layout.height)return false;
 native_layout_page(false);
 for(unsigned i=native_target_count;i;i--)if(spectrum_native_hit(native_targets[i-1].r,x,y)){
  int a=native_targets[i-1].x,b=native_targets[i-1].y;
  if(a==3000){toast_message=NULL;dirty=true;return false;}if(a==3001){undo_delete();return false;}
  if(a>=1001&&a<=1013){unsigned setting=(unsigned)a-1000;settings_change(setting,controls_segment(setting)?0:b,controls_segment(setting)?b:0);return false;}
  if(a>=2000&&a<2008){unsigned slot=(unsigned)a-2000;if(labels[slot].present)open_label((int)slot,0);return false;}
  if(a>=2010&&a<2018){delete_label(a-2010);return false;}
  if(a==2100){lab_edit=!lab_edit;list_scroll=0;set_page(PAGE_MAIN);return false;}
  if(a==2110||a==2202){shared_open(a==2110?1:2);return false;}
  if(a==2201){*toggle_requested=true;return false;}
  if(a==2200||a==2203){unsigned n;if(lab_edit)n=saved_count();else{monitor_item items[17];n=monitor_items(items);}if(a==2200&&list_scroll)list_scroll--;else if(a==2203&&list_scroll+spectrum_native_monitor_rows()<n)list_scroll++;dirty=true;return false;}
  return tap_action_legacy(a,b,toggle_requested,freeze_requested);
 }
 if(page==PAGE_MAIN&&started&&view<2){
  char caption[40];spectrum_native_cursor(caption);
  if(cursor_visible&&hit(x,y,pill_x,pill_y,pill_w,pill_h)){open_label(-1,cursor_hz);return false;}
  spectrum_native_rect r=native_layout.plot;
  if(spectrum_native_hit(r,x,y)){unsigned size=(unsigned)(view==1?r.h:r.w),at=(unsigned)(view==1?y-r.y:x-r.x),current=spectrum_dsp_column_at(&dsp_config,cursor_hz,size);if(cursor_visible&&abs_int((int)current-(int)at)>14)cursor_visible=false;else{cursor_visible=true;cursor_hz=spectrum_dsp_frequency_at(&dsp_config,at,size);}dirty=true;}
 }
 return false;
}
static bool spectrum_native_contact(void){
 if(!optional_contact())return false;
 char caption[40];spectrum_native_cursor(caption);
 t5_app_contact_t contact={0};if(!app->touch_contact(&contact))return false;
 int x=contact.x,y=contact.y;bool consumed=false;spectrum_native_rect r=native_layout.plot;
 spectrum_native_rect toast={native_layout.margin,native_layout.footer.y-40,native_layout.width-2*native_layout.margin,36};
 bool on_toast=toast_message&&spectrum_native_hit(toast,x,y);
 if(contact.down){
  if(!contact_down){contact_down=true;contact_controls=!on_toast&&page==PAGE_CONTROLS&&spectrum_native_hit(native_layout.content,x,y);controls_touch_y=y;controls_touch_scroll=controls_scroll;contact_plot=!on_toast&&page==PAGE_MAIN&&started&&view<2&&spectrum_native_hit(r,x,y)&&!(cursor_visible&&hit(x,y,pill_x,pill_y,pill_w,pill_h));contact_moved=false;
   if(contact_plot){int at=view==1?y-r.y:x-r.x;unsigned size=(unsigned)(view==1?r.h:r.w);drag_origin=at;int current=(int)spectrum_dsp_column_at(&dsp_config,cursor_hz,size);contact_drag=!cursor_visible||abs_int(current-at)<=14;if(contact_drag){cursor_hz=spectrum_dsp_frequency_at(&dsp_config,(unsigned)at,size);cursor_visible=true;dirty=true;}}
  }else if(contact_controls){if(abs_int(y-controls_touch_y)>6)contact_moved=true;if(contact_moved){controls_move(controls_touch_scroll+controls_touch_y-y);consumed=true;}}
  else if(contact_plot){unsigned size=(unsigned)(view==1?r.h:r.w);int at=clamp_int(view==1?y-r.y:x-r.x,0,(int)size-1);if(abs_int(at-drag_origin)>6){contact_moved=true;contact_drag=true;}if(contact_drag){cursor_hz=spectrum_dsp_frequency_at(&dsp_config,(unsigned)at,size);cursor_visible=true;dirty=true;}}
 }else if(contact_down){consumed=contact_plot||(contact_controls&&contact_moved);if(contact_plot&&!contact_drag&&!contact_moved){cursor_visible=false;dirty=true;}contact_down=contact_plot=contact_controls=false;}
 return consumed;
}
