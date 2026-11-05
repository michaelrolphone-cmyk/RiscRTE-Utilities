/* Opt-in native color presentation. The existing model, storage, radio and
 * shared editor own their existing behavior; all geometry here is physical. */
static const char *lm_native_state_text(void){
 switch(model.state){
 case LM_LISTENING:return "LISTENING / RAW";
 case LM_SENDING:return "SENDING / WAIT";
 case LM_TRANSMITTED:return "SENT / NO RECEIPT";
 case LM_SEND_UNKNOWN:return "SEND UNCONFIRMED";
 case LM_RADIO_ERROR:return "RADIO ERROR / RETRY";
 case LM_CLEANUP_ERROR:return "CLEANUP UNCONFIRMED";
 default:return !model.choice?"OFF / CHOOSE RADIO":!model.profile_valid?"OFF / SET RF":!lm_profile_matches(&model.profile,model.choice)?"RF / RADIO MISMATCH":"OFF / READY";
 }
}
static unsigned lm_native_face(void){return lm_geometry.line_height>=20?1u:2u;}
static void lm_native_label(lm_native_rect r,const char*s,uint32_t color,bool center){
 if(!s||r.w<1||r.h<10)return;
 unsigned face=r.h>=20?1u:2u;int line=face==1?16:14;
 if(portable_nova_measure(face,s)>r.w-4){face=2;line=14;}
 char fit[103];size_t n=strlen(s);if(n>99)n=99;memcpy(fit,s,n);fit[n]=0;
 if(portable_nova_measure(face,fit)>r.w-4){while(n&&portable_nova_measure(face,fit)+portable_nova_measure(face,"...")>r.w-4)fit[--n]=0;memcpy(fit+n,"...",4);}
 int size=portable_nova_measure(face,fit);if(size>r.w-4)return;
 int x=r.x+(center?(r.w-size)/2:2);
 portable_nova_native_text(face,x,r.y+(r.h-line)/2,r.x+r.w-2-x,fit,color);
}
static void lm_native_button(lm_native_rect r,const char*s,bool active){
 portable_nova_native_round(r.x,r.y,r.w,r.h,4,active?NOVA_CYAN:NOVA_DIM);
 if(r.w>2&&r.h>2)portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,3,0x031115u);
 lm_native_label(r,s,active?NOVA_WHITE:NOVA_TEXT,true);
}
static void lm_native_row(lm_native_rect r,const char*label,const char*value,bool active){
 portable_nova_native_round(r.x,r.y,r.w,r.h,4,active?NOVA_DIM:NOVA_LINE);
 int half=r.h/2;
 lm_native_label((lm_native_rect){r.x+2,r.y,r.w-4,half},label,NOVA_TEXT,false);
 lm_native_label((lm_native_rect){r.x+2,r.y+half,r.w-4,r.h-half},value,NOVA_CYAN,false);
}
/* Bound by the widest printable glyph, so paging never drops a character and
 * even a draft of all wide glyphs fits the same painted physical content box. */
static unsigned lm_native_columns(void){
 unsigned face=lm_native_face();int widest=1;char glyph[2]={0};
 for(unsigned c=32;c<=126;c++){glyph[0]=(char)c;int w=portable_nova_measure(face,glyph);if(w>widest)widest=w;}
 unsigned n=(unsigned)(lm_geometry.body.w-4)/widest;return n>79?79:n?n:1;
}
static unsigned lm_native_compose_step(void){return lm_native_columns()*lm_geometry.compose_rows;}
static unsigned lm_native_detail_step(void){return lm_native_columns()*lm_geometry.detail_rows;}
static void lm_native_bytes(lm_native_rect r,const char*s,unsigned length,unsigned offset,unsigned rows){
 unsigned columns=lm_native_columns(),face=lm_native_face();char line[80];
 for(unsigned row=0;row<rows;row++){unsigned start=offset+row*columns;if(start>=length)break;unsigned n=length-start;if(n>columns)n=columns;memcpy(line,s+start,n);line[n]=0;
  portable_nova_native_text(face,r.x+2,r.y+(int)row*lm_geometry.line_height,r.w-4,line,NOVA_TEXT);}
}
/* Ordinary help prose uses the measured native font width. Message/draft
 * review keeps its fixed byte pages separately, so offsets remain reversible. */
