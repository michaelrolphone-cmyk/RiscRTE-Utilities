#include "CanvasApp.h"
#include "RiscRuntimeV1.h"
#include "RiscDisplayOutputV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscStorageVolumeV1.h"
#include "RiscAppDataV1.h"
#include "T5AppApi.h"
#include "T5VideoApi.h"
#include "T5StorageApi.h"
#include "T5FileOpenApi.h"
#ifdef CANVAS_FILE_SOURCE
#include "RiscFileOpenSourceV1.h"
#endif
#include "T5ProviderCapabilityApi.h"
#include "T5MathApi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define HAS(p,t,f) ((p)&&(p)->struct_size>=offsetof(t,f)+sizeof((p)->f)&&(p)->f)
#define MAX_GRANTS 12u
static const risc_runtime_api_v1 *rt;
static risc_runtime_capability_v1 grants[MAX_GRANTS];
static const risc_display_output_api_v1 *display;
static const risc_touch_api_v1 *touch;
static const risc_input_navigation_api_v1 *navigation;
static const risc_storage_volume_api_v1 *volume;
static const risc_app_data_v1 *data;
static const t5_file_open_api_v1 *files;
#ifdef CANVAS_FILE_SOURCE
static const risc_file_open_source_api_v1 *source;
static risc_file_open_source_info_v1 source_info;
static bool source_stream;
#endif
static risc_display_info_v1 info;
static risc_display_present_token_v1 tokens[4];
static uint32_t token_at[4],frames;
static risc_display_surface_v1 borrowed;
static uint8_t *working,*snapshot,*model_bytes;
static unsigned kind,width,height,stride,format,logical_out_w,logical_out_h;
static unsigned out_x,out_y,out_w,out_h,selected,model_size,model_offset;
static uint64_t subscription,last_sequence;
static bool retained,exiting,dirty,started,controls,back_exits,rotated,blocked,claimed;
static risc_touch_snapshot_v1 contacts,event_contacts,touch_queue[64];
static unsigned touch_head,touch_count;
static uint32_t buttons,game_buttons;
static risc_storage_file_t stream;
static char stream_path[512],selected_path[512];
static uint64_t stream_size,stream_offset;
static uint32_t clock_last;

