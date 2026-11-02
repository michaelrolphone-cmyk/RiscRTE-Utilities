/* Viewer compatibility client over provider-owned ui.canvas@1.
 * Engine pixels, model data and controller policy remain application-owned.
 * No physical display format, rotation, touch transport or product selection. */
#include "CanvasApp.h"
#include "RiscRuntimeV1.h"
#include "RiscCanvasV1.h"
#include "RiscFileOpenSourceV1.h"
#include "RiscStorageVolumeV1.h"
#include "RiscAppDataV1.h"
#include "T5AppApi.h"
#include "T5VideoApi.h"
#include "T5StorageApi.h"
#include "T5FileOpenApi.h"
#include "T5ProviderCapabilityApi.h"
#include "T5MathApi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define HAS(p,t,f) ((p)&&(p)->struct_size>=offsetof(t,f)+sizeof((p)->f)&&(p)->f)
#define MAX_GRANTS 12u
static const risc_runtime_api_v1 *rt;
static risc_runtime_capability_v1 grants[MAX_GRANTS];
static const risc_canvas_api_v1 *canvas;
static const risc_storage_volume_api_v1 *volume;
static const risc_app_data_v1 *data;
static const t5_file_open_api_v1 *files;
static const risc_file_open_source_api_v1 *source_files;
static risc_file_open_source_info_v1 selected_source;
static bool source_selected,source_stream;
static uint64_t session,receipt,last_sequence;
static risc_canvas_geometry_v1 geometry;
static uint32_t receipt_at,frames,clock_last,buttons;
static uint8_t *working,*snapshot,*model_bytes;
static unsigned width,height,stride,selected,model_size,model_offset;
static bool retained,exiting,dirty,started,blocked,back_exits;
static risc_touch_snapshot_v1 contacts,touch_queue[64];
static unsigned touch_head,touch_count;
static risc_storage_file_t stream;
static char stream_path[512],selected_path[512];
static uint64_t stream_size,stream_offset;
static uint32_t now(void){
 uint32_t ms=clock_last;
 if(rt&&!retained){
  if(HAS(rt,risc_runtime_api_v1,monotonic_ms)){if(!rt->monotonic_ms(&ms))exiting=true;}
  else{risc_runtime_health_v1 h={.struct_size=sizeof(h)};if(rt->health(&h))ms=h.uptime_ms;else exiting=true;}
 }
 clock_last=ms;return ms;
}
bool canvas_alive(void){return rt&&!retained&&!exiting;}
void canvas_retain(const char *reason){
 if(retained)return;
 if(rt&&rt->diagnostic)(void)rt->diagnostic(reason);
 retained=exiting=true;
 if(HAS(rt,risc_runtime_api_v1,retain_invocation))(void)rt->retain_invocation();
}
static bool result_ok(int32_t rc){
 if(rc==RISC_CANVAS_RETAINED)canvas_retain("Viewer: canvas custody retained");
 return rc==RISC_CANVAS_OK;
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
 if(!lease)return true;
 if(retained||!rt||lease>MAX_GRANTS)return false;
 if(!rt->release(&grants[lease-1])){canvas_retain("Viewer: capability release retained");return false;}
 memset(&grants[lease-1],0,sizeof(grants[0]));return true;
}
static const void *get(const char *name,uint32_t version){uint32_t n;const void *p=NULL;(void)acquire(name,version,&n,&p);return p;}
static void memory_free(void *p){if(!retained)free(p);}
static void *memory_alloc(size_t n){return retained?NULL:malloc(n);}
unsigned canvas_width(void){return width;}
unsigned canvas_height(void){return height;}
bool canvas_compact(void){return width<=320;}
void canvas_pixel(uint8_t *p,int x,int y,bool black){
 if(!p||x<0||y<0||(unsigned)x>=width||(unsigned)y>=height)return;
 size_t at=(size_t)y*stride+(unsigned)x/8;uint8_t mask=0x80u>>(x&7);
 if(black)p[at]|=mask;else p[at]&=(uint8_t)~mask;
}
uint32_t canvas_game_buttons(void){return 0;}
void canvas_set_controls(bool on){(void)on;}
static bool statuses(void){
 if(retained)return false;
 if(!receipt)return true;
 risc_canvas_receipt_v1 status={.struct_size=sizeof(status)};
 if(!result_ok(canvas->poll(canvas->context,session,receipt,&status)))return false;
 if(status.state==RISC_CANVAS_COMPLETE||status.state==RISC_CANVAS_SUPERSEDED||status.state==RISC_CANVAS_FAILED){
  if(!result_ok(canvas->forget_receipt(canvas->context,session,receipt)))return false;
  receipt=0;frames++;return true;
 }
 if(status.state!=RISC_CANVAS_QUEUED&&status.state!=RISC_CANVAS_PRESENTING){
  canvas_retain("Viewer: unknown canvas receipt");return false;
 }
 if((uint32_t)(now()-receipt_at)>5000){canvas_retain("Viewer: canvas presentation timeout");return false;}
 return true;
}
static void pump(void){
 if(!started||retained||!statuses()||receipt||!dirty)return;
 risc_canvas_frame_v1 f={.struct_size=sizeof(f)};
 int32_t rc=canvas->begin_frame(canvas->context,session,&f);
 if(!result_ok(rc))return;
 if(!f.lease||!f.pixels||f.geometry_revision!=geometry.revision||
    f.width!=width||f.height!=height||f.encoding!=RISC_CANVAS_MONO1||
    f.stride<stride||height>f.byte_count/f.stride){
  canvas_retain("Viewer: invalid logical canvas frame");return;
 }
 /* The retained engine uses one=black; the logical MONO1 ABI uses one=white.
  * This is a content encoding conversion, never a physical panel decision. */
 for(unsigned y=0;y<height;y++){
  uint8_t *row=(uint8_t*)f.pixels+(size_t)y*f.stride;
  for(unsigned x=0;x<stride;x++)row[x]=(uint8_t)~snapshot[(size_t)y*stride+x];
 }
 uint64_t token=0;rc=canvas->submit(canvas->context,session,f.lease,&token);
 if(result_ok(rc)){
  if(!token){canvas_retain("Viewer: missing canvas receipt");return;}
  receipt=token;receipt_at=now();dirty=false;
 }else if(!retained){
  if(!result_ok(canvas->cancel_frame(canvas->context,session,f.lease)))
   canvas_retain("Viewer: logical lease cleanup uncertain");
 }
}
static void queue_contacts(void){
 if(touch_count==64){blocked=true;touch_head=touch_count=0;return;}
 touch_queue[(touch_head+touch_count++)%64]=contacts;
}
static bool adopt_contact(const risc_canvas_event_v1 *e){
 if(e->contact>UINT8_MAX||e->x<0||e->y<0||(uint32_t)e->x>=width||(uint32_t)e->y>=height)return false;
 unsigned at=0;while(at<contacts.contact_count&&contacts.contacts[at].id!=e->contact)at++;
 if(e->phase==RISC_CANVAS_UP||e->phase==RISC_CANVAS_CANCEL){
  if(at==contacts.contact_count)return true;
  for(unsigned i=at+1;i<contacts.contact_count;i++)contacts.contacts[i-1]=contacts.contacts[i];
  contacts.contact_count--;
 }else if(e->phase==RISC_CANVAS_DOWN||e->phase==RISC_CANVAS_MOVE){
  if(at==contacts.contact_count){if(contacts.contact_count==RISC_TOUCH_MAX_CONTACTS)return false;contacts.contact_count++;}
  contacts.contacts[at]=(risc_touch_contact_v1){.id=(uint8_t)e->contact,.x=(uint16_t)e->x,.y=(uint16_t)e->y};
 }else return false;
 contacts.width=(uint16_t)width;contacts.height=(uint16_t)height;contacts.timestamp_ms=now();return true;
}
static bool recover_contacts(void){
 risc_canvas_event_v1 events[RISC_TOUCH_MAX_CONTACTS];
 for(unsigned i=0;i<RISC_TOUCH_MAX_CONTACTS;i++)events[i]=(risc_canvas_event_v1){.struct_size=sizeof(events[i])};
 size_t count=0;uint64_t cut=0;
 int32_t rc=canvas->contacts(canvas->context,session,events,RISC_TOUCH_MAX_CONTACTS,&count,&cut);
 if(!result_ok(rc))return false;
 contacts=(risc_touch_snapshot_v1){0};touch_head=touch_count=0;buttons=0;
 /* A gap cancels the gesture. Re-arm only at a neutral snapshot; never infer
  * a new tap or motion from a contact whose DOWN was lost. */
 last_sequence=cut;blocked=count!=0;return !blocked;
}
static void input_poll(void){
 if(blocked){(void)recover_contacts();if(blocked||retained)return;}
 for(unsigned i=0;i<64;i++){
  risc_canvas_event_v1 e={.struct_size=sizeof(e)};
  int32_t rc=canvas->next(canvas->context,session,&e);
  if(rc==RISC_CANVAS_IDLE||rc==RISC_CANVAS_AGAIN||rc==RISC_CANVAS_BUSY)break;
  if(!result_ok(rc)){if(!retained)exiting=true;break;}
  if(e.kind==RISC_CANVAS_INPUT_GAP){blocked=true;(void)recover_contacts();break;}
  if(e.kind==RISC_CANVAS_GEOMETRY_CHANGED){
   /* Preserve model ownership: end this fixed-layout invocation cleanly.
    * A subsequent launch obtains the new responsive extent. */
   exiting=true;break;
  }
  if(e.geometry_revision!=geometry.revision||!e.sequence||e.sequence<=last_sequence){blocked=true;break;}
  last_sequence=e.sequence;
  if(e.kind==RISC_CANVAS_CONTROLS_REQUEST){exiting=true;break;}
  if(e.kind==RISC_CANVAS_NAVIGATION){
   buttons=e.navigation.buttons|e.navigation.pressed;
   if(buttons&RISC_NAV_HOME)exiting=true;
   if(buttons&RISC_NAV_PAGE_BACK)buttons|=RISC_NAV_LEFT;
   if(buttons&RISC_NAV_PAGE_FORWARD)buttons|=RISC_NAV_RIGHT;
  }else if(e.kind==RISC_CANVAS_CONTACT){
   if(!adopt_contact(&e)){blocked=true;contacts.contact_count=0;touch_head=touch_count=0;break;}
   queue_contacts();if(blocked)break;
  }
 }
}
bool canvas_contacts(risc_touch_snapshot_v1 *out){
 if(!out)return false;
 if(blocked||retained){memset(out,0,sizeof(*out));return false;}
 if(touch_count){*out=touch_queue[touch_head];touch_head=(touch_head+1)%64;touch_count--;}
 else *out=contacts;
 return true;
}
static bool poll_nowait(t5_app_input_t *in){
 memset(in,0,sizeof(*in));if(!canvas_alive()){in->exit_requested=true;return false;}
 if(HAS(rt,risc_runtime_api_v1,exit_requested)&&rt->exit_requested())exiting=true;
 input_poll();in->buttons=buttons&63;in->exit_requested=exiting||(back_exits&&(buttons&RISC_NAV_BACK));
 pump();return !retained;
}
static bool poll_wait(t5_app_input_t *in,uint32_t ms){if(rt&&!retained)rt->yield_ms(ms?ms:1);return poll_nowait(in);}
static void set_back(bool b){back_exits=b;}
static void log_message(const char *s){if(rt&&!retained&&rt->diagnostic)(void)rt->diagnostic(s);}
static const t5_app_api_v1 app_api={.abi_version=1,.struct_size=sizeof(app_api),.poll=poll_wait,.millis=now,.set_back_exits_app=set_back,.psram_alloc=memory_alloc,.psram_free=memory_free,.poll_nowait=poll_nowait,.log_message=log_message};
const t5_app_api_v1 *t5_app_get_api(uint32_t v){return v==1?&app_api:NULL;}
const t5_provider_capability_api_v1 *t5_provider_capability_get_api(uint32_t v){static const t5_provider_capability_api_v1 a={1,sizeof(a),acquire,release,NULL};return v==1?&a:NULL;}
const t5_math_api_v1 *t5_math_get_api(uint32_t v){(void)v;return NULL;}
static bool video_start(t5_video_surface_v1 *s,uint8_t f){if(!started||f!=1)return false;*s=(t5_video_surface_v1){width,height,stride,1,1};return true;}
static bool video_default(t5_video_surface_v1 *s){return video_start(s,1);}
static uint8_t *backbuffer(size_t *size){if(size)*size=started?(size_t)stride*height:0;return retained?NULL:working;}
static bool can_submit(void){return started&&!retained&&!exiting;}
static bool submit(uint16_t y,uint16_t h){(void)y;(void)h;if(!can_submit())return false;memcpy(snapshot,working,(size_t)stride*height);dirty=true;pump();return !retained;}
static bool pending(void){if(!retained)pump();return receipt||dirty;}
static uint32_t frame_counter(void){return frames;}
static void video_stop(void){}
const t5_video_api_v1 *t5_video_get_api(uint32_t v){static const t5_video_api_v1 a={.api_version=1,.struct_size=sizeof(a),.start=video_default,.backbuffer=backbuffer,.can_submit=can_submit,.submit=submit,.pending=pending,.frame_counter=frame_counter,.stop=video_stop,.start_format=video_start};return v==1?&a:NULL;}
static const char cube[]="v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\nf 1 4 3 2\nf 5 6 7 8\nf 1 2 6 5\nf 4 8 7 3\nf 1 5 8 4\nf 2 3 7 6\n";
static const char *model_names[]={"CUBE","SAVED OBJ","SAVED STL"};
unsigned canvas_model_count(void){return 3;}
const char *canvas_model_name(unsigned i){return i<3?model_names[i]:"";}
void canvas_model_select(unsigned i){selected=i<3?i:0;selected_path[0]=0;source_selected=false;}
bool canvas_model_source(char *out,size_t cap){
 if(selected_path[0])snprintf(out,cap,"%s",selected_path);
 else snprintf(out,cap,"%s",selected==0?"builtin.obj":selected==1?"model.obj":"model.stl");
 return true;
}
static bool source_path_get(char *out,size_t cap){return canvas_model_source(out,cap);}
const t5_file_open_api_v1 *t5_file_open_get_api(uint32_t v){static const t5_file_open_api_v1 a={.api_version=1,.struct_size=sizeof(a),.source_path_get=source_path_get};return v==1?&a:NULL;}
static void stream_close(uint32_t h){
 if(!h||retained)return;
 source_stream=false;
 if(stream&&!volume->file_close(volume->context,stream,false)){canvas_retain("Canvas: model file close retained");return;}stream=0;
 free(model_bytes);model_bytes=NULL;model_size=model_offset=0;
}
static uint32_t stream_open(const char *path,size_t *size){
 *size=0;stream_close(1);if(retained)return 0;stream_size=stream_offset=0;
 if(source_selected&&!strcmp(path,selected_path)){
  if(!source_files||!selected_source.token||selected_source.size>SIZE_MAX)return 0;
  source_stream=true;stream_size=selected_source.size;*size=(size_t)stream_size;return 1;
 }
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
 if(source_stream){
  if(n>RISC_FILE_OPEN_SOURCE_IO_MAX)n=RISC_FILE_OPEN_SOURCE_IO_MAX;
  uint32_t actual=0;int32_t rc;
  do{
   t5_app_input_t input={0};rt->yield_ms(1);
   if(!poll_nowait(&input)||input.exit_requested||(input.buttons&T5_APP_BUTTON_BACK)){exiting=true;return 0;}
   rc=source_files->read_at(selected_source.token,stream_offset,out,(uint32_t)n,&actual);
  }while(rc==RISC_FILE_SOURCE_BUSY);
  if(rc==RISC_FILE_SOURCE_RETAINED){canvas_retain("Viewer: selected source read retained");return 0;}
  if(rc||actual>n||actual>stream_size-stream_offset){exiting=true;return 0;}
  stream_offset+=actual;return actual;
 }
 if(model_bytes){if(n>model_size-model_offset)n=model_size-model_offset;memcpy(out,model_bytes+model_offset,n);model_offset+=(unsigned)n;return n;}
 if(!stream)return 0;
 if(n>RISC_STORAGE_VOLUME_IO_MAX)n=RISC_STORAGE_VOLUME_IO_MAX;
 size_t got=volume->file_read(volume->context,stream,out,n);if(got>n){exiting=true;return 0;}stream_offset+=got;return got;
}
static bool stream_seek(uint32_t h,size_t offset){
 if(!h||retained)return false;
 if(source_stream){if(offset>stream_size||exiting)return false;stream_offset=offset;return true;}
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
 rt=risc_runtime_get_api(1);
 if(app_kind!=CANVAS_VIEWER||!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release||!rt->health||!rt->yield_ms)return false;
 retained=exiting=dirty=started=false;blocked=true;back_exits=true;
 session=receipt=last_sequence=0;frames=buttons=clock_last=touch_head=touch_count=0;
 contacts=(risc_touch_snapshot_v1){0};memset(grants,0,sizeof(grants));
 canvas=get(RISC_CANVAS_CAPABILITY,1);
 if(!canvas||!risc_canvas_get_v1(canvas,canvas->struct_size)){log_message("3D Viewer: canvas unavailable");goto bad;}
 risc_canvas_open_v1 request={sizeof(request),RISC_CANVAS_MONO1,RISC_CANVAS_RESPONSIVE,0,0};
 if(!result_ok(canvas->open(canvas->context,&request,&session))){log_message("3D Viewer: presentation unavailable");goto bad;}
 geometry=(risc_canvas_geometry_v1){.struct_size=sizeof(geometry)};
 if(!result_ok(canvas->geometry(canvas->context,session,&geometry)))goto bad;
 if(geometry.width<160||geometry.height<160||geometry.width>1200||geometry.height>1200){log_message("3D Viewer: logical extent unsupported");goto bad;}
 width=geometry.width;height=geometry.height;stride=(width+7)/8;
 working=calloc(stride,height);snapshot=calloc(stride,height);if(!working||!snapshot)goto bad;
 volume=get("storage.volume",1);data=get("storage.app-data",1);files=get("file.open",1);
 if(volume&&(volume->api_version!=1||!HAS(volume,risc_storage_volume_api_v1,file_close)||!volume->file_open_read||!volume->file_read||!volume->refresh||!volume->ready))volume=NULL;
 if(data&&(data->api_version!=1||!HAS(data,risc_app_data_v1,read)||!data->stat))data=NULL;
 if(files&&(files->api_version!=1||!HAS(files,t5_file_open_api_v1,source_path_get)))files=NULL;
 selected=0;selected_path[0]=0;
 if(files)(void)files->source_path_get(selected_path,sizeof(selected_path));
 selected_path[sizeof(selected_path)-1]=0;
 source_files=get(RISC_FILE_OPEN_SOURCE_CAPABILITY,1);
 source_selected=source_stream=false;
 selected_source=(risc_file_open_source_info_v1){.struct_size=sizeof(selected_source)};
 if(source_files){
  if(source_files->api_version!=1||!HAS(source_files,risc_file_open_source_api_v1,read_at)||!source_files->source_info)source_files=NULL;
  else{
   int32_t rc=source_files->source_info(&selected_source);
   if(rc==RISC_FILE_SOURCE_RETAINED){canvas_retain("Viewer: selected source admission retained");return false;}
   if(rc==RISC_FILE_SOURCE_OK){
    if(!selected_source.token||!memchr(selected_source.name,0,sizeof(selected_source.name))||
       !selected_source.name[0]||strchr(selected_source.name,'/')||strchr(selected_source.name,'\\')||
       selected_source.size>SIZE_MAX){log_message("3D Viewer: selected source invalid");goto bad;}
    snprintf(selected_path,sizeof(selected_path),"source:%s",selected_source.name);source_selected=true;
   }else if(rc!=RISC_FILE_SOURCE_CONTEXT&&rc!=RISC_FILE_SOURCE_UNAVAILABLE){
    log_message("3D Viewer: selected source unavailable");goto bad;
   }
  }
 }
 started=true;return true;
 bad:canvas_close();return false;
}
void canvas_close(void){
 if(retained)return;
 dirty=false;uint32_t at=now();
 while(started&&pending()&&!retained){if((uint32_t)(now()-at)>5100){canvas_retain("Viewer: close presentation timeout");break;}rt->yield_ms(1);}
 if(retained)return;
 stream_close(1);if(retained)return;
 if(session){
  int32_t rc=canvas->close(canvas->context,session);
  if(!result_ok(rc)){if(!retained)canvas_retain("Viewer: canvas close uncertain");return;}
  session=0;
 }
 for(unsigned i=MAX_GRANTS;i>0;i--)if(grants[i-1].api&&!release(i))return;
 free(working);free(snapshot);working=snapshot=NULL;started=false;
}