static void lm_native_paragraph(lm_native_rect r,const char*s,unsigned rows){
 unsigned face=lm_native_face();char line[80];
 for(unsigned row=0;row<rows&&*s;row++){
  unsigned n=0;while(s[n]&&n<sizeof(line)-1){line[n]=s[n];line[n+1]=0;if(portable_nova_measure(face,line)>r.w-4)break;n++;}
  if(!n)return;
  line[n]=0;portable_nova_native_text(face,r.x+2,r.y+(int)row*lm_geometry.line_height,r.w-4,line,NOVA_TEXT);s+=n;
 }
}
static void lm_native_mapping(void){
 if(!lm_native_active)return;
 const uint64_t values[]={lm_geometry.width,lm_geometry.height,page,rf_page,history_page,detail_offset,compose_offset,picker_mask,model.choice,model.owned,model.state==LM_SENDING,edit_field,
#ifdef PORTABLE_LORA_SHARED_TEXT_INPUT
 text_active,text_closing,
#else
 key_page,
#endif
 page==PAGE_HISTORY?model.next:0,page==PAGE_HISTORY?model.count:0};
 uint64_t id=UINT64_C(1469598103934665603);for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){id^=values[i];id*=UINT64_C(1099511628211);}portable_lora_native_layout(id);
}
static void lm_native_draw(void){
 lm_native_layout*l=&lm_geometry;char line[100],clock[32];
 portable_nova_begin();portable_nova_native_fill(0,0,l->width,l->height,0x010708u);lm_native_button(l->back,"<",false);
 const char*title=page==PAGE_HOME?"LORA MESSAGES":page==PAGE_COMPOSE?"REVIEW MESSAGE":page==PAGE_RF||page==PAGE_RF_ERROR?(profile_save_state==2?"RF UNSAVED":profile_save_state==1?"RF SESSION":"RF SETTINGS"):page==PAGE_HISTORY?"SESSION HISTORY":page==PAGE_RADIO?"RADIO HARDWARE":page==PAGE_KEYBOARD?(edit_field<6?field_names[edit_field]:"COMPOSE MESSAGE"):"MESSAGE";
 lm_native_rect heading=l->title;if(page==PAGE_RF)heading.w=l->radio.x-heading.x-4;lm_native_label(heading,title,NOVA_CAP,true);
 if(page==PAGE_HOME){
  lm_native_label(l->state,lm_native_state_text(),NOVA_CYAN,false);lm_native_label(l->notice,notice?notice:profile_save_state==2?"RF save unconfirmed":profile_save_state==1?"RF: session only":radio_name(),NOVA_CAP,false);
  lm_native_button(l->home[0],"COMPOSE",false);snprintf(line,sizeof(line),"HISTORY %u / %u",model.count,LM_HISTORY_MAX);lm_native_button(l->home[1],line,false);
  lm_native_button(l->home[2],model.owned?"STOP":"LISTEN",model.listening);lm_native_button(l->home[3],"SET RF",false);
 }else if(page==PAGE_COMPOSE){
  if(!draft[0])lm_native_label(l->body,"Tap to write a message",NOVA_TEXT,true);
  else lm_native_bytes(l->body,draft,(unsigned)strlen(draft),compose_offset,l->compose_rows);
  lm_native_rect state={l->body.x,l->body.y+l->body.h,l->body.w,l->line_height};lm_native_label(state,lm_native_state_text(),NOVA_CYAN,false);state.y+=l->line_height;
  lm_native_label(state,notice?notice:model.state==LM_SEND_UNKNOWN?"Retry may duplicate":"Tap text: next page",NOVA_CAP,false);
  lm_native_button(l->footer[0],"EDIT",false);lm_native_button(l->footer[1],model.state==LM_SENDING?"CANCEL":"SEND",false);
 }else if(page==PAGE_RF){
  lm_native_button(l->radio,"RADIO",false);
  for(unsigned row=0;row<3;row++){unsigned i=rf_page*3+row;char value[32];if(rf_fields&(1u<<i))snprintf(value,sizeof(value),"%lld",(long long)field_value(i));else snprintf(value,sizeof(value),"NOT SET");lm_native_row(l->rf[row],field_names[i],value,false);}
  lm_native_button(l->footer[0],rf_page?"PREVIOUS":"NEXT",false);lm_native_button(l->footer[1],"APPLY",false);
 }else if(page==PAGE_HISTORY){
  if(!model.count){lm_native_label(l->state,"No packets this session",NOVA_TEXT,false);lm_native_label(l->notice,"History clears on exit",NOVA_CAP,false);}
  for(unsigned row=0;row<l->history_rows;row++){lm_message*v=lm_at(&model,history_page*l->history_rows+row);if(!v)break;time_text(v,clock);snprintf(line,sizeof(line),"%s %s",v->status==LM_IN?"IN":v->status==LM_OUT_SENT?"TX":v->status==LM_OUT_UNKNOWN?"?":"...",clock);lm_native_row(l->history[row],v->text,line,false);}
  lm_native_button(l->footer[0],"NEWER",false);lm_native_button(l->footer[1],"OLDER",false);
 }else if(page==PAGE_RADIO){
  for(unsigned choice=1;choice<=4;choice++){bool available=!!(picker_mask&(1u<<(choice-1)));lm_native_row(l->picker[choice-1],radio_names[choice],!available?"UNAVAILABLE":model.choice==choice?(profile_save_state==2?"SELECTED / UNSAVED":profile_save_state==1?"SELECTED / SESSION":"SELECTED"):"SELECT",available&&model.choice==choice);}
  lm_native_label(l->counter,notice?notice:"Match radio and antenna",NOVA_CAP,false);
 }else if(page==PAGE_RF_ERROR){
  const char*help="Set all six RF values. Match radio, antenna, peers and local power limits. Radio stays off.";
  lm_native_rect area={l->body.x,l->state.y,l->body.w,l->full_footer.y-l->state.y-4};
  lm_native_paragraph(area,help,(unsigned)area.h/l->line_height);
  lm_native_button(l->full_footer,"BACK TO RF SETTINGS",false);
 }else if(page==PAGE_KEYBOARD){
#ifdef PORTABLE_LORA_SHARED_TEXT_INPUT
  lm_native_bytes(l->body,entry,(unsigned)strlen(entry),0,l->compose_rows);
  lm_native_rect help={l->body.x,l->body.y+l->body.h,l->body.w,2*l->line_height};const char*help_text=notice?notice:"Text input unavailable. Retry.";lm_native_paragraph(help,help_text,2);
  lm_native_button(l->full_footer,"EDIT",false);
#else
  size_t n=strlen(entry);unsigned columns=lm_native_columns();lm_native_label(l->entry,n>columns?entry+n-columns:entry,NOVA_WHITE,false);
  for(unsigned k=0;k<PWK_COUNT;k++){char label[2]={0};const char*s;if(k<PWK_CHARACTERS){label[0]=(char)portable_watch_key_character(key_page,k);s=label[0]==' '?"SP":label;}else s=k==PWK_PAGE?"PAGE":k==PWK_DELETE?"DEL":"DONE";lm_native_button(l->keys[k],s,k==key_choice);}
  snprintf(line,sizeof(line),"%u / %u  ASCII raw text",(unsigned)n,edit_field<6?11:LM_TEXT_MAX);lm_native_label(l->counter,notice?notice:line,NOVA_CAP,false);
#endif
 }else{
  time_text(&detail,clock);lm_native_label(l->state,clock,NOVA_CAP,false);lm_native_label(l->detail_state,detail.binary?"RECEIVED / BINARY":message_state(&detail),NOVA_CYAN,false);
  lm_native_rect body={l->body.x,l->detail_state.y+l->line_height,l->body.w,(int)l->detail_rows*l->line_height};lm_native_bytes(body,detail.text,detail.length,detail_offset,l->detail_rows);
  lm_native_rect info=l->detail_info;info.h=l->line_height;
  if(detail.status==LM_IN)snprintf(line,sizeof(line),"%uB  RSSI %d",detail.length,detail.rssi);else snprintf(line,sizeof(line),"%u bytes",detail.length);
  lm_native_label(info,line,NOVA_CAP,false);info.y+=l->line_height;
  if(detail.status==LM_IN)snprintf(line,sizeof(line),"SNR %s%u.%02u dB",detail.snr<0?"-":"",(unsigned)(detail.snr<0?-detail.snr:detail.snr)/4,(unsigned)(detail.snr<0?-detail.snr:detail.snr)%4*25);else snprintf(line,sizeof(line),"No delivery receipt");
  lm_native_label(info,line,NOVA_CAP,false);lm_native_button(l->footer[0],"PREVIOUS",false);lm_native_button(l->footer[1],"NEXT",false);
 }
 portable_lora_native_present(false);dirty=false;
}
/* Physical press and release must belong to one rendered action. A slide or
 * screen-generation change cancels it, even if the adapter returns a tap. */