static uint32_t now(void){
 uint32_t ms=clock_last;
 if(rt && !retained){
  if(rt->struct_size>=RISC_RUNTIME_MONOTONIC_V1_SIZE && rt->monotonic_ms){if(!rt->monotonic_ms(&ms))exiting=true;}
  else{risc_runtime_health_v1 h={.struct_size=sizeof(h)};if(rt->health(&h))ms=h.uptime_ms;else exiting=true;}
 }
 clock_last=ms;return ms;
}
bool canvas_alive(void){return rt && !retained && !exiting;}
void canvas_retain(const char *reason){
 if(retained)return;
 if(rt && rt->diagnostic)(void)rt->diagnostic(reason);
 retained=exiting=true;
 if(rt && rt->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE && rt->retain_invocation)(void)rt->retain_invocation();
}
static bool acquire(const char *name,uint32_t version,uint32_t *lease,const void **api){
 *lease=0;*api=NULL;if(!rt||retained)return false;
 for(unsigned i=0;i<MAX_GRANTS;i++)if(!grants[i].api){
  grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(grants[i])};
  if(!rt->acquire(name,version,0,&grants[i]))return false;
  *lease=i+1;*api=grants[i].api;return *api!=NULL;
 }return false;
}
static bool release(uint32_t lease){
 if(!lease)return true;if(retained||!rt||lease>MAX_GRANTS)return false;
 if(!rt->release(&grants[lease-1])){canvas_retain("Canvas: capability release retained");return false;}
 memset(&grants[lease-1],0,sizeof(grants[0]));return true;
}
static const void *get(const char *name,uint32_t version){uint32_t n;const void *p=NULL;(void)acquire(name,version,&n,&p);return p;}
static void memory_free(void *p){if(!retained)free(p);}
static void *memory_alloc(size_t n){return retained?NULL:malloc(n);}
unsigned canvas_width(void){return width;}
unsigned canvas_height(void){return height;}
bool canvas_compact(void){return kind==CANVAS_VIEWER&&width<=320;}
void canvas_pixel(uint8_t *p,int x,int y,bool black){
 if(!p||x<0||y<0||(unsigned)x>=width||(unsigned)y>=height)return;
 size_t at=(size_t)y*stride+(unsigned)x/8;uint8_t mask=0x80u>>(x&7);
 if(black)p[at]|=mask;else p[at]&=(uint8_t)~mask;
}
static bool statuses(void){
 if(retained)return false;
 for(unsigned i=0;i<4;i++)if(tokens[i]){
  risc_display_present_status_v1 s={0};
  if(!display->present_status(display->context,tokens[i],&s)){canvas_retain("Canvas: presentation status unknown");return false;}
  if(s.state==RISC_DISPLAY_PRESENT_COMPLETE||s.state==RISC_DISPLAY_PRESENT_SUPERSEDED){tokens[i]=0;++frames;}
  else if(s.state==RISC_DISPLAY_PRESENT_FAILED || (s.state!=RISC_DISPLAY_PRESENT_ACTIVE&&s.state!=RISC_DISPLAY_PRESENT_QUEUED) || (uint32_t)(now()-token_at[i])>5000){canvas_retain("Canvas: presentation failed or timed out");return false;}
 }
 return true;
}
static void put_output(unsigned x,unsigned y,bool black){
 unsigned px=x,py=y;
 if(rotated){if(kind==CANVAS_VIEWER){px=y;py=info.height-1-x;}else{px=info.width-1-y;py=x;}}
 if(px>=borrowed.width||py>=borrowed.height)return;
 uint8_t *row=(uint8_t*)borrowed.pixels+(size_t)py*borrowed.stride_bytes;
 if(format==RISC_DISPLAY_FORMAT_RGB565){uint16_t v=black?0:0xffff;memcpy(row+px*2,&v,2);}
 else{uint8_t m=0x80u>>(px&7);if(black)row[px/8]|=m;else row[px/8]&=(uint8_t)~m;}
}
/* Shared hit rectangles also draw the game controls. IDs are app intents. */
typedef struct{unsigned x,y,w,h;uint32_t action;const char *label;} control;
static control control_at(unsigned i){
 unsigned w=logical_out_w/10,h=logical_out_h/7;
 if(w<44)w=44;if(h<44)h=44;
 const uint32_t acts[]={1,2,512,1024,4,32,8,128};
 const char *names[]={"<",">","^","v","JUMP","USE","PAUSE","NOTES"};
 unsigned x=0,y=logical_out_h-h-4;
 if(i<2)x=4+i*(w+4);
 else if(i<4){x=4+(i-2)*(w+4);y-=h+4;}
 else if(i<6)x=logical_out_w-(6-i)*(w+4);
 else{x=logical_out_w-(8-i)*(w+4);y=4;}
 return(control){x,y,w,h,acts[i],names[i]};
}
/* Small immutable bitmap alphabet for non-scene canvas controls. */
static const uint8_t letters[26][5]={
 {126,9,9,9,126},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,34,28},{127,73,73,73,65},{127,9,9,9,1},{62,65,73,73,122},{127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},{127,2,12,2,127},{127,4,8,16,127},{62,65,65,65,62},{127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{1,1,127,1,1},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},{99,20,8,20,99},{3,4,120,4,3},{97,81,73,69,67}};
static void label(control c){
 unsigned scale=2,len=(unsigned)strlen(c.label),x=c.x+(c.w-len*6*scale)/2,y=c.y+(c.h-7*scale)/2;
 for(unsigned i=0;i<len;i++)for(unsigned cx=0;cx<5;cx++)for(unsigned cy=0;cy<7;cy++){
  char ch=c.label[i];bool on=false;
  if(ch>='A'&&ch<='Z')on=(letters[ch-'A'][cx]>>cy)&1;
  else if(ch=='<'||ch=='>')on=(int)cx==(ch=='<'?(cy<4?3-(int)cy:(int)cy-3):(cy<4?(int)cy:6-(int)cy));
  else if(ch=='^'||ch=='v')on=(int)cy==(ch=='^'?(cx<3?3-(int)cx:(int)cx-1):(cx<3?3+(int)cx:7-(int)cx));
  if(on)for(unsigned a=0;a<scale;a++)for(unsigned b=0;b<scale;b++)put_output(x+i*6*scale+cx*scale+a,y+cy*scale+b,true);
 }
}
static void pump(void){
 if(!started||retained||!statuses()||!dirty)return;
 unsigned slot=0;while(slot<4&&tokens[slot])slot++;if(slot==4)return;
 if(!(info.flags&RISC_DISPLAY_INFO_MAILBOX))for(unsigned i=0;i<4;i++)if(tokens[i])return;
 memset(&borrowed,0,sizeof(borrowed));
 if(!display->acquire(display->context,format,&borrowed))return;
 unsigned min_stride=(info.width*(format==RISC_DISPLAY_FORMAT_RGB565?16:1)+7)/8;
 if(!borrowed.frame||!borrowed.pixels||borrowed.pixel_format!=format||borrowed.width!=info.width||borrowed.height!=info.height||borrowed.stride_bytes<min_stride||borrowed.stride_bytes>borrowed.size_bytes/info.height){canvas_retain("Canvas: invalid borrowed surface");return;}
 /* Hollow Trail keeps its 16:9 image; unused panel space is black. */
 bool black_background=kind==CANVAS_GAME;
 unsigned background=(format==RISC_DISPLAY_FORMAT_RGB565)?(black_background?0:255):(black_background?255:0);
 memset(borrowed.pixels,background,(size_t)borrowed.stride_bytes*borrowed.height);
 for(unsigned y=0;y<out_h;y++){
  unsigned sy=y*height/out_h;
  for(unsigned x=0;x<out_w;x++){unsigned sx=x*width/out_w;bool black=(snapshot[(size_t)sy*stride+sx/8]&(0x80u>>(sx&7)))!=0;put_output(x+out_x,y+out_y,black);}
  if((y&15u)==15u){rt->yield_ms(1);if(!risc_runtime_get_api(1)){canvas_retain("Canvas: authority lost during raster copy");return;}}
 }
 if(kind==CANVAS_GAME&&controls)for(unsigned i=0;i<8;i++){
  control c=control_at(i);for(unsigned y=c.y;y<c.y+c.h;y++)for(unsigned x=c.x;x<c.x+c.w;x++)put_output(x,y,x==c.x||y==c.y||x==c.x+c.w-1||y==c.y+c.h-1);label(c);
 }
 risc_display_present_options_v1 o={RISC_DISPLAY_PRESENT_LOW_LATENCY,(info.flags&RISC_DISPLAY_INFO_MAILBOX)?RISC_DISPLAY_QUEUE_MAILBOX:RISC_DISPLAY_QUEUE_FIFO,0};
 risc_display_present_token_v1 token=0;
 if(!display->submit(display->context,borrowed.frame,NULL,0,&o,&token)){display->release(display->context,borrowed.frame);memset(&borrowed,0,sizeof(borrowed));return;}
 memset(&borrowed,0,sizeof(borrowed));if(!token){canvas_retain("Canvas: missing presentation token");return;}
 tokens[slot]=token;token_at[slot]=now();dirty=false;
}
static bool map_contact(risc_touch_contact_v1 *c,unsigned w,unsigned h){
 if(!w||!h||c->x>=w||c->y>=h)return false;
 unsigned x,y;
 if(kind==CANVAS_GAME&&w<h){x=(unsigned)c->y*logical_out_w/h;y=logical_out_h-1-(unsigned)c->x*logical_out_h/w;}
 else if(kind==CANVAS_VIEWER&&w>h){x=logical_out_w-1-(unsigned)c->y*logical_out_w/h;y=(unsigned)c->x*logical_out_h/w;}
 else{x=(unsigned)c->x*logical_out_w/w;y=(unsigned)c->y*logical_out_h/h;}
 if(kind==CANVAS_GAME&&controls)for(unsigned n=0;n<8;n++){control b=control_at(n);if(x>=b.x&&x<b.x+b.w&&y>=b.y&&y<b.y+b.h)game_buttons|=b.action;}
 c->x=(uint16_t)(x<out_x?0:(x-out_x)*width/out_w);c->y=(uint16_t)(y<out_y?0:(y-out_y)*height/out_h);return true;
}
static void enqueue_contacts(void){
 if(kind!=CANVAS_VIEWER)return;
 if(touch_count==64){blocked=true;touch_head=touch_count=0;return;}
 touch_queue[(touch_head+touch_count++)%64]=event_contacts;
}
static void touch_poll(void){
 game_buttons=0;bool ok=touch->poll(touch->context,16),drained=false;
 risc_touch_event_v1 events[RISC_TOUCH_QUEUE_LENGTH];unsigned count=0;
 for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH;i++){
  risc_touch_event_v1 e={0};int32_t rc=touch->next(touch->context,subscription,&e);
  if(!rc){drained=true;break;}if(rc!=1){ok=false;break;}
  if(last_sequence&&e.sequence!=last_sequence+1)ok=false;last_sequence=e.sequence;events[count++]=e;
 }
 risc_touch_snapshot_v1 s={0};if(!touch->snapshot(touch->context,&s)||!s.width||!s.height||s.contact_count>RISC_TOUCH_MAX_CONTACTS)ok=false;
 if(s.buttons&RISC_TOUCH_BUTTON_PRIMARY)exiting=true;
 if(!ok||!drained)blocked=true;
 if(blocked){contacts=(risc_touch_snapshot_v1){0};event_contacts=contacts;touch_head=touch_count=0;if(ok&&drained&&!s.contact_count&&!s.buttons)blocked=false;return;}
 for(unsigned n=0;n<count;n++){
  risc_touch_event_v1 e=events[n];
  if(e.kind==RISC_TOUCH_EVENT_BUTTON_DOWN&&e.id==0){exiting=true;continue;}
  if(e.kind>=RISC_TOUCH_EVENT_BUTTON_DOWN)continue;
  unsigned at=0;while(at<event_contacts.contact_count&&event_contacts.contacts[at].id!=e.id)at++;
  if(e.kind==RISC_TOUCH_EVENT_DOWN){if(at<event_contacts.contact_count||at==RISC_TOUCH_MAX_CONTACTS){blocked=true;break;}event_contacts.contact_count++;}
  else if(at==event_contacts.contact_count){blocked=true;break;}
  if(e.kind==RISC_TOUCH_EVENT_UP){for(unsigned i=at+1;i<event_contacts.contact_count;i++)event_contacts.contacts[i-1]=event_contacts.contacts[i];event_contacts.contact_count--;}
  else if(e.kind==RISC_TOUCH_EVENT_DOWN||e.kind==RISC_TOUCH_EVENT_MOVE){event_contacts.contacts[at]=(risc_touch_contact_v1){.id=e.id,.x=e.x,.y=e.y};if(!map_contact(&event_contacts.contacts[at],s.width,s.height)){blocked=true;break;}}
  else{blocked=true;break;}
  event_contacts.width=width;event_contacts.height=height;event_contacts.timestamp_ms=e.timestamp_ms;enqueue_contacts();
 }
 contacts=s;contacts.width=width;contacts.height=height;
 for(unsigned i=0;i<s.contact_count;i++)if(!map_contact(&contacts.contacts[i],s.width,s.height))blocked=true;
 if(blocked){contacts.contact_count=0;event_contacts=contacts;game_buttons=0;touch_head=touch_count=0;}
}
bool canvas_contacts(risc_touch_snapshot_v1 *out){
 if(!out)return false;
 if(blocked||retained){memset(out,0,sizeof(*out));return false;}
 if(touch_count){*out=touch_queue[touch_head];touch_head=(touch_head+1)%64;touch_count--;}
 else *out=contacts;return true;
}
uint32_t canvas_game_buttons(void){return game_buttons;}
void canvas_set_controls(bool on){controls=on;}
static bool poll_nowait(t5_app_input_t *in){
 memset(in,0,sizeof(*in));if(!canvas_alive()){in->exit_requested=true;return false;}
 buttons=0;
 if(navigation){risc_input_navigation_frame_v1 n={0};if(!navigation->poll(navigation->context,&n)){exiting=true;return false;}buttons=n.buttons|n.pressed;if(buttons&RISC_NAV_HOME)exiting=true;if(buttons&RISC_NAV_PAGE_BACK)buttons|=RISC_NAV_LEFT;if(buttons&RISC_NAV_PAGE_FORWARD)buttons|=RISC_NAV_RIGHT;}
 touch_poll();in->buttons=buttons&63;in->exit_requested=exiting||(back_exits&&(buttons&RISC_NAV_BACK));pump();return !retained;
}
static bool poll_wait(t5_app_input_t *in,uint32_t ms){if(rt&&!retained)rt->yield_ms(ms?ms:1);return poll_nowait(in);}
static void set_back(bool b){back_exits=b;}
static void log_message(const char *s){if(rt&&!retained&&rt->diagnostic)(void)rt->diagnostic(s);}
static const t5_app_api_v1 app_api={.abi_version=1,.struct_size=sizeof(app_api),.poll=poll_wait,.millis=now,.set_back_exits_app=set_back,.psram_alloc=memory_alloc,.psram_free=memory_free,.poll_nowait=poll_nowait,.log_message=log_message};
const t5_app_api_v1 *t5_app_get_api(uint32_t v){return v==1?&app_api:NULL;}
const t5_provider_capability_api_v1 *t5_provider_capability_get_api(uint32_t v){static const t5_provider_capability_api_v1 a={1,sizeof(a),acquire,release,NULL};return v==1?&a:NULL;}
const t5_math_api_v1 *t5_math_get_api(uint32_t v){(void)v;return NULL;/* Original scalar paths, no fabricated SIMD capability. */}
static bool video_start(t5_video_surface_v1 *s,uint8_t f){if(!started||f!=1)return false;*s=(t5_video_surface_v1){width,height,stride,1,1};return true;}
static bool video_default(t5_video_surface_v1 *s){return video_start(s,1);}
static uint8_t *backbuffer(size_t *size){if(size)*size=started?(size_t)stride*height:0;return retained?NULL:working;}
static bool can_submit(void){return started&&!retained&&!exiting;}
static bool submit(uint16_t y,uint16_t h){(void)y;(void)h;if(!can_submit())return false;memcpy(snapshot,working,(size_t)stride*height);dirty=true;pump();return !retained;}
static bool pending(void){if(!retained)pump();for(unsigned i=0;i<4;i++)if(tokens[i])return true;return dirty;}
static uint32_t frame_counter(void){return frames;}
static void video_stop(void){/* canvas_close owns the single display lifetime. */}
const t5_video_api_v1 *t5_video_get_api(uint32_t v){static const t5_video_api_v1 a={.api_version=1,.struct_size=sizeof(a),.start=video_default,.backbuffer=backbuffer,.can_submit=can_submit,.submit=submit,.pending=pending,.frame_counter=frame_counter,.stop=video_stop,.start_format=video_start};return v==1?&a:NULL;}