static int lm_native_target(int x,int y){
 lm_native_layout*l=&lm_geometry;if(lm_native_hit(l->back,x,y))return 100;
 if(page==PAGE_HOME){for(int i=0;i<4;i++)if(lm_native_hit(l->home[i],x,y))return i;}
 else if(page==PAGE_RADIO){for(int i=0;i<4;i++)if(lm_native_hit(l->picker[i],x,y))return i;}
 else if(page==PAGE_RF){if(lm_native_hit(l->radio,x,y))return 101;for(int i=0;i<3;i++)if(lm_native_hit(l->rf[i],x,y))return 10+i;}
 else if(page==PAGE_HISTORY){for(unsigned i=0;i<l->history_rows;i++)if(lm_native_hit(l->history[i],x,y))return 10+(int)i;}
 else if(page==PAGE_COMPOSE&&lm_native_hit(l->body,x,y))return 10;
 else if(page==PAGE_KEYBOARD){
#ifndef PORTABLE_LORA_SHARED_TEXT_INPUT
  for(int i=0;i<35;i++)if(lm_native_hit(l->keys[i],x,y))return i;
  return -1;
#endif
 }
 if(page==PAGE_RF_ERROR||page==PAGE_KEYBOARD){if(lm_native_hit(l->full_footer,x,y))return 20;}
 else if(page!=PAGE_HOME&&page!=PAGE_RADIO){for(int i=0;i<2;i++)if(lm_native_hit(l->footer[i],x,y))return 20+i;}
 return -1;
}
static int lm_native_pressed;
static bool lm_native_contact,lm_native_buttons_armed;
static uint32_t lm_native_previous_buttons;
static uint64_t lm_native_input_generation;
static void lm_native_filter(t5_app_input_t*in){
 uint64_t generation=0;bool ready=portable_lora_native_ready(&generation,NULL);
 uint32_t buttons=in->buttons;if(!buttons)lm_native_buttons_armed=true;in->buttons=lm_native_buttons_armed?(buttons&~lm_native_previous_buttons):0;lm_native_previous_buttons=buttons;
 if(generation!=lm_native_input_generation){lm_native_input_generation=generation;lm_native_contact=false;lm_native_pressed=-1;}
 t5_app_contact_t c={0};bool ok=app->touch_contact&&app->touch_contact(&c);
 if(!ready)in->buttons&=~(T5_APP_BUTTON_CONFIRM|T5_APP_BUTTON_LEFT|T5_APP_BUTTON_RIGHT);
 if(!ready||!ok){lm_native_contact=false;lm_native_pressed=-1;in->tapped=false;return;}
 if(c.down){int target=lm_native_target(c.x,c.y);if(!lm_native_contact){lm_native_contact=true;lm_native_pressed=target;}else if(target!=lm_native_pressed)lm_native_pressed=-1;}
 if(in->tapped){int target=lm_native_target(in->touch_x,in->touch_y);uint16_t ox=0,oy=0;bool origin=portable_lora_native_tap_origin(&ox,&oy);if(target<0||!origin||lm_native_target(ox,oy)!=target||(lm_native_contact&&target!=lm_native_pressed))in->tapped=false;lm_native_contact=false;lm_native_pressed=-1;}
 else if(!c.down){lm_native_contact=false;lm_native_pressed=-1;}
}