static const char cube[]="v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\nf 1 4 3 2\nf 5 6 7 8\nf 1 2 6 5\nf 4 8 7 3\nf 1 5 8 4\nf 2 3 7 6\n";
static const char *model_names[]={"CUBE","SAVED OBJ","SAVED STL"};
unsigned canvas_model_count(void){return 3;}
const char *canvas_model_name(unsigned i){return i<3?model_names[i]:"";}
void canvas_model_select(unsigned i){selected=i<3?i:0;selected_path[0]=0;}
bool canvas_model_source(char *out,size_t cap){
 if(selected_path[0])snprintf(out,cap,"%s",selected_path);
 else snprintf(out,cap,"%s",selected==0?"builtin.obj":selected==1?"model.obj":"model.stl");return true;
}
static bool source_path_get(char *out,size_t cap){return canvas_model_source(out,cap);}
const t5_file_open_api_v1 *t5_file_open_get_api(uint32_t v){static const t5_file_open_api_v1 a={.api_version=1,.struct_size=sizeof(a),.source_path_get=source_path_get};return v==1?&a:NULL;}
static void stream_close(uint32_t h){
 if(!h||retained)return;
 if(stream&&!volume->file_close(volume->context,stream,false)){canvas_retain("Canvas: model file close retained");return;}stream=0;
#ifdef CANVAS_FILE_SOURCE
 source_stream=false;
#endif
 free(model_bytes);model_bytes=NULL;model_size=model_offset=0;
}
static uint32_t stream_open(const char *path,size_t *size){
 *size=0;stream_close(1);if(retained)return 0;stream_size=stream_offset=0;
#ifdef CANVAS_FILE_SOURCE
 /* The basename chooses format only. All selected bytes are read with the
  * receiver token; neither a mount path nor caller-owned handle is accepted. */
 if(source_info.token&&selected_path[0]&&!strcmp(path,selected_path)){
  if(source_info.size>32u*1024u*1024u)return 0;
  source_stream=true;stream_size=source_info.size;*size=(size_t)stream_size;return 1;
 }
#endif
 if(!strcmp(path,"builtin.obj")){model_size=sizeof(cube)-1;model_bytes=malloc(model_size);if(!model_bytes)return 0;memcpy(model_bytes,cube,model_size);*size=model_size;return 1;}
 if(path[0]!='/'&&data){
  uint32_t n=0;uint64_t rev=0,read_rev=0;int32_t rc=data->stat(data->context,path,&n,&rev);
  if(rc==RISC_APP_DATA_RETAINED){canvas_retain("Canvas: model storage retained");return 0;}
  if(rc!=RISC_APP_DATA_OK||!n||n>RISC_APP_DATA_FILE_MAX)return 0;
  model_bytes=malloc(n);if(!model_bytes)return 0;uint32_t got=0;
  rc=data->read(data->context,path,rev,model_bytes,n,&got,&read_rev);
  if(rc==RISC_APP_DATA_RETAINED){canvas_retain("Canvas: model read retained");return 0;}
  if(rc!=RISC_APP_DATA_OK||got!=n||read_rev!=rev){free(model_bytes);model_bytes=NULL;return 0;}
  model_size=n;*size=n;return 1;
 }
 if(!volume||!volume->refresh(volume->context)||!volume->ready(volume->context))return 0;
 if(strncmp(path,"/sd/",4)||strstr(path,"/../")||strlen(path)>=sizeof(stream_path))return 0;
 strcpy(stream_path,path+3);stream=volume->file_open_read(volume->context,stream_path,&stream_size);
 if(!stream||stream_size>32u*1024u*1024u){stream_close(1);return 0;}*size=(size_t)stream_size;return 1;
}
static size_t stream_read(uint32_t h,void *out,size_t n){
 if(!h||retained||exiting)return 0;
#ifdef CANVAS_FILE_SOURCE
 if(source_stream){
  if(n>RISC_FILE_OPEN_SOURCE_IO_MAX)n=RISC_FILE_OPEN_SOURCE_IO_MAX;
  if(n>stream_size-stream_offset)n=(size_t)(stream_size-stream_offset);
  uint32_t got=0;int32_t rc=source->read_at(source_info.token,stream_offset,out,(uint32_t)n,&got);
  if(rc==RISC_FILE_SOURCE_RETAINED){canvas_retain("Canvas: selected source retained");return 0;}
  if(rc!=RISC_FILE_SOURCE_OK||got>n){exiting=true;return 0;}
  stream_offset+=got;return got;
 }
#endif
 if(model_bytes){if(n>model_size-model_offset)n=model_size-model_offset;memcpy(out,model_bytes+model_offset,n);model_offset+=(unsigned)n;return n;}
 if(!stream)return 0;if(n>RISC_STORAGE_VOLUME_IO_MAX)n=RISC_STORAGE_VOLUME_IO_MAX;size_t got=volume->file_read(volume->context,stream,out,n);if(got>n){exiting=true;return 0;}stream_offset+=got;return got;
}
static bool stream_seek(uint32_t h,size_t offset){
 if(!h||retained)return false;
#ifdef CANVAS_FILE_SOURCE
 if(source_stream){if(offset>stream_size||exiting)return false;stream_offset=offset;return true;}
#endif
 if(model_bytes){if(offset>model_size)return false;model_offset=(unsigned)offset;return true;}
 if(!stream||offset>stream_size||exiting)return false;
 if(volume->struct_size>=sizeof(risc_storage_volume_api_v1_ext)){
  const risc_storage_volume_api_v1_ext *e=(const void*)volume;if(e->file_seek){bool ok=e->file_seek(volume->context,stream,offset);if(ok)stream_offset=offset;return ok;}
 }
 if(offset<stream_offset){if(!volume->file_close(volume->context,stream,false)){canvas_retain("Canvas: seek close retained");return false;}stream=volume->file_open_read(volume->context,stream_path,&stream_size);stream_offset=0;if(!stream)return false;}
 uint8_t skip[768];while(stream_offset<offset){size_t n=offset-stream_offset;if(n>sizeof(skip))n=sizeof(skip);if(stream_read(h,skip,n)!=n)return false;rt->yield_ms(1);if(exiting||retained)return false;}return true;
}
const t5_storage_api_v1 *t5_storage_get_api(uint32_t v){static const t5_storage_api_v1 a={.api_version=1,.struct_size=sizeof(a),.stream_open=stream_open,.stream_read=stream_read,.stream_seek=stream_seek,.stream_close=stream_close};return v==1?&a:NULL;}

bool canvas_open(unsigned app_kind){
 rt=risc_runtime_get_api(1);if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release||!rt->health||!rt->yield_ms)return false;
 kind=app_kind;retained=exiting=dirty=started=claimed=false;blocked=true;back_exits=kind==CANVAS_VIEWER;controls=kind==CANVAS_GAME;frames=0;last_sequence=subscription=0;touch_head=touch_count=0;event_contacts=(risc_touch_snapshot_v1){0};contacts=event_contacts;
 memset(grants,0,sizeof(grants));memset(tokens,0,sizeof(tokens));
 display=get("display.output",1);touch=get("input.touch.raw",1);
 if(!HAS(display,risc_display_output_api_v1,present_status)||!display->get_info||!display->acquire||!display->submit||!display->release||!display->get_info(display->context,&info))goto bad;
 if(!HAS(touch,risc_touch_api_v1,snapshot)||!touch->poll||!touch->next||!touch->subscribe||!touch->unsubscribe)goto bad;
 if(info.width<160||info.height<160||info.width>1200||info.height>1200)goto bad;
 format=(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565))?RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1;
 if(!(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(format)))goto bad;
 /* Native X4 surfaces may be landscape; the viewer stays portrait. The game stays landscape. */
 rotated=kind==CANVAS_GAME?info.width<info.height:info.width>info.height;
 logical_out_w=rotated?info.height:info.width;logical_out_h=rotated?info.width:info.height;
 width=kind==CANVAS_GAME?960:logical_out_w;height=kind==CANVAS_GAME?540:logical_out_h;stride=(width+7)/8;
 out_w=logical_out_w;out_h=out_w*height/width;if(out_h>logical_out_h){out_h=logical_out_h;out_w=out_h*width/height;}out_x=(logical_out_w-out_w)/2;out_y=(logical_out_h-out_h)/2;
 working=calloc(stride,height);snapshot=calloc(stride,height);if(!working||!snapshot)goto bad;
 subscription=touch->subscribe(touch->context);if(!subscription)goto bad;
 navigation=get("input.navigation",1);
 if(navigation){if(!HAS(navigation,risc_input_navigation_api_v1,reset)||!navigation->poll||!navigation->foreground)goto bad;risc_input_foreground_v1 claim={"input.touch.raw",1};if(!navigation->foreground(navigation->context,&claim,1))goto bad;claimed=true;if(!navigation->reset(navigation->context))goto bad;}
 if(kind==CANVAS_VIEWER){
  volume=get("storage.volume",1);data=get("storage.app-data",1);files=get("file.open",1);
  if(volume&&(!HAS(volume,risc_storage_volume_api_v1,file_close)||!volume->file_open_read||!volume->file_read||!volume->refresh||!volume->ready))goto bad;
  if(data&&(!HAS(data,risc_app_data_v1,read)||!data->stat))goto bad;
#ifdef CANVAS_FILE_SOURCE
  source=get(RISC_FILE_OPEN_SOURCE_CAPABILITY,1);source_stream=false;
  source_info=(risc_file_open_source_info_v1){.struct_size=sizeof(source_info)};
  if(source){
   if(source->api_version!=1||!HAS(source,risc_file_open_source_api_v1,read_at)||!source->source_info)goto bad;
   int32_t rc=source->source_info(&source_info);
   if(rc==RISC_FILE_SOURCE_RETAINED){canvas_retain("Canvas: selected source retained");return false;}
   if(rc!=RISC_FILE_SOURCE_OK&&rc!=RISC_FILE_SOURCE_CONTEXT)goto bad;
   if(rc==RISC_FILE_SOURCE_CONTEXT)source_info.token=0;
   else if(!source_info.token||!source_info.name[0]||!memchr(source_info.name,0,sizeof(source_info.name))||strchr(source_info.name,'/')||strchr(source_info.name,'\\'))goto bad;
  }
#endif
  selected=0;selected_path[0]=0;if(files&&HAS(files,t5_file_open_api_v1,source_path_get))(void)files->source_path_get(selected_path,sizeof(selected_path));
#ifdef CANVAS_FILE_SOURCE
  if(source_info.token)snprintf(selected_path,sizeof(selected_path),"%s",source_info.name);
#endif
 }
 started=true;return true;
 bad:canvas_close();return false;
}
void canvas_close(void){
 if(retained)return;
 dirty=false;uint32_t at=now();
 while(started&&pending()&&!retained){if((uint32_t)(now()-at)>5100){canvas_retain("Canvas: close presentation timeout");break;}rt->yield_ms(1);}
 if(retained)return;
 stream_close(1);if(retained)return;
 if(touch&&subscription&&!touch->unsubscribe(touch->context,subscription)){canvas_retain("Canvas: touch unsubscribe retained");return;}subscription=0;
 if(claimed&&(!navigation->foreground(navigation->context,NULL,0)||!navigation->reset(navigation->context))){canvas_retain("Canvas: input claim cleanup retained");return;}claimed=false;
 for(unsigned i=MAX_GRANTS;i>0;i--)if(grants[i-1].api&&!release(i))return;
 free(working);free(snapshot);working=snapshot=NULL;started=false;
}
